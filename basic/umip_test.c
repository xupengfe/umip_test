/*
 * umip_test.c
 *
 * This tests for Intel User-Mode Execution Prevention
 *
 * Copyright (C) 2018, Intel - http://www.intel.com/
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Contributors:
 *        Pengfei, Xu <pengfei.xu@intel.com>
 *        - Tested sgdt, sidt, sldt, smsw and str by asm in C
 *        - All above 5 instruction should be #GP exception in UMIP
 *          supproted and enabled platform, if disabled UMIP will
 *          show instruction store results
 *        - Add parameter for each instruction test
 */

/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GDT_LEN 10
#define IDT_LEN 10
#define LEN 10

void usage(void)
{
	printf("Usage: [g][i][l][m][t][a]\n");
	printf("g      Test sgdt\n");
	printf("i      Test sidt\n");
	printf("l      Test sldt\n");
	printf("m      Test smsw\n");
	printf("t      Test str\n");
	printf("a      Test all\n");
}


static void asm_sgdt(void)
{
	int i;
	unsigned char val[GDT_LEN];

	memset(val,0,sizeof(val));

	printf("Test SGDT and save at [%p]\n", val);
	printf("Initial val:0x");
	for (i = 0; i < GDT_LEN; i++)
		printf("%02x",val[i]);

	asm volatile("sgdt %0\n" : "=m" (val));
	printf("\nSGDT result in val:\n");
	printf("val:0x");
	for (i = 0; i < GDT_LEN; i++)
		printf("%02x",val[i]);
	printf("\nDone.\n\n");
}

static void asm_sidt(void)
{
	int i;
	unsigned char val[IDT_LEN];

	memset(val,0,sizeof(val));

	printf("Test SIDT and save at [%p]\n", val);
	printf("Initial val:0x");
	for (i = 0; i < IDT_LEN; i++)
		printf("%02x",val[i]);
	asm volatile("sidt %0\n" : "=m" (val));
	printf("\nSIDT result in val:\n");
	printf("val:0x");
	for (i = 0; i < GDT_LEN; i++)
		printf("%02x",val[i]);
	printf("\nDone.\n\n");
}

static void asm_sldt(void)
{
	unsigned long val;

	printf("Test SLDT and save at [%p]\n", &val);
	printf("Initial val:0x%lx",val);
	asm volatile("sldt %0\n" : "=m" (val));

	printf("\nSLDT result in val:\n");
	printf("val:0x%lx",val);
	printf("\nDone.\n\n");
}

static void asm_smsw(void)
{
	unsigned long val;

	printf("Test SMSW and save at [%p]\n", &val);
	printf("Initial val:0x%lx",val);
	asm volatile("smsw %0\n" : "=m" (val));

	printf("\nSMSW result in val:\n");
	printf("val:0x%lx",val);
	printf("\nDone.\n\n");
}

static void asm_str(void)
{
	unsigned long val;

	printf("Test STR and save at [%p]\n", &val);
	printf("Initial val:0x%lx",val);
	asm volatile("str %0\n" : "=m" (val));

	printf("\nSTR result in val:\n");
	printf("val:0x%lx",val);
	printf("\nDone.\n");
}


int main(int argc, char *argv[])
{
	char parm;

	if (argc == 1)
	{
		usage();
		exit(1);
	}
	else if (argc == 2)
	{
		sscanf(argv[1], "%c", &parm);
		printf("1 parameters: parm=%c\n", parm);
	}
	else
	{
		sscanf(argv[1], "%c", &parm);
		printf("Just get first parm:%c\n",parm);
	}

	switch (parm)
	{
		case 'a' : printf("Test all.\n");
			asm_sgdt();
			asm_sidt();
			asm_sldt();
			asm_smsw();
			asm_str();
			break;
		case 'g' : asm_sgdt();
			break;
		case 'i' : asm_sidt();
			break;
		case 'l' : asm_sldt();
			break;
		case 'm' : asm_smsw();
			break;
		case 't' : asm_str();
			break;
		default: usage();
			exit(1);
	}
}
