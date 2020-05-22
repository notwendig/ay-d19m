/*
 * decoder.c
 *
 *  Created on: May 22, 2020
 *      Author: juergen
 */

#include "ay_d19m.h"
#include "decoder.h"

int wiegand26(uint32_t code0,  char *buffer, size_t bsz)
{
	int i;
	unsigned ep=0, op=1, facility;
	uint16_t code;

	code = (code0 >> 1) & 0xFFFF;
	facility = (code0 >> 16) & 0xFF;

	for (i = 0; i < 13; i++)
	{
		ep ^= code0 & (1 << i) ? 1:0;
		op ^= code0 & (0x2000 << i) ? 1:0;
	}

	if (ep & op)
	{
		snprintf(buffer, bsz, "R=%d, M=%d, F=%d, D=%X, L=26", RES_OK, WIEGAND26, facility, code);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=26", RES_PARITY, WIEGAND26, code0);
	}

	return strlen(buffer);

}

// Single Key, Wiegand 6-Bit (Rosslare Format). Factory setting
int fmt_SKW06RF(uint32_t code0, int bits,  char *buffer, size_t bsz)
{
	int i;
	unsigned ep=0, op=1;
	uint16_t code;

	*buffer = 0;

	code = (code0 >> 1) & 0xF;

	for (i = 0; i < 3; i++)
	{
		ep ^= code0 & (1 << i) ? 1:0;
		op ^= code0 & (8 << i) ? 1:0;
	}

	if (ep & op)
	{
		if(code && code != 12 && code != 13 && code < 15)
		{
			char key;
			if (code == 10) key = '0';
			else if (code == 11) key = '*';
			else if (code == 14) key = '#';
			else key = '0' + code;
			snprintf(buffer, bsz, "R=%d, M=%d, K=\'%c\', L=%d", RES_OK, SKW06RF, key, bits);
		}
		else
			snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_DATAERR, SKW06RF, code0, bits);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_PARITY, SKW06RF, code0, bits);
	}
	return strlen(buffer);
}

// Single Key, Wiegand 6-Bit with Nibble + Parity Bits
int fmt_SKW06NP(uint32_t code0, int bits,  char *buffer, size_t bsz)
{
	int i;
	unsigned ep=0, op=1;
	uint16_t code;

	*buffer = 0;

	code = (code0 >> 1) & 0xF;

	for (i = 0; i < 3; i++)
	{
		ep ^= code0 & (1 << i) ? 1:0;
		op ^= code0 & (8 << i) ? 1:0;
	}

	if (ep & op)
	{
		if(code < 12)
		{
			char key;
			if (code == 10) key = '*';
			else if (code == 11) key = '#';
			else key = '0' + code;
			snprintf(buffer, bsz, "R=%d, M=%d, K=\'%c\', L=%d", RES_OK, SKW06NP, key, bits);
		}
		else
			snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_DATAERR, SKW06NP, code0, bits);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_PARITY, SKW06NP, code0, bits);
	}

	return strlen(buffer);
}

// Single Key, Wiegand 8-Bit, Nibbles Complemented
int fmt_SKW08NC(uint32_t code0, int bits, char *buffer, size_t bsz)
{
	uint16_t code = code0 & 0xF;
	uint16_t icode = (code0 >> 4) & 0xF;

	if ((code ^ icode) == 0xF)
	{
		char key;
		if (code == 10) key = '*';
		else if (code == 11) key = '#';
		else key = '0' + code;
		snprintf(buffer, bsz, "R=%d, M=%d, K=\'%c\', L=%d", RES_OK, SKW08NC, key, bits);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_DATAERR, SKW08NC, code0, bits);
	}

	return strlen(buffer);
}

// 4 Keys Binary + Facility code, Wiegand 26-Bit
int fmt_K4W26BF(uint32_t code0, int bits, char *buffer, size_t bsz)
{
	int i;
	unsigned ep=0, op=1, facility;
	uint16_t code;

	code = (code0 >> 1) & 0xFFFF;
	facility = (code0 >> 16) & 0xFF;

	for (i = 0; i < 13; i++)
	{
		ep ^= code0 & (1 << i) ? 1:0;
		op ^= code0 & (0x2000 << i) ? 1:0;
	}

	if (ep & op)
	{
		snprintf(buffer, bsz, "R=%d, M=%d, F=%d, D=%d, L=%d", RES_OK, K4W26BF, facility, code, bits);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_PARITY, K4W26BF, code0, bits);
	}

	return strlen(buffer);
}

// 1 to 5 Keys + Facility code, Wiegand 26-Bit
int fmt_K5W26FC(uint32_t code0, int bits, char *buffer, size_t bsz)
{
	int i;
	unsigned ep=0, op=1, facility;
	uint16_t code;

	code = (code0 >> 1) & 0xFFFF;
	facility = (code0 >> 16) & 0xFF;

	for (i = 0; i < 13; i++)
	{
		ep ^= code0 & (1 << i) ? 1:0;
		op ^= code0 & (0x2000 << i) ? 1:0;
	}

	if (ep & op)
	{
		snprintf(buffer, bsz, "R=%d, M=%d, F=%d, D=%d, L=%d", RES_OK, K5W26FC, facility, code, bits);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_PARITY, K5W26FC, code0, bits);
	}

	return strlen(buffer);
}

// 6 Keys BCD and Parity Bits, Wiegand 26-Bit
int fmt_K6W26BCD(uint32_t code0, int bits, char *buffer, size_t bsz)
{
	int i;
	unsigned ep=0, op=1;
	uint32_t code;

	code = (code0 >> 1) & 0xFFFFFF;

	for (i = 0; i < 13; i++)
	{
		ep ^= code0 & (1 << i) ? 1:0;
		op ^= code0 & (0x2000 << i) ? 1:0;
	}

	if (ep & op)
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%X, L=%d", RES_OK, K6W26BCD, code, bits);
	}
	else
	{
		snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_PARITY, K6W26BCD, code0, bits);
	}

	return strlen(buffer);
}

int fmt_SK3X4MX(uint32_t code0, int bits, char *buffer, size_t bsz) // not supported yet 		Single Key, 3x4 Matrix Keypad
{
	snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_NOSUPORT, SK3X4MX, code0, bits);
	return strlen(buffer);
}

int fmt_K8CDBCD(uint32_t code0, int bits, char *buffer, size_t bsz) // not supported yet 		1 to 8 Keys BCD, Clock & Data Single Key
{
	snprintf(buffer, bsz, "R=%d, M=%d, D=%8.8X, L=%d", RES_NOSUPORT, K8CDBCD, code0, bits);
	return strlen(buffer);
}
