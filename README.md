wiegand-linux AY-D19M Device Driver
====================================

V1.0.0 untested

Linux driver for reading wiegand data from 
AY-D19M Indoor Multi-Format Readers.

The AY-D19M is a programmable indoor reader that allow
entry via a personal identification number (PIN) and/or 
by presenting a proximity card. 
The keypad can be programmed to output eight
different data formats. The AY-D19M supports multiple proximity
card formats to provide a high level of compatibility and connectivity
with host controllers.

This driver kernel module is developed on a Raspberry Pi 3B+ 
running Raspbian RT-Kernel version:
Linux nadipi 4.19.71-rt24-nadipi-v7+ #1 SMP PREEMPT RT Thu May 14 11:22:59 CEST 2020 armv7l GNU/Linux

To connect the TTL Reader-Interface to RPi's 3,3V GPIO a Iono Pi board 
(IPMB20RP Iono Pi with Raspberry Pi 3 Model B+) is installed.

This Iono board, one of its open-collector outputs, is also used to 
control the power-line of the Reader.

BUILD
=====
To Build the module you must have installed the kernel-module build environment.
Change to the project-directory and type make.

INSTALL (root)
=======
Copy or link the builded module ay_d19m.ko into your kernel-module directory 
and type depmod -a

USING
=====  
To use thies driver load the module ay_d19m <params>
Enter: modprobe ay_d19m [optional pasams=n]
Where <params> could be one or several of the params, 
shown be entering the following command.

modinfo ay-d19m 

filename:       /lib/modules/4.19.71-rt24-nadipi-v7+/ay-d19m.ko

version:        0.1

description:    AY_D19M KeyPad Driver.

author:         Jürgen Willi Sievers <JSievers@NadiSoft.de>

license:        GPL

srcversion:     7DD61FFEFB2099176C96559

depends:      

name:           ay_d19m

vermagic:       4.19.71-rt24-nadipi-v7+ SMP preempt mod_unload modversions ARMv7 p2v8 

parm:           ay_d19m_power:AYD19M Power GPOI Port. Default GPIO18 (uint)

parm:           ay_d19m_d0:AYD19M DATA0 GPOI Port. Default GPIO4 (uint)

parm:           ay_d19m_d1:AYD19M DATA1 GPOI Port. Defaul GPIO26 (uint)

parm:           ay_d19m_mode:AYD19M Keypad Transmission (0..) Format. Default 0 (uint)

Where transmission formats are:

	ay_d19m_mode=n   	Reader format
	
	 0 	Single Key, Wiegand 6-Bit (Rosslare Format). Factory setting
	 
	 1	Single Key, Wiegand 6-Bit with Nibble + Parity Bits
	 
	 2	Single Key, Wiegand 8-Bit, Nibbles Complemented
	 
	 3	4 Keys Binary + Facility code, Wiegand 26-Bit
	 
	 4	1 to 5 Keys + Facility code, Wiegand 26-Bit
	 
	 5	6 Keys BCD and Parity Bits, Wiegand 26-Bi
	 
not supported	 6		Single Key, 3x4 Matrix Keypad

not supported	 7		1 to 8 Keys BCD, Clock & Data Single Key

You must programming the Reader to the equivalent mode that was given 
by the ay_d19m_mode parameter.

TEST
====
open a 2nd console and type
journalctl -f

on the 1st console j
-- Logs begin at Tue 2020-05-19 18:32:18 CEST. --
Mai 19 22:03:14 nadipi kernel: AYD19M: cleanup success
Mai 19 22:03:56 nadipi kernel: AYD19M: The D0/D1 is mapped to IRQ: 168/169
Mai 19 22:03:56 nadipi kernel: AYD19M: The D0 interrupt request result is: 0
Mai 19 22:03:56 nadipi kernel: AYD19M: The D1 interrupt request result is: 0
Mai 19 22:03:56 nadipi kernel: AYD19M: Initializing the EBBChar LKM
Mai 19 22:03:56 nadipi kernel: AYD19M: registered correctly with major number 240
Mai 19 22:03:56 nadipi kernel: AYD19M: device class registered correctly
Mai 19 22:03:56 nadipi kernel: AYD19M: device class created correctly


cat /dev/ayd19m

[enter code on the Device]


Annotations
===========
Only wiegand 26 bit formats are supported

Card Format 	Facility Sequence	Notes
26-bit H10301 	0-255		0-65,535 	Standard, most common format
26-Bit 40134 	0-255 	0-65,535 	For Indala systems
