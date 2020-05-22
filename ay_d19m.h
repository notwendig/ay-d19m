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
#ifndef _AY_D19M_H
#define _AY_D19M_H

/* Raspberry PI 3 Model B+ with Iono PI IPMB20RP IO-Board
 * module_param
 * ay_d19m_power, 	GPIO-Output-Pin for AYD19M Power-Control. Default GPIO18
 * ay_d19m_d0,		GPIO-Input-Pin for Wiegand D0-Line. Default GPIO4
 * ay_d19m_d1,		GPIO-Input-Pin for Wiegand D1-Line. Default GPIO26
 * ay_d19m_mode,	AYD19M Keypad Transmission Format (1 to 8). Default 1
 */

#define  DEVICE_NAME "ayd19m"	///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "AYD19M"	///< The device class -- this is a character device driver

#define AY_D19M_POWER 	18 	/* GPIO18   out  Iono open collector output         */
#define AY_D19M_D0 		4		/* GPIO4    in   Iono Wiegand DATA0 generic TTL I/O */
#define AY_D19M_D1 		26 	/* GPIO26   in   Iono Wiegand DATA1 generic TTL I/O */

#endif /* _AY_D19M_H */
