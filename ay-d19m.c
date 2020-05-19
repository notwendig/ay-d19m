/*
	 This file is part of ay-d19m.

    ay-d19m project is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

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


MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Jürgen Willi Sievers <JSievers@NadiSoft.de>");
MODULE_DESCRIPTION ("AY_D19M KeyPad Driver.");
MODULE_VERSION ("0.1");


static unsigned ay_d19m_power = AY_D19M_POWER;
static unsigned ay_d19m_d0 = AY_D19M_D0;
static unsigned ay_d19m_d1 = AY_D19M_D1;
static unsigned ay_d19m_mode = SKW06RF;


module_param (ay_d19m_power, uint, 0644);
MODULE_PARM_DESC (ay_d19m_power, CLASS_NAME " Power GPOI Port. Default GPIO18");
module_param (ay_d19m_d0, uint, 0644);
MODULE_PARM_DESC (ay_d19m_d0, CLASS_NAME " DATA0 GPOI Port. Default GPIO4");
module_param (ay_d19m_d1, uint, 0644);
MODULE_PARM_DESC (ay_d19m_d1, CLASS_NAME " DATA1 GPOI Port. Defaul GPIO26");
module_param (ay_d19m_mode, uint, 0644);
MODULE_PARM_DESC (ay_d19m_mode, CLASS_NAME " Keypad Transmission (0..) Format. Default 2");

int ayd19m_major = 0;
int ayd19m_minor = 0;

struct timer_list wiegand_timeout;

static const int wiegandLength[] = {6-1, 6-1, 8-1, 26-1, 26-1, 26-1, 0, 0};
uint32_t wiegandMask;
static volatile int bitmsk;
static volatile uint32_t data0;
static volatile uint32_t data1;

DEFINE_MUTEX(rmutex);
static int irqlineD0;
static int irqlineD1;

static int isOpen = 0;


struct list_head todo_list;
wait_queue_head_t rqueue;

struct todo_data {
    struct list_head node;
    char data[MAX_READSZ+1];
};

static irqreturn_t ay_d19m_irqdata (int irq, void *dev);
static irqreturn_t ay_d19m_thread (int irq, void *dev);
static void wiegand_timeoutfunc (struct timer_list *timer);

static struct class*  ay_d19m_Class  = NULL; ///< The device-driver class struct pointer
static struct device* ay_d19m_Device = NULL; ///< The device-driver device struct pointer

static int fmt_SKW06RF(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)    // "n" n=[0-9*#] 			Single Key, Wiegand 6-Bit (Rosslare Format). Factory setting
{
    int i;
    unsigned ep, op, cep, cop, dep, dop;
    uint16_t code;

    if((code0 ^ code1) == 0x3F)
    {
        ep = code0 & wiegandMask ? 1 : 0;
        op = code0 & 1;

        code = (code0 >> 1) & 0xF;
        cep = 0;
        cop = 1;
        dep = 0x7 & (code0 >> 3);
        dop = 0x7 & (code0 >> 1);

        for (i = 0; i < 2; i++)
        {
            cep ^= dep & 1;
            cop ^= dop & 1;
            dep >>= 1;
            dop >>= 1;
        }
        if (ep == cep && op == cop)
        {
            char key;
            if(code == 10) key = '0';
            else if(code == 11) key = '*';
            else if(code == 14) key = '#';
            else key = '0' + code;
            snprintf(buffer, bsz, "M=%d, K=%c", ay_d19m_mode, key);
        }
        else
        {
            printk(KERN_WARNING CLASS_NAME ": Mode %d, parity error! D0 %7.7X xor D1 %7.7X = %7.7X expected 000003F\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
           *buffer = 0;
        }
    }
    else
    {
        printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %7.7X xor D1 %7.7X = %7.7X expected 3FFFFFF\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
        *buffer = 0;
    }

    return strlen(buffer);
}

static int fmt_SKW06NP(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)    // "n" n=[0-9*#] 			Single Key, Wiegand 6-Bit with Nibble + Parity Bits
{
    int i;
    unsigned ep, op, cep, cop, dep, dop;
    uint16_t code;

    if((code0 ^ code1) == 0x3F)
    {
        ep = code0 & wiegandMask ? 1 : 0;
        op = code0 & 1;

        code = (code0 >> 1) & 0xF;
        cep = 0;
        cop = 1;
        dep = 0x7 & (code0 >> 3);
        dop = 0x7 & (code0 >> 1);

        for (i = 0; i < 2; i++)
        {
            cep ^= dep & 1;
            cop ^= dop & 1;
            dep >>= 1;
            dop >>= 1;
        }
        if (ep == cep && op == cop)
        {
            char key;
            if(code == 10) key = '*';
            else if(code == 11) key = '#';
            else key = '0' + code;
            snprintf(buffer, bsz, "M=%d, K=%c", ay_d19m_mode, key);
        }
        else
        {
            printk(KERN_WARNING CLASS_NAME ": Mode %d, parity error! D0 %7.7X xor D1 %7.7X = %7.7X expected 000003F\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
           *buffer = 0;
        }
    }
    else
    {
        printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %7.7X xor D1 %7.7X = %7.7X expected 3FFFFFF\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
        *buffer = 0;
    }

    return strlen(buffer);
}

static int fmt_SKW08NC(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)    // "n" n=[0-9*#] 			Single Key, Wiegand 8-Bit, Nibbles Complemented
{
    uint16_t code = code0 & 0xF;
    uint16_t icode= (code0 >> 4) & 0xF;

    if((code0 ^ code1) == 0xFF && (code ^ icode) == 0xF)
    {
        char key;
        if(code == 10) key = '*';
        else if(code == 11) key = '#';
        else key = '0' + code;
        snprintf(buffer, bsz, "M=%d, K=%c", ay_d19m_mode, key);
    }
    else
    {
        printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %7.7X xor D1 %7.7X = %7.7X expected 3FFFFFF\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
        *buffer = 0;
    }

    return strlen(buffer);}

static int fmt_K4W26BF(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)    // "f,n" f=Facility,n=code 	4 Keys Binary + Facility code, Wiegand 26-Bit
{
    int i;
    unsigned ep, op, cep, cop, dep, dop,facility;
    uint16_t code;

    if((code0 ^ code1) == 0x3FFFFFF)
    {
        ep = code0 & wiegandMask ? 1 : 0;
        op = code0 & 1;

        code = (code0 >> 1) & 0xFFFF;
        facility = (code0 >> 17) & 0xFF;
        cep = 0;
        cop = 1;
        dep = 0xFFF & (code0 >> 13);
        dop = 0xFFF & (code0 >> 1);

        for (i = 0; i < 12; i++)
        {
            cep ^= dep & 1;
            cop ^= dop & 1;
            dep >>= 1;
            dop >>= 1;
        }
        if (ep == cep && op == cop)
        {
            snprintf(buffer, bsz, "M=%d, F=%d, C=%d\n", ay_d19m_mode, facility, code);
        }
        else
            *buffer = 0;
    }
    else
    {
        printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %7.7X xor D1 %7.7X = %7.7X expected 3FFFFFF\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
        *buffer = 0;
    }

    return strlen(buffer);
}

static int fmt_K5W26FC(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)    // "f,n" f=Facility,n=code 	1 to 5 Keys + Facility code, Wiegand 26-Bit
{
    int i;
    unsigned ep, op, cep, cop, dep, dop,facility;
    uint16_t code;

    if((code0 ^ code1) == 0x3FFFFFF)
    {
        ep = code0 & wiegandMask ? 1 : 0;
        op = code0 & 1;

        code = (code0 >> 1) & 0xFFFF;
        facility = (code0 >> 17) & 0xFF;
        cep = 0;
        cop = 1;
        dep = 0xFFF & (code0 >> 13);
        dop = 0xFFF & (code0 >> 1);

        for (i = 0; i < 12; i++)
        {
            cep ^= dep & 1;
            cop ^= dop & 1;
            dep >>= 1;
            dop >>= 1;
        }
        if (ep == cep && op == cop)
        {
            snprintf(buffer, bsz, "M=%d, F=%d, C=%d\n", ay_d19m_mode, facility, code);
        }
        else
            *buffer = 0;
    }
    else
    {
        printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %7.7X xor D1 %7.7X = %7.7X expected 3FFFFFF\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
        *buffer = 0;
    }

    return strlen(buffer);
}

static int fmt_K6W26BCD(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)   // "n" n=code 				6 Keys BCD and Parity Bits, Wiegand 26-Bit
{
    int i;
    unsigned ep, op, cep, cop, dep, dop;
    uint32_t code;

    if((code0 ^ code1) == 0x3FFFFFF)
    {
        ep = code0 & wiegandMask ? 1 : 0;
        op = code0 & 1;

        code = (code0 >> 1) & 0xFFFFFF;
        cep = 0;
        cop = 1;
        dep = 0xFFF & (code >> 12);
        dop = 0xFFF & code;

        for (i = 0; i < 12; i++)
        {
            cep ^= dep & 1;
            cop ^= dop & 1;
            dep >>= 1;
            dop >>= 1;
        }
        if (ep == cep && op == cop)
        {
            snprintf(buffer, bsz, "M=%d,C=%6.6x\n", ay_d19m_mode, code);
        }
        else
            *buffer = 0;
    }
    else
    {
        printk(KERN_WARNING CLASS_NAME ": Mode %d, bit-error! D0 %7.7X xor D1 %7.7X = %7.7X expected 3FFFFFF\n",
                ay_d19m_mode, code0, code1, code0 ^ code1);
        *buffer = 0;
    }

    return strlen(buffer);
}

static int fmt_SK3X4MX(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)    // not supported yet 		Single Key, 3x4 Matrix Keypad
{
	snprintf(buffer, bsz,"Error: Unsupported mode %d",ay_d19m_mode);
	return strlen(buffer);
}

static int fmt_K8CDBCD(uint32_t code0, uint32_t code1, char *buffer, size_t bsz)     // not supported yet 		1 to 8 Keys BCD, Clock & Data Single Key
{
	snprintf(buffer, bsz,"Error: Unsupported mode %d",ay_d19m_mode);
	return strlen(buffer);
}

typedef int (*fmt)(uint32_t, uint32_t, char *, size_t);

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
ssize_t
ayd19m_read (struct file *filp, char __user * buf, size_t count, loff_t * f_pos)
{
    ssize_t retval;
    struct todo_data *data;
    struct list_head *todo = (struct list_head *) filp->private_data;
    unsigned n;

    printk(KERN_DEBUG CLASS_NAME ": read pbuf=%p, cnt=%d, off=%lld\n", buf, count, *f_pos);

    if(!(filp->f_flags & O_NONBLOCK))
    {
    	retval = wait_event_interruptible(rqueue, todo->next != todo);
    	if(retval) return retval;
    }

    retval = mutex_lock_interruptible(&rmutex);
    if(retval) return retval;
    if(todo->next != todo)
    {
		data = list_entry(todo->next,struct todo_data, node);
		n = strlen(data->data)+1;
		if(*f_pos < n)
		{
			n -= *f_pos;
			if(count < n) n = count;

			n -= copy_to_user(buf, data->data + *f_pos, n);
			*f_pos = n;

			if(n > strlen(data->data))
			{
				*f_pos = 0;
				list_del(&data->node);
				kfree(data);
			}
			retval = n;
		}
		else
		{
			//todo: check *f_pos -= n;
			list_del(&data->node);
			kfree(data);
			retval = 0;
		}
    }
    mutex_unlock(&rmutex);

    return retval;
}

int ayd19m_release (struct inode *inode, struct file *filp)
{
    int retval = mutex_lock_interruptible(&rmutex);
    if(!retval)
    {
        if(!isOpen)
        {
            retval = -EBADF;
            printk (KERN_DEBUG CLASS_NAME ": bad close.\n");
        }
        else if(!(O_NONBLOCK & filp->f_flags))
        {
        	struct todo_data *d;
        	struct list_head *p,*n;
        	gpio_set_value (ay_d19m_power, 0);
        	list_for_each_safe(p, n, todo_list.next)
        	{
        		d = list_entry(p, struct todo_data, node);
        		list_del(&d->node);
        		kfree(d);
        	}
            printk (KERN_DEBUG CLASS_NAME ": close.\n");
        }
        isOpen = 0;

        mutex_unlock(&rmutex);
    }
    return retval;
}

/*
* Open and close
*/
int ayd19m_open (struct inode *inode, struct file *filp)
{
	int retval = -EACCES;
	if(!(filp->f_flags & (O_WRONLY | O_RDWR)))
	{
		retval = mutex_lock_interruptible(&rmutex);
		if(!retval)
		{
			if(!isOpen)
			{
				isOpen = 1;
				filp->private_data = &todo_list;
				mod_timer(&wiegand_timeout, HZ/4);
				printk (KERN_DEBUG CLASS_NAME ": open.\n");
			}
			else
			{
				retval = -EBUSY;
				printk (KERN_DEBUG CLASS_NAME ": busy.\n");
			}

			mutex_unlock(&rmutex);
		}
	}
    return retval;
}

struct file_operations ayd19m_fops = {
  .owner = THIS_MODULE,
//  .llseek = ayd19m_llseek,
  .read = ayd19m_read,
//  .write = ayd19m_write,
// .unlocked_ioctl = ayd19m_ioctl,
  .open = ayd19m_open,
  .release = ayd19m_release,
};

void ayd19m_cleanup_module (void)
{
	struct todo_data *d;
	struct list_head *p,*n;

	del_timer(&wiegand_timeout);
	free_irq(irqlineD0, 0);     // Free the IRQ number, no *dev_id required in this case
	free_irq(irqlineD1, 0);     // Free the IRQ number, no *dev_id required in this case

	gpio_set_value(ay_d19m_power, 0);        // Turn the Power off, makes it clear the device was unloaded

	gpio_free(ay_d19m_power);                // Free the Power GPIO
	gpio_free(ay_d19m_d0);                   // Free the D0 GPIO
	gpio_free(ay_d19m_d1);                   // Free the D1 GPIO

	list_for_each_safe(p, n, todo_list.next)
	{
		d = list_entry(p, struct todo_data, node);
		list_del(&d->node);
		kfree(d);
	}

   device_destroy(ay_d19m_Class, MKDEV(ayd19m_major, 0));     // remove the device
   class_unregister(ay_d19m_Class);                          // unregister the device class
   class_destroy(ay_d19m_Class);                             // remove the device class 9rS8s5M2x9nCxjK
   unregister_chrdev(ayd19m_major, DEVICE_NAME);             // unregister the major number
   printk (KERN_INFO CLASS_NAME ": cleanup success\n");
}

int
ayd19m_init_module (void)
{
    int result;
  
  	 printk(KERN_INFO CLASS_NAME ": Initializing ...\n");
    mutex_init(&rmutex);
    INIT_LIST_HEAD(&todo_list);
    init_waitqueue_head (&rqueue);

    wiegandMask = 1 << wiegandLength[ay_d19m_mode];

  if (!gpio_is_valid (ay_d19m_d0))
    {
      printk (KERN_INFO CLASS_NAME ": invalid D0 GPIO\n");
      return -ENODEV;
    }
  if (!gpio_is_valid (ay_d19m_d1))
    {
      printk (KERN_INFO CLASS_NAME ": invalid D1 GPIO\n");
      return -ENODEV;
    }
  if (!gpio_is_valid (ay_d19m_power))
    {
      printk (KERN_INFO CLASS_NAME ": invalid Power GPIO\n");
      return -ENODEV;
    }


  gpio_request_one (ay_d19m_power, GPIOF_OUT_INIT_LOW | GPIOF_EXPORT_DIR_FIXED, "av-d19m.power");
  gpio_request_one (ay_d19m_d0, GPIOF_IN | GPIOF_EXPORT_DIR_FIXED, "av-d19m.d0");	// Set up the D0
  gpio_request_one (ay_d19m_d1, GPIOF_IN | GPIOF_EXPORT_DIR_FIXED, "av-d19m.d1");	// Set up the D1

  irqlineD0 = gpio_to_irq (ay_d19m_d0);
  irqlineD1 = gpio_to_irq (ay_d19m_d1);
  printk (KERN_INFO CLASS_NAME ": The D0/D1 mapped to IRQ: %d/%d\n", irqlineD0, irqlineD1);

  result = request_threaded_irq (irqlineD0,	// The interrupt number requested
                 ay_d19m_irqdata,	// The pointer to the handler function below
                 ay_d19m_thread, IRQF_TRIGGER_FALLING,	// Interrupt on rising edge (button press, not release)
                 "ay_d19m D0 gpio_handler",	// Used in /proc/interrupts to identify the owner
                 0);	// The *dev_id for shared interrupt lines, NULL is okay
  printk (KERN_INFO CLASS_NAME ": The D0 interrupt request result is: %d\n", result);

  result = request_threaded_irq (irqlineD1,	// The interrupt number requested
                 ay_d19m_irqdata,	// The pointer to the handler function below
                 ay_d19m_thread, IRQF_TRIGGER_FALLING,	// Interrupt on rising edge (button press, not release)
                 "ay_d19m D1 gpio_handler",	// Used in /proc/interrupts to identify the owner
                 0);	// The *dev_id for shared interrupt lines, NULL is okay
  printk (KERN_INFO CLASS_NAME ": The D1 interrupt request result is: %d\n", result);

 

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   ayd19m_major = register_chrdev(0, DEVICE_NAME, &ayd19m_fops);
   if (ayd19m_major<0){
      printk(KERN_ALERT CLASS_NAME " failed to register a major number\n");
      return ayd19m_major;
   }
   printk(KERN_INFO CLASS_NAME ": registered correctly with major number %d\n", ayd19m_major);

   // Register the device class
   ay_d19m_Class = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ay_d19m_Class)){                // Check for error and clean up if there is
      unregister_chrdev(ayd19m_major, DEVICE_NAME);
      printk(KERN_ALERT CLASS_NAME ":Failed to register device class\n");
      return PTR_ERR(ay_d19m_Class);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO CLASS_NAME ": device class registered correctly\n");

   // Register the device driver
   ay_d19m_Device = device_create(ay_d19m_Class, NULL, MKDEV(ayd19m_major, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ay_d19m_Device)){               // Clean up if there is an error
      class_destroy(ay_d19m_Class);           // Repeated code but the alternative is goto statements
      unregister_chrdev(ayd19m_major, DEVICE_NAME);
      printk(KERN_ALERT CLASS_NAME ":Failed to create the device\n");
      return PTR_ERR(ay_d19m_Device);
   }

   timer_setup(&wiegand_timeout, wiegand_timeoutfunc,0);

   printk(KERN_INFO CLASS_NAME ": device class created correctly\n"); // Made it! device was initialized
//   add_timer(&wiegand_timeout);
//   mod_timer(&wiegand_timeout, HZ/4);
  return result;
}

static irqreturn_t
ay_d19m_irqdata (int irq, void *dev)
{
    if (bitmsk == wiegandMask)
    {
        data1 = gpio_get_value (ay_d19m_d1) ? bitmsk : 0;
        data0 = gpio_get_value (ay_d19m_d0) ? bitmsk : 0;

        mod_timer (&wiegand_timeout, jiffies + HZ / 10);
    }
    else
    {
        data1 |= gpio_get_value (ay_d19m_d1) ? bitmsk : 0;
        data0 |= gpio_get_value (ay_d19m_d0) ? bitmsk : 0;
    }

    bitmsk >>= 1;

    return bitmsk ? IRQ_HANDLED : IRQ_WAKE_THREAD;
}

static irqreturn_t
ay_d19m_thread (int irq, void *dev)
{
    int s;
    char rbuffer[MAX_READSZ];
    printk(KERN_DEBUG CLASS_NAME ": wiegand mode %d, D0 %7.7X, D1 %7.7X, D0 xor D1 %7.7X", ay_d19m_mode, data0, data1, data0 ^ data1);

	s = ffmt[ay_d19m_mode] (data0, data1, rbuffer, sizeof(rbuffer)-2);
	if(s)
	{
		struct todo_data *data = kmalloc(sizeof(struct todo_data),GFP_KERNEL);
		printk(KERN_INFO CLASS_NAME ": new key on mode %d, code %s\n", ay_d19m_mode, rbuffer);

		rbuffer[s++] = '\n';
		rbuffer[s++] = '\0';

		if(data)
		{
			mutex_lock(&rmutex);
			del_timer(&wiegand_timeout);
			strncpy(data->data,rbuffer,s);
			list_add_tail(&data->node, &todo_list);
			mutex_unlock(&rmutex);

			wake_up(&rqueue);
		}
	}

	data0 = data1 = 0;
	bitmsk = wiegandMask;
	return IRQ_HANDLED;
}

static void wiegand_timeoutfunc (struct timer_list *timer)
{
    static int state = 0;
    printk(KERN_DEBUG CLASS_NAME ": Power cycle %d\n", state);

    switch (state)
    {
        case 0:			// switch power on
            gpio_set_value (ay_d19m_power, 1);
            state++;
            mod_timer (&wiegand_timeout, jiffies + HZ / 2);
            printk (KERN_INFO CLASS_NAME ": Switching Power om ...\n");
            break;
        case 1 ... 3:
            if (!gpio_get_value (ay_d19m_power))
            {
                gpio_set_value (ay_d19m_power, 1);
                state++;
                mod_timer (&wiegand_timeout, jiffies + HZ / 2);
                printk (KERN_WARNING CLASS_NAME ": Power on error. Try again.\n");
            }
            else
            {
                del_timer (&wiegand_timeout);
                state = 5;
                printk (KERN_INFO CLASS_NAME ": Power is on.\n");
            }
            break;
        case 4:
            printk (KERN_WARNING CLASS_NAME ": Can not switch power on. Give up.\n");
            state = 0;
            mod_timer(&wiegand_timeout,60 * HZ);
            break;
        case 5:
            gpio_set_value (ay_d19m_power, 0);
            printk (KERN_WARNING CLASS_NAME ": Timed-out, Power cycle.\n");
            state = 0;
            mod_timer (&wiegand_timeout, jiffies + HZ);
            break;
    }
    data0 = data1 = 0;
    bitmsk = wiegandMask;
}

module_init (ayd19m_init_module);
module_exit (ayd19m_cleanup_module);
