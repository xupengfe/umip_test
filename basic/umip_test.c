#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GDTR_LEN 10

int main()
{
	unsigned char val[GDTR_LEN];
	int i;


	for (i = 0; i < GDTR_LEN; i++)
		val[i] = 0;
	printf("Init Val:0x");
	for (i = 0; i < GDTR_LEN; i++)
		printf("%x",val[i]);
	printf("\nWill issue SGDT and save at [%p]\n", val);
	asm volatile("sgdt %0\n" : "=m" (val));
	printf("Will show val:\n");
	for (i = 0; i < GDTR_LEN; i++)
		printf("Val[%d]:0x%x\n",i,val[i]);
	printf("Val:0x");
	for (i = 0; i < GDTR_LEN; i++)
		printf("%x",val[i]);
	printf("\nDone.\n");
}
