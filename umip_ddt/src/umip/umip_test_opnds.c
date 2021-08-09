/*
 * umip_test_opnds.c
 *
 * This tests for Intel User-Mode Execution Prevention
 *
 * Copyright (C) 2017, Intel - http://www.intel.com/
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
 *      Neri, Ricardo <ricardo.neri@intel.com>
 *      - GPL v2
 *      - Tested sldt, smsw and str register operands
 *      Pengfei, Xu <pengfei.xu@intel.com>
 *      - Add parameter for each item test and unify the code style
 */

/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include "umip_test_defs.h"

/* Register operands */

#if __x86_64__
#define PUSH(reg) "push %%r"reg"\n"
#define POP(reg) "pop %%r"reg"\n"
#define SAVE_SP(reg) "mov %%rsp, %%r"reg"\n"
#define RESTORE_SP(reg) "mov %%r"reg", %%rsp\n"
#define PFX_ZEROS "16"
#else
#define PUSH(reg) "push %%e"reg"\n"
#define POP(reg) "pop %%e"reg"\n"
#define SAVE_SP(reg) "mov %%esp, %%e"reg"\n"
#define RESTORE_SP(reg) "mov %%e"reg", %%esp\n"
#define PFX_ZEROS "8"
#endif

#define INSNreg16(insn, reg, scratch, scratch_sp) \
	/*
	 * Move initialization value to %scratch. Do it before changing %rsp as	\
	 * some compilers refer local variables as an offset from %esp. This	\
	 * code causes %scratch to be clobbered. This is OK as long it is not	\
	 * used in the caller function.						\
	 */									\
	"mov %0, %%"scratch"\n"						\
	/* Make a backup of test register */					\
	PUSH(reg)								\
	/* Write our initialization value in test register */			\
	"mov %%"scratch", %%"reg"\n"						\
	/* In case we are testing %esp, make a backup to restore after test */	\
	SAVE_SP(scratch_sp)							\
	/* Run our test: register operands */					\
	insn" %%"reg"\n"							\
	NOP_SLED								\
	/* Save result to scratch */						\
	"mov %%"reg", %%"scratch"\n"						\
	/* Restore %esp */							\
	RESTORE_SP(scratch_sp)							\
	/* Restore test register */						\
	POP(reg)								\
	/*									\
	 * Since some compilers refer local variables as an offset from %esp,	\
	 * the test result can only be saved to a local variable only once %esp	\
	 * has been restored by an equal number of stack pops and pushes.	\
	 */									\
	"mov %%"scratch", %1\n"

#define INSNreg32(insn, reg, scratch, scratch_sp) \
	/*
	 * Move initialization value to %scratch. Do it before changing %rsp as	\
	 * some compilers refer local variables as an offset from %esp. This	\
	 * code causes %scratch to be clobbered. This is OK as long it is not	\
	 * used in the caller function.						\
	 */									\
	"mov %0, %%e"scratch"\n"						\
	/* Make a backup of test register */					\
	PUSH(reg)								\
	/* Write our initialization value in test register */			\
	"mov %%e"scratch", %%e"reg"\n"						\
	/* In case we are testing %esp, make a backup to restore after test */	\
	SAVE_SP(scratch_sp)							\
	/* Run our test: register operands */					\
	insn" %%e"reg"\n"							\
	NOP_SLED								\
	/* Save result to scratch */						\
	"mov %%e"reg", %%e"scratch"\n"						\
	/* Restore %esp */							\
	RESTORE_SP(scratch_sp)							\
	/* Restore test register */						\
	POP(reg)								\
	/*									\
	 * Since some compilers refer local variables as an offset from %esp,	\
	 * the test result can only be saved to a local variable only once %esp	\
	 * has been restored by an equal number of stack pops and pushes.	\
	 */									\
	"mov %%e"scratch", %1\n"

#define INSNreg64(insn, reg, scratch, scratch_sp) \
	/*
	 * Move initialization value to %scratch. Do it before changing %rsp as	\
	 * some compilers refer local variables as an offset from %esp. This	\
	 * code causes %scratch to be clobbered. This is OK as long it is not	\
	 * used in the caller function.						\
	 */									\
	"mov %0, %%"scratch"\n"						\
	/* Make a backup of test register */					\
	"push %%"reg"\n"							\
	/* Write our initialization value in test register */			\
	"mov %%"scratch", %%"reg"\n"						\
	/* In case we are testing %esp, make a backup to restore after test */	\
	"mov %%rsp, %%"scratch_sp"\n"						\
	/* Run our test: register operands */					\
	insn" %%"reg"\n"							\
	NOP_SLED								\
	/* Save result to scratch */						\
	"mov %%"reg", %%"scratch"\n"						\
	/* Restore %rsp */							\
	"mov %%"scratch_sp", %%rsp\n"						\
	/* Restore test register */						\
	"pop %%"reg"\n"								\
	/*									\
	 * Since some compilers refer local variables as an offset from %rsp,	\
	 * the test result can only be saved to a local variable only once %rsp	\
	 * has been restored by an equal number of stack pops and pushes.	\
	 */									\
	"mov %%"scratch", %1\n"

#define CHECK_INSN(op_size, insn, reg, val, scratch, scratch_sp, init, exp) \
	do { \
		val = init; \
		mask = get_mask(op_size); \
		if (!mask) \
			return -1; \
		\
		got_signal = 0; \
		got_sigcode = 0; \
		\
		asm volatile(INSNreg##op_size(insn, reg, scratch, scratch_sp) : "=m" (val) : "m" (val): "%"scratch, "%"scratch_sp ); \
		\
		if(inspect_signal(exp_signum, exp_sigcode)) \
			break; \
		/* \
		 * Check that the bits that are supposed to change does so \
		 * as well as that the bits that are not supposed to change \
		 * does not change. \
		 */ \
		if (((val & mask) == (exp & mask)) && ((exp & ~mask) == (exp & ~mask))) \
			pr_pass(test_passed, " On %s-bit '%s %s'! " \
					     "Got [0x%0"PFX_ZEROS"lx] " \
					     "Exp[0x%0"PFX_ZEROS"lx]\n", \
			        #op_size, insn, reg, val, (exp&mask) | (init&~mask)); \
		else { \
			pr_fail(test_failed, " On %s-bit '%s %s'! " \
					     "Got[0x%0"PFX_ZEROS"lx] " \
					     "Exp[0x%0"PFX_ZEROS"lx]\n", \
			       #op_size, insn, reg, val, (exp&mask) | (init&~mask)); \
		} \
	} while(0);

#define CHECK_ALLreg32(insn, val, init, exp) \
	CHECK_INSN(16, insn,  "ax", val, "cx", "dx", init, exp); \
	CHECK_INSN(16, insn,  "cx", val, "ax", "dx", init, exp); \
	CHECK_INSN(16, insn,  "dx", val, "ax", "cx", init, exp); \
	CHECK_INSN(16, insn,  "bx", val, "ax", "dx", init, exp); \
	CHECK_INSN(16, insn,  "bp", val, "ax", "dx", init, exp); \
	CHECK_INSN(16, insn,  "si", val, "ax", "dx", init, exp); \
	CHECK_INSN(16, insn,  "di", val, "ax", "dx", init, exp); \
	CHECK_INSN(32, insn,  "ax", val, "cx", "dx", init, exp); \
	CHECK_INSN(32, insn,  "cx", val, "ax", "dx", init, exp); \
	CHECK_INSN(32, insn,  "dx", val, "ax", "cx", init ,exp); \
	CHECK_INSN(32, insn,  "bx", val, "ax", "dx", init ,exp); \
	CHECK_INSN(32, insn,  "bp", val, "ax", "dx", init, exp); \
	CHECK_INSN(32, insn,  "si", val, "ax", "dx", init, exp); \
	CHECK_INSN(32, insn,  "di", val, "ax", "dx", init, exp);

#define CHECK_ALLreg64(insn, val, init, exp) \
	CHECK_INSN(64, insn, "rax", val, "rcx", "rdx", init, exp); \
	CHECK_INSN(64, insn, "rcx", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "rdx", val, "rax", "rcx", init, exp); \
	CHECK_INSN(64, insn, "rbx", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "rbp", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "rsi", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "rdi", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn,  "r8", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn,  "r9", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "r10", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "r11", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "r12", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "r13", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "r14", val, "rax", "rdx", init, exp); \
	CHECK_INSN(64, insn, "r15", val, "rax", "rdx", init, exp);

#if __x86_64__
#define CHECK_ALLreg(insn, val, init, exp) \
	CHECK_ALLreg32(insn, val, init, exp) \
	CHECK_ALLreg64(insn, val, init, exp)
#else
#define CHECK_ALLreg(insn, val, init, exp) \
	CHECK_ALLreg32(insn, val, init, exp)
#endif


/* Memory operands */
#if __x86_64__
#define INSNmem(insn, reg, scratch, disp) \
	/*
	 * Move initialization value to %scratch. Do it before changing %rsp as	\
	 * some compilers refer local variables as an offset from %esp. This	\
	 * code causes %scratch to be clobbered. This is OK as long it is not	\
	 * used in the caller function.						\
	 */									\
	"mov %0, %%"scratch"\n"							\
	/*									\
	 * Make a backup of our scratch memory. Adjust wrt %rsp according to the\
	 * two stack pushes below.						\
	 */									\
	"push ("disp"-0x8-0x8)(%%rsp)\n"					\
	/* Make a backup of contents of test register */			\
	"push %%"reg"\n"							\
	/* Write our initialization value in scratch memory. */			\
	"mov %%"scratch", "disp"(%%rsp)\n"					\
	/* Make test register point to our scratch memory. */			\
	"mov %%rsp, %%"reg"\n"							\
	/* Run our test: register-indirect addressing with an offset. */	\
	insn" "disp"(%%"reg")\n"						\
	NOP_SLED								\
	/* Save result to %rax */						\
	"mov "disp"(%%rsp), %%"scratch"\n"						\
	/* Restore test register */						\
	"pop %%"reg"\n"								\
	/*									\
	 * Restore scratch memory. Adjust offset wrt %rsp according to the	\
	 * two stack pops above							\
	 */									\
	"pop ("disp"-0x8-0x8)(%%rsp)\n"						\
	/*									\
	 * Since some compilers refer local variables as an offset from %esp,	\
	 * the test result can only be saved to a local variable only once %esp	\
	 * has been restored by an equal number of stack pops and pushes.	\
	 */									\
	"mov %%"scratch", %0\n"
#else
#define INSNmem(insn, reg, scratch, disp) \
	/*
	 * Move initialization value to %scratch. Do it before changing %esp as \
	 * some compilers refer local variables as an offset from %esp. This	\
	 * code causes %scratch to be clobbered. This is OK as long it is not	\
	 * used in the caller function.						\
	 */									\
	"mov %0, %%"scratch"\n"							\
	/*									\
	 * Make a backup of our scratch memory. Adjust wrt %rsp according to the\
	 * two stack pushes below.						\
	 */									\
	"push ("disp"-0x4-0x4)(%%esp)\n"					\
	/* Make a backup of contents of test register */			\
	"push %%"reg"\n"							\
	/*									\
	 * Write our initialization value in scratch memory. Adjust offset	\
	 * according to	the number of previous stack pushes.			\
	 */									\
	"mov %%"scratch", "disp"(%%esp)\n"					\
	/* Make test register point to our scratch memory. */			\
	"mov %%esp, %%"reg"\n"							\
	/* Run our test: register-indirect addressing with an offset. */	\
	insn" "disp"(%%"reg")\n"						\
	NOP_SLED								\
	/* Save result to %eax */						\
	"mov "disp"(%%esp), %%"scratch"\n"					\
	/* Restore test register */						\
	"pop %%"reg"\n"								\
	/*									\
	 * Restore scratch memory. Adjust offset wrt %rsp according to the	\
	 * two stack pops above							\
	 */									\
	"pop ("disp"-0x4-0x4)(%%esp)\n"						\
	/*									\
	 * Since some compilers refer local variables as an offset from %esp,	\
	 * the test result can only be saved to a local variable only once %esp	\
	 * has been restored by an equal number of stack pops and pushes.	\
	 */									\
	"mov %%"scratch", %0\n"
#endif

/*
 * TODO: test no displacement. However, gcc refuses to not use displacement.
 * Instead, it uses -0x0(%%reg). No displacement is OK unless the SIB byte
 * is used with RBP.
 */
#define INSNmemdisp8(insn, reg, scratch) INSNmem(insn, reg, scratch, "-0x80")
#define INSNmemdisp32(insn, reg, scratch) INSNmem(insn, reg, scratch, "-0x1000")

#define CHECK_INSNmemdisp(INSNmacro, insn, reg, scratch, val, init, exp) \
	val = init; \
	/* Memory operands are always treated as 16-bit locations */ \
	mask = get_mask(16); \
	if (!mask) \
		return -1; \
		\
		got_signal = 0; \
		got_sigcode = 0; \
	\
	asm volatile(INSNmacro(insn, reg, scratch) : "=m" (val): "m"(val) : "%"scratch""); \
	\
	if(!inspect_signal(exp_signum, exp_sigcode)) { \
		/* \
		* Check that the bits that are supposed to change does so \
		* as well as that the bits that are not supposed to change \
		* does not change. \
		*/ \
		if (((val & mask) == (exp & mask)) && ((exp & ~mask) == (exp & ~mask))) \
			pr_pass(test_passed, "On '%s %s(%s)'! " \
					     "Got [0x%0"PFX_ZEROS"lx] " \
					     "Exp[0x%0"PFX_ZEROS"lx]\n", \
			insn, #INSNmacro, reg, val, (exp & mask) | (init & ~mask)); \
		else { \
			pr_fail(test_failed, "On '%s %s(%s)'! " \
					     "Got[0x%0"PFX_ZEROS"lx] " \
					     "Exp[0x%0"PFX_ZEROS"lx]\n", \
				insn, #INSNmacro, reg, val, (exp & mask) | (init & ~mask)); \
		}\
	}

#define CHECK_INSNmem(insn, reg, scratch, val, init, exp) \
	CHECK_INSNmemdisp(INSNmemdisp8, insn, reg, scratch, val, init, exp) \
	CHECK_INSNmemdisp(INSNmemdisp32, insn, reg, scratch, val, init, exp)

#if __x86_64__
#define CHECK_ALLmem(insn, val, init, exp) \
	CHECK_INSNmem(insn, "rax", "rcx", val, init, exp) \
	CHECK_INSNmem(insn, "rcx", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "rdx", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "rbx", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "rsp", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "rbp", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "rsi", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "rdi", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r8", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r9", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r10", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r11", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r12", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r13", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r14", "rax", val, init, exp) \
	CHECK_INSNmem(insn, "r15", "rax", val, init, exp)
#else
#define CHECK_ALLmem(insn, val, init, exp) \
	CHECK_INSNmem(insn, "eax", "ecx", val, init, exp) \
	CHECK_INSNmem(insn, "ecx", "eax", val, init, exp) \
	CHECK_INSNmem(insn, "edx", "eax", val, init, exp) \
	CHECK_INSNmem(insn, "ebx", "eax", val, init, exp) \
	CHECK_INSNmem(insn, "esp", "eax", val, init, exp) \
	CHECK_INSNmem(insn, "ebp", "eax", val, init, exp) \
	CHECK_INSNmem(insn, "esi", "eax", val, init, exp) \
	CHECK_INSNmem(insn, "edi", "eax", val, init, exp)
#endif

#define INIT_SS   INIT_VAL(13131313)
#define INIT_MSW  INIT_VAL(14141414)
#define INIT_LDTS INIT_VAL(15151515)

extern sig_atomic_t got_signal, got_sigcode;
int test_passed, test_failed, test_errors;

static unsigned long get_mask(int op_size) {
	switch (op_size) {
	case 16:
		return 0xffff;
	case 32:
		return 0xffffffff;
#if __x86_64__
	case 64:
		return 0xffffffffffffffff;
#endif
	default:
		pr_error(test_errors, "Invalid operand size!\n");
		/*
		 * We can't return -1 as it would be equal to the
		 * 32 or 64-bit mask
		 */
		return 0;
	}
}

static int test_str(void)
{
	unsigned long val;
	unsigned long mask = 0xffff;
	int exp_signum, exp_sigcode;

	if (kver_cmp(5, 10) == 0) {
		pr_info("Kernel version is or newer than v5.10\n");
		if(unexpected_signal())
			return 1;
	} else {
		pr_info("Kernel version is older than v5.10\n");
		INIT_EXPECTED_SIGNAL_STR_SLDT(exp_signum, 0, exp_sigcode, 0);
	}

	pr_info("====Checking STR. Expected value: [0x%lx]====\n", expected_tr);
	pr_info("==Tests for register operands==\n");
	pr_info("Value should be saved at [0x%p]. Init value[0x%0"PFX_ZEROS"lx]\n",
		&val, expected_tr);
	CHECK_ALLreg("str", val, INIT_SS, (unsigned long)expected_tr);
	pr_info("==Tests for memory operands==\n");
	pr_info("Value should be saved at [0x%p]. Init value[0x%0"PFX_ZEROS"lx]\n",
		&val, expected_tr);
	CHECK_ALLmem("str", val, INIT_SS, (unsigned long)expected_tr);
	/* TODO: check with addressing using SIB byte */
	return 0;

}

static int test_smsw(void)
{
	unsigned long val;
	unsigned long mask = 0xffff;
	int exp_signum, exp_sigcode;

#ifdef __x86_64__
	if (kver_cmp(5, 4) == 0) {
		pr_info("Kernel version is or newer than v5.4\n");
		if(unexpected_signal())
			return 1;
	} else {
		pr_info("Kernel version is older than v5.4\n");
		INIT_EXPECTED_SIGNAL(exp_signum, 0, exp_sigcode, 0);
	}
#else
	INIT_EXPECTED_SIGNAL(exp_signum, 0, exp_sigcode, 0);
#endif


	pr_info("====Checking SMSW. Expected valuE: [0x%lx]====\n", expected_msw);
	pr_info("==Tests for register operands==\n");
	pr_info("Value should be saved at [0x%p]. Init value[0x%0"PFX_ZEROS"lx]\n",
		&val, expected_msw);
	CHECK_ALLreg("smsw", val, INIT_MSW, (unsigned long)expected_msw);
	pr_info("==Tests for memory operands==\n");
	pr_info("Value should be saved at [0x%p]. Init value[0x%0"PFX_ZEROS"lx]\n",
		&val, expected_msw);
	CHECK_ALLmem("smsw", val, INIT_MSW, (unsigned long)expected_msw);
	/* TODO: check with addressing using SIB byte */
	return 0;
}

static int test_sldt(void)
{
	unsigned long val;
	unsigned long mask = 0xffff;
	int exp_signum, exp_sigcode;

	if (kver_cmp(5, 10) == 0) {
		pr_info("Kernel version is or newer than v5.10\n");
		if(unexpected_signal())
			return 1;
	} else {
		pr_info("Kernel version is older than v5.10\n");
		INIT_EXPECTED_SIGNAL_STR_SLDT(exp_signum, 0, exp_sigcode, 0);
	}

	pr_info("====Checking SLDT. Expected value: [0x%lx]====\n", expected_ldt);
	pr_info("==Tests for register operands==\n");
	pr_info("Value should be saved at [0x%p]. Init value[0x%0"PFX_ZEROS"lx]\n",
		&val, expected_ldt);
	CHECK_ALLreg("sldt", val, INIT_LDTS, (unsigned long)expected_ldt);
	pr_info("==Tests for memory operands==\n");
	pr_info("Value should be saved at [0x%p]. Init value[0x%0"PFX_ZEROS"lx]\n",
		&val, expected_ldt);
	CHECK_ALLmem("sldt", val, INIT_LDTS, (unsigned long)expected_ldt);
	/* TODO: check with addressing using SIB byte */
	return 0;
}

void usage(void)
{
	printf("Usage: [l][m][t][a]\n");
	printf("l      Test sldt register operands\n");
	printf("m      Test smsw register operands\n");
	printf("t      Test str register operands\n");
	printf("a      Test all\n");
}

int main (int argc, char *argv[])
{
	int ret_str=0, ret_smsw=0, ret_sldt=0;
	struct sigaction action;

	PRINT_BITNESS;

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = &signal_handler;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	char parm;

	if (argc == 1)
	{
		usage();
		exit(1);
	}
	else if (argc == 2)
	{
		sscanf(argv[1], "%c", &parm);
		pr_info("1 parameters: parm=%c\n", parm);
	}
	else
	{
		sscanf(argv[1], "%c", &parm);
		pr_info("Just get parm=%c\n", parm);
	}

	if (sigaction(SIGSEGV, &action, NULL) < 0) {
		pr_error(test_errors, "Could not set the signal handler!");
		exit(1);
	}

	pr_info("***Starting tests***\n");

	switch (parm)
	{
		case 'a' : pr_info("Test all.\n");
			pr_info("***Test str next***\n");
			ret_str = test_str();
			pr_info("***Test smsw next***\n");
			ret_smsw = test_smsw();
			pr_info("***Test sldt next***\n");
			ret_sldt = test_sldt();
			break;
		case 't' : pr_info("***Test str next***\n");
			ret_str = test_str();
			break;
		case 'm' : pr_info("***Test smsw next***\n");
			ret_smsw = test_smsw();
			break;
		case 'l' : pr_info("***Test sldt next***\n");
			ret_sldt = test_sldt();
			break;
		default: usage();
		exit(1);
	}

	if (ret_str || ret_smsw || ret_sldt)
		pr_info("***Test completed with errors str[%d] smsw[%d] sldt[%d]\n",
		       ret_str, ret_smsw, ret_sldt);
	else
		pr_info("***All tests completed successfully.***\n");

	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);

	if (sigaction(SIGSEGV, &action, NULL) < 0) {
		pr_error(test_errors, "Could not remove signal handler!");
		return 1;
	}

	print_results();
	return 0;
}
