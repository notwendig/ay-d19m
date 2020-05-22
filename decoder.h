/*
 * decoder.h
 *
 *  Created on: May 22, 2020
 *      Author: juergen
 */

#ifndef DECODER_H_
#define DECODER_H_
#include <linux/types.h>	/* size_t */
#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>


#define	MAX_READSZ		35

typedef int (*fmt)( uint32_t, int, char *, size_t);

int wiegand26(uint32_t code0,  char *buffer, size_t bsz);

typedef enum {
	RES_OK,
	RES_PARITY,
	RES_DATAERR,
	RES_NOSUPORT
}ayd19m_result;
/*
 * AYD19M Keypad Transmission Format (0 to 7). Default 0 (SKW06RF)
 */
typedef enum  { // Read format			Description
    SKW06RF,    // "M=0, K=%c"			Single Key, Wiegand 6-Bit (Rosslare Format). Factory setting
    SKW06NP,    // "M=1, K=%c" 			Single Key, Wiegand 6-Bit with Nibble + Parity Bits
    SKW08NC,    // "M=2, K=%c" 			Single Key, Wiegand 8-Bit, Nibbles Complemented
    K4W26BF,    // "M=3, F=%d, C=%d" 	4 Keys Binary + Facility code, Wiegand 26-Bit
    K5W26FC,    // "M=4, F=%d, C=%d" 	1 to 5 Keys + Facility code, Wiegand 26-Bit
    K6W26BCD,   // "M=5, F=%d, C=%d"	6 Keys BCD and Parity Bits, Wiegand 26-Bit
    SK3X4MX,    // not supported yet	Single Key, 3x4 Matrix Keypad
    K8CDBCD,     // not supported yet	1 to 8 Keys BCD, Clock & Data Single Key

    WIEGAND26	// Wiegand 26 (H10301, 40134) Card Format
}ayd19m_mode_t;

/*
 * SKW06RF
 * Single Key, Wiegand 6-Bit (Rosslare Format). Factory setting
 * Each key press immediately sends 4 bits with 2 parity bits added; even
 * parity for the first 2 bits and odd parity for the last 2 bits.
 * (EP) AAAA (OP)
 * Where:
 * EP = Even parity for first 2 bits
 * OP = Odd parity for last 2 bits
 * A = 4-bit key-code
 * 0 = 1 1010 0
 * 1 = 0 0001 0
 * 2 = 0 0010 0
 * 3 = 0 0011 1
 * 4 = 1 0100 1
 * 5 = 1 0101 0
 * 6 = 1 0110 0
 * 7 = 1 0111 1
 * 8 = 1 1000 1
 * 9 = 1 1001 0
 * * = 1 1011 1 = "B" in Hexadecimal
 * # = 0 1110 0 = "E" in Hexadecimal
 */
int fmt_SKW06RF(uint32_t code0, int bits,  char *buffer, size_t bsz);

/*
 * SKW06NP
 * Single Key, Wiegand 6-Bit, Nibble & Parities
 * Each key press immediately sends 4 bits with 2 parity bits added; even
 * parity for the first 2 bits and odd parity for the last 2 bits.
 * (EP) AAAA (OP)
 * Where:
 * EP = Even parity for first 2 bits
 * OP = Odd parity for last 2 bits
 * A = 4-bit key-code
 * 0 = 0 0000 1
 * 1 = 0 0001 0
 * 2 = 0 0010 0
 * 3 = 0 0011 1
 * 4 = 1 0100 1
 * 5 = 1 0101 0
 * 6 = 1 0110 0
 * 7 = 1 0111 1
 * 8 = 1 1000 1
 * 9 = 1 1001 0
 * * = 1 1010 0 = "A" in Hexadecimal
 * # = 1 1011 1 = "B" in Hexadecimal
 */
int fmt_SKW06NP(uint32_t code0, int bits,  char *buffer, size_t bsz);

/*
 * SKW08NC
 * Single Key, Wiegand 8-Bit, Nibbles Complemented
 * This option inverts the most significant bits in the message leaving the
 * least 4 significant bits as Binary Coded Decimal (BCD) representation
 * of the key. The host system receives an 8-bit message.
 * 0 = 11110000
 * 1 = 11100001
 * 2 = 11010010
 * 3 = 11000011
 * 4 = 10110100
 * 5 = 10100101
 * 6 = 10010110
 * 7 = 10000111
 * 8 = 01111000
 * 9 = 01101001
 * * = 01011010 = "A" in Hexadecimal
 * # = 01001011 = "B" in Hexadecimal
 */
int fmt_SKW08NC(uint32_t code0, int bits, char *buffer, size_t bsz);

/*
 * K4W26BF
 * 4 Keys Binary + Facility Code, Wiegand 26-Bit
 * This option buffers 4 keys and outputs keypad data with a three-digit
 * Facility code like a standard 26-bit card output.
 * The Facility code is set in Programming Menu number four and can be
 * in the range 000 to 255. The factory default setting for the Facility
 * code is 000 (see Section 5.6).
 * The keypad PIN code must be 4 digits long and can range between
 * 0000 and 9999. On the fourth key press of the 4-digit PIN code, the
 * data is sent across the Wiegand data lines as binary data in the same
 * format as a 26-bit card.
 * If * or # is pressed during PIN code entry, the keypad clears the PIN
 * code entry buffer, generates a beep and is ready to receive a new 4-
 * digit keypad PIN code.
 * If the entry of the 4-digit keypad PIN code is disrupted and no number
 * key is pressed within 5 seconds, the keypad clears the PIN code entry
 * buffer, generates a beep, and is ready to receive a new 4-digit keypad
 * PIN code.
 * (EP) FFFF FFFF AAAA AAAA AAAA AAAA (OP)
 * Where:
 * EP = Even parity for first 12 bits
 * OP = Odd parity for last 12 bits
 * F = 8-bit Facility code
 * A = 24-bit code generated from keyboard
 */
int fmt_K4W26BF(uint32_t code0, int bits, char *buffer, size_t bsz);

/*
 * K5W26FC
 * 1 to 5 Keys + Facility Code, Wiegand 26-Bit
 * This option buffers up to 5 keys and outputs keypad data with a
 * Facility code like a 26-bit card output.
 * The Facility code is set in Programming Menu number four and can be
 * in the range 000 to 254. The factory default setting for the Facility
 * code is 000 (see Section 5.6).
 * The keypad PIN code can be one to five digits long and can range
 * between 1 and 65,535. When entering a keypad PIN code that is less
 * than 5 digits long, # must be pressed to signify the end of PIN code
 * entry. For keypad PIN codes that are 5 digits long, on the fifth key
 * press of the 5-digit PIN code, the data is sent across the Wiegand data
 * lines as binary data in the same format as a 26-bit card.
 * If * or # is pressed during PIN code entry or a PIN code greater than
 * 65,535 is entered, the keypad clears the PIN code entry buffer,
 * generates a beep and is ready to receive a new 4-digit keypad PIN
 * code.
 * If the entry of the 1- to 5-digit keypad PIN code is disrupted and a
 * number key or # is not pressed within 5 seconds, the keypad clears
 * the PIN code entry buffer, generates a medium length beep and is
 * ready to receive a new 1 to 5-digit keypad PIN code.
 * (EP) FFFF FFFF AAAA AAAA AAAA AAAA (OP)
 * AY-Dx9M Family Installation and Programming Manual
 * 19Programming the AY-Dx9M
 * Where:
 * EP = Even parity for first 12 bits
 * OP = Odd parity for last 12 bits
 * F = 8-bit Facility code
 * A = 24-bit code generated from keyboard
 */
int fmt_K5W26FC(uint32_t code0, int bits, char *buffer, size_t bsz);

/*
 * K6W26BCD
 * 6 Keys BCD and Parity Bits, Wiegand 26-Bit
 * Sends buffer of 6 keys, adds parity and sends a 26-bit BCD message.
 * Each key is a four bit equivalent of the decimal number.
 * The keypad PIN code must be 6 key presses long. On the sixth key
 * press of the 6-digit PIN code, the data is sent across the Wiegand data
 * lines as a BCD message.
 * If the entry of the 6-digit keypad PIN code is disrupted and a number
 * key or # is not pressed within 5 seconds, the keypad clears the PIN
 * code entry buffer, generates a medium length beep and is ready to
 * receive a new 6-digit keypad PIN code.
 * (EP) AAAA BBBB CCCC DDDD EEEE FFFF (OP)
 * Where:
 * A = First key entered D = Fourth key entered
 * B = Second key entered E = Fifth key entered
 * C = Third key entered F = Sixth key entered
 */
int fmt_K6W26BCD(uint32_t code0, int bits, char *buffer, size_t bsz);

/*
 * SK3X4MX
 * Single Key, 3x4 Matrix Keypad (MD-P64)
 * This unidque mode is intended to let the host controller scan the AY-
 * Dx9M keypad while still keeping the proximity card readers Wiegand
 * 26-Bit or Clock & Data formats active.
 * An optional interface board must be used between the AY-Dx9M and
 * the host system. Each key press is immediately sent on DATA0 as an
 * ASCII character at a baud rate of 9600 bits per second.
 * When a key is pressed, DATA1 is pulled "low" until the key is
 * released at which point DATA1 is set to "high". This allows the
 * controller to detect the duration of the key press.
 * The MD-P64 interface unit outputs the data received to 7 outputs
 * emulating a keyboard. The interface unit does not affect any data
 * that it receives from the proximity reader whether it is Wiegand 26-Bit
 * or Clock & Data.
 * Key pressed = ASCII Value
 * 0 = '0' (0x30 hex) 6 = '6' (0x36 hex)
 * 1 = '1' (0x31 hex) 7 = '7' (0x37 hex)
 * 2 = '2' (0x32 hex) 8 = '8' (0x38 hex)
 * 3 = '3' (0x33 hex) 9 = '9' (0x39 hex)
 * 4 = '4' (0x34 hex) *= '*' (0x2A hex)
 * 5 = '5' (0x35 hex) # = '#' (0x23 hex)
 */
int fmt_SK3X4MX(uint32_t code0, int bits, char *buffer, size_t bsz);

/*
 * K8CDBCD
 * 1 to 8 Keys BCD, Clock & Data
 * Buffers up to 8 keys and outputs keypad data without a facility code
 * like standard Clock and Data card output.
 * The keypad PIN code can be one to eight digits long. The PIN code
 * length is selected while programming the reader for Option 8. The
 * reader transmits the data when it receives the last key press of the PIN
 * code. The data is sent across the two data output lines as binary data
 * in Clock & Data format.
 * If * or # is pressed during PIN code entry, the keypad clears the PIN
 * code entry buffer, generates a beep, and is ready to receive a new
 * keypad PIN code.
 * If the entry of the keypad PIN code is disrupted and a number key or
 * # is not pressed within 5 seconds, the keypad clears the PIN code
 * entry buffer, generates a medium length beep and is ready to receive
 * a new keypad PIN code.
 */
int fmt_K8CDBCD(uint32_t code0, int bits, char *buffer, size_t bsz);

#endif /* DECODER_H_ */
