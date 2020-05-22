/*
 This file is part of ay-d19m.

 ay-d19m project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 any later version.

 ay-d19m project is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with ay-d19m project.  If not, see <http://www.gnu.org/licenses/>.

 AUTHOR "Jürgen Willi Sievers <JSievers@NadiSoft.de>";
 Mi 20. Mai 01:23:43 CEST 2020 Version 1.0.0 untested

 */
#include "ay-d19m.h"
#include "decoder.c"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/cdev.h>
#include <asm/uaccess.h>	/* copy_*_user */
#include <linux/gpio.h>		// Required for the GPIO functions
#include <linux/interrupt.h>	// Required for the IRQ code
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jürgen Willi Sievers <JSievers@NadiSoft.de>");
MODULE_DESCRIPTION("AY_D19M KeyPad Driver.");
MODULE_VERSION("0.1");

static unsigned ay_d19m_power = AY_D19M_POWER;
static unsigned ay_d19m_d0 = AY_D19M_D0;
static unsigned ay_d19m_d1 = AY_D19M_D1;
unsigned ay_d19m_mode = K6W26BCD;

module_param(ay_d19m_power, uint, 0644);
MODULE_PARM_DESC(ay_d19m_power, CLASS_NAME " Power GPOI Port. Default GPIO18");
module_param(ay_d19m_d0, uint, 0644);
MODULE_PARM_DESC(ay_d19m_d0, CLASS_NAME " DATA0 GPOI Port. Default GPIO4");
module_param(ay_d19m_d1, uint, 0644);
MODULE_PARM_DESC(ay_d19m_d1, CLASS_NAME " DATA1 GPOI Port. Defaul GPIO26");
module_param(ay_d19m_mode, uint, 0644);
MODULE_PARM_DESC(ay_d19m_mode, CLASS_NAME " Keypad Transmission (0..7) Format. Default 0");

int ayd19m_major = 0;
int ayd19m_minor = 0;

struct timer_list wiegand_timeout;

static const int wiegandLength[] = { 6 - 1, 6 - 1, 8 - 1, 26 - 1, 26 - 1, 26 - 1, 0, 0 };
static const uint32_t wiegandMask = 0x80000000;
static volatile uint32_t bitmsk;
static volatile uint32_t data0;
static volatile uint32_t data1;

DEFINE_MUTEX(rmutex);
static int irqlineD0 = 0;
static int irqlineD1 = 0;

static int isOpen = 0;

struct list_head todo_list;
wait_queue_head_t rqueue;

struct todo_data
{
	struct list_head node;
	char data[MAX_READSZ + 1];
};

static irqreturn_t ay_d19m_irqdata(int irq, void *dev);
static void wiegand_timeoutfunc(struct timer_list *timer);
static int powerOn(void);
static int powerOff(void);
static int acquiresGPIO(void);
static int releaseGPIO(void);


static struct class* ay_d19m_Class = NULL; ///< The device-driver class struct pointer
static struct device* ay_d19m_Device = NULL; ///< The device-driver device struct pointer



static const fmt ffmt[] = {
		fmt_SKW06RF,
		fmt_SKW06NP,
		fmt_SKW08NC,
		fmt_K4W26BF,
		fmt_K5W26FC,
		fmt_K6W26BCD,
		fmt_SK3X4MX,
		fmt_K8CDBCD
};

/*
 * Data management: read and write.
 */
ssize_t ayd19m_read(struct file *filp, char __user * buf, size_t count, loff_t * f_pos)
{
	ssize_t retval;
	struct todo_data *data;
	struct list_head *todo = (struct list_head *) filp->private_data;
	unsigned n;

	printk(KERN_DEBUG CLASS_NAME ": read pbuf=%p, cnt=%d, off=%lld\n", buf, count, *f_pos);

	if (!(filp->f_flags & O_NONBLOCK))
	{
		retval = wait_event_interruptible(rqueue, todo->next != todo);
		if (retval) return retval;
	}

	retval = mutex_lock_interruptible(&rmutex);
	if (retval) return retval;
	if (todo->next != todo)
	{
		data = list_entry(todo->next,struct todo_data, node);
		n = strlen(data->data) + 1;
		if (*f_pos < n)
		{
			n -= *f_pos;
			if (count < n) n = count;

			n -= copy_to_user(buf, data->data + *f_pos, n);
			*f_pos = n;

			if (n > strlen(data->data))
			{
				*f_pos = 0;
				list_del(&data->node);
				kfree(data);
			}
			retval = n;
		}
		else
		{
			*f_pos -= n;
			list_del(&data->node);
			kfree(data);
			retval = 0;
		}
	}
	mutex_unlock(&rmutex);

	return retval;
}

int ayd19m_release(struct inode *inode, struct file *filp)
{
	int retval = mutex_lock_interruptible(&rmutex);
	if (!retval)
	{
		if (!isOpen)
		{
			retval = -EBADF;
			printk(KERN_DEBUG CLASS_NAME ": bad close.\n");
		}
		else if (!(O_NONBLOCK & filp->f_flags))
		{
			struct todo_data *d;
			struct list_head *p, *n;
			powerOff();
			list_for_each_safe(p, n, todo_list.next)
			{
				d = list_entry(p, struct todo_data, node);
				list_del(&d->node);
				kfree(d);
			}
			printk(KERN_DEBUG CLASS_NAME ": close.\n");
		}
		isOpen = 0;

		mutex_unlock(&rmutex);
	}
	return retval;
}

/*
 * Open and close
 */
int ayd19m_open(struct inode *inode, struct file *filp)
{
	int retval = -EACCES;
	if (!(filp->f_flags & (O_WRONLY | O_RDWR)))
	{
		retval = mutex_lock_interruptible(&rmutex);
		if (!retval)
		{
			if (!isOpen)
			{
				isOpen = 1;
				filp->private_data = &todo_list;
				powerOn();
				printk(KERN_DEBUG CLASS_NAME ": open.\n");
			}
			else
			{
				retval = -EBUSY;
				printk(KERN_DEBUG CLASS_NAME ": busy.\n");
			}
			bitmsk = wiegandMask;
			mutex_unlock(&rmutex);
		}
	}
	return retval;
}

struct file_operations ayd19m_fops = { .owner = THIS_MODULE,
//  .llseek = ayd19m_llseek,
        .read = ayd19m_read,
//  .write = ayd19m_write,
// .unlocked_ioctl = ayd19m_ioctl,
        .open = ayd19m_open, .release = ayd19m_release, };

void ayd19m_cleanup_module(void)
{
	struct todo_data *d;
	struct list_head *p, *n;

	del_timer(&wiegand_timeout);

	releaseGPIO();

	list_for_each_safe(p, n, todo_list.next)
	{
		d = list_entry(p, struct todo_data, node);
		list_del(&d->node);
		kfree(d);
	}

	device_destroy(ay_d19m_Class, MKDEV(ayd19m_major, 0));	// remove the device
	class_unregister(ay_d19m_Class);                        // unregister the device class
	class_destroy(ay_d19m_Class);                           // remove the device class 9rS8s5M2x9nCxjK
	unregister_chrdev(ayd19m_major, DEVICE_NAME);           // unregister the major number
	printk(KERN_INFO CLASS_NAME ": cleanup success\n");
}

int ayd19m_init_module(void)
{
	int result;

	printk(KERN_INFO CLASS_NAME ": Initializing mode %d on %d HZ System...\n", ay_d19m_mode, HZ);
	mutex_init(&rmutex);
	INIT_LIST_HEAD(&todo_list);
	init_waitqueue_head(&rqueue);
	timer_setup(&wiegand_timeout, wiegand_timeoutfunc, 0);

	result = acquiresGPIO();
	if (!result)
	{
		// Try to dynamically allocate a major number for the device -- more difficult but worth it
		result = ayd19m_major = register_chrdev(0, DEVICE_NAME, &ayd19m_fops);
		if (0 <= result)
		{
			result = 0;
			printk(KERN_INFO CLASS_NAME ": registered correctly with major number %d\n", ayd19m_major);
			// Register the device class
			ay_d19m_Class = class_create(THIS_MODULE, CLASS_NAME);
			if (IS_ERR(ay_d19m_Class))             // Check for error and clean up if there is
			{
				unregister_chrdev(ayd19m_major, DEVICE_NAME);
				printk(KERN_ERR CLASS_NAME ":Failed to register device class\n");
				result = PTR_ERR(ay_d19m_Class);          // Correct way to return an error on a pointer
			}
			else
			{
				printk(KERN_INFO CLASS_NAME ": device class registered correctly\n");

				// Register the device driver
				ay_d19m_Device = device_create(ay_d19m_Class, NULL, MKDEV(ayd19m_major, 0), NULL, DEVICE_NAME);
				if (IS_ERR(ay_d19m_Device))               // Clean up if there is an error
				{
					class_destroy(ay_d19m_Class);           // Repeated code but the alternative is goto statements
					unregister_chrdev(ayd19m_major, DEVICE_NAME);
					printk(KERN_ERR CLASS_NAME ":Failed to create the device\n");
					result = PTR_ERR(ay_d19m_Device);
				}
				else printk(KERN_INFO CLASS_NAME ": device created correctly\n"); // Made it! device was initialized
			}
		}
		else
			printk(KERN_ERR CLASS_NAME " failed to register a major number\n");
	}
	if (result) releaseGPIO();
	return result;
}

static irqreturn_t ay_d19m_irqdata(int irq, void *dev)
{
	if (bitmsk == wiegandMask)
	{
		data1 = gpio_get_value(ay_d19m_d1) ? bitmsk : 0;
		data0 = gpio_get_value(ay_d19m_d0) ? bitmsk : 0;

		mod_timer(&wiegand_timeout, jiffies + HZ / 25); // 40ms
	}
	else
	{
		data1 |= gpio_get_value(ay_d19m_d1) ? bitmsk : 0;
		data0 |= gpio_get_value(ay_d19m_d0) ? bitmsk : 0;
	}
	bitmsk >>= 1;
	return IRQ_HANDLED;
}

static void wiegand_timeoutfunc(struct timer_list *timer)
{
	int s, n = 32;
	while(bitmsk)
	{
		n--;
		bitmsk >>= 1;
	}
	data0 >>= (32-n);
	data1 >>= (32-n);

	printk(KERN_DEBUG CLASS_NAME ": wiegand mode %d, D0 %8.8X, D1 %8.8X, D0 xor D1 %8.8X, bits %d\n", ay_d19m_mode, data0, data1,
	        data0 ^ data1, n);

	if((data0 ^ data1) == ~(-1 << n))
	{
		char rbuffer[MAX_READSZ];

		if(n == wiegandLength[ay_d19m_mode]+1)
			s = ffmt[ay_d19m_mode](data0, n, rbuffer, sizeof(rbuffer) - 2);
		else if(n == 26)
			s = wiegand26(data0, rbuffer, sizeof(rbuffer) - 2);
		else
			s = snprintf(rbuffer, sizeof(rbuffer) - 2, "R=%d, M=%d, D=%8.8X, L=%d", RES_NOSUPORT, ay_d19m_mode, data0, n);

		if (s)
		{
			struct todo_data *data = kmalloc(sizeof(struct todo_data), GFP_KERNEL);
			printk(KERN_INFO CLASS_NAME ": new key on mode %d, code %s\n", ay_d19m_mode, rbuffer);

			rbuffer[s++] = '\n';
			rbuffer[s++] = '\0';

			if (data)
			{
				mutex_lock(&rmutex);
				del_timer(&wiegand_timeout);
				strncpy(data->data, rbuffer, s);
				list_add_tail(&data->node, &todo_list);
				mutex_unlock(&rmutex);

				wake_up(&rqueue);
			}
		}
	}
	else
		printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %8.8X xor D1 %8.8X = %8.8X expected %8.8X\n", ay_d19m_mode,
				        data0, data1, data0 ^ data1, ~(-1 << n));

	data0 = data1 = 0;
	bitmsk = wiegandMask;

}

static int acquiresGPIO(void)
{
	int res = -ENODEV;

	if (!gpio_is_valid(ay_d19m_d0))
	{
		printk(KERN_INFO CLASS_NAME ": invalid D0 GPIO\n");
		return res;
	}
	if (!gpio_is_valid(ay_d19m_d1))
	{
		printk(KERN_INFO CLASS_NAME ": invalid D1 GPIO\n");
		return res;
	}
	if (!gpio_is_valid(ay_d19m_power))
	{
		printk(KERN_INFO CLASS_NAME ": invalid Power GPIO\n");
		return res;
	}

	if (!gpio_request_one(ay_d19m_power, GPIOF_OUT_INIT_LOW | GPIOF_EXPORT_DIR_FIXED, "av-d19m.power")
	        && !gpio_request_one(ay_d19m_d0, GPIOF_IN | GPIOF_EXPORT_DIR_FIXED, "av-d19m.d0")
	        && !gpio_request_one(ay_d19m_d1, GPIOF_IN | GPIOF_EXPORT_DIR_FIXED, "av-d19m.d1"))
	{
		if ( 0 <= (res = irqlineD0 = gpio_to_irq(ay_d19m_d0)) && 0 <= (res = irqlineD1 = gpio_to_irq(ay_d19m_d1)))
		{
			printk(KERN_INFO CLASS_NAME ": The D0/D1 mapped to IRQ: %d/%d\n", irqlineD0, irqlineD1);
			res = request_irq(irqlineD0,	// The interrupt number requested
			        ay_d19m_irqdata,	// The pointer to the handler function below
			        IRQF_TRIGGER_FALLING,	// Interrupt on rising edge (button press, not release)
			        "ay_d19m D0 gpio_handler",	// Used in /proc/interrupts to identify the owner
			        0);	// The *dev_id for shared interrupt lines, NULL is okay
			printk(KERN_INFO CLASS_NAME ": The D0 interrupt request result is: %d\n", res);

			res = request_irq(irqlineD1,	// The interrupt number requested
			        ay_d19m_irqdata,	// The pointer to the handler function below
			        IRQF_TRIGGER_FALLING,	// Interrupt on rising edge (button press, not release)
			        "ay_d19m D1 gpio_handler",	// Used in /proc/interrupts to identify the owner
			        0);	// The *dev_id for shared interrupt lines, NULL is okay
			printk(KERN_INFO CLASS_NAME ": The D1 interrupt request result is: %d\n", res);

		}
		else printk(KERN_ERR CLASS_NAME ": Can not set irq on GPIO (Wiegand D0/D1) lines.\n");
	}
	else printk(KERN_ERR CLASS_NAME ": Can not requst GPIO (Wiegand Power/D0/D1) lines.\n");
	printk(KERN_INFO CLASS_NAME ": Init result: %d\n", res);
	return res;
}

static int releaseGPIO(void)
{
	powerOff();        						// Turn the Power off.
	if (irqlineD0) free_irq(irqlineD0, 0);  // Free the IRQ number for D0 line
	if (irqlineD1) free_irq(irqlineD1, 0);  // Free the IRQ number for D1 line
	irqlineD0 = irqlineD1 = 0;

	gpio_free(ay_d19m_power);   // Free the Power GPIO
	gpio_free(ay_d19m_d0);      // Free the D0 GPIO
	gpio_free(ay_d19m_d1);      // Free the D1 GPIO
	return 0;
}

static int powerOn(void)
{
	printk(KERN_DEBUG CLASS_NAME ": Power on\n");

	// switch power on
	gpio_set_value(ay_d19m_power, 1);
	msleep(500);
	return gpio_get_value(ay_d19m_power);
}

static int powerOff(void)
{
	printk(KERN_DEBUG CLASS_NAME ": Power off\n");

	// switch power on
	gpio_set_value(ay_d19m_power, 0);
	msleep(10);
	return gpio_get_value(ay_d19m_power);
}

module_init(ayd19m_init_module);
module_exit(ayd19m_cleanup_module);
