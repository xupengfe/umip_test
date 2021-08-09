/* Test cases for UMIP */
/* Copyright Intel Corporation 2017 */

#include <stdio.h>
#include <stdlib.h>
#include <asm/ldt.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <string.h>
#include "umip_test_defs.h"
#include "test_umip_ldt_64.h"
#include "test_umip_code_64.h"

extern unsigned char test_umip[], test_umip_end[];
extern unsigned char finish_testing[];
extern unsigned char data_fs[SEGMENT_SIZE];
extern unsigned char data_gs[SEGMENT_SIZE];
extern int exit_on_signal;
extern void (*cleanup)(void);
unsigned long old_fsbase, old_gsbase;
unsigned short old_fs, old_gs;

#define CODE_DESC_INDEX 1
#define DATA_FS_DESC_INDEX 2
#define DATA_GS_DESC_INDEX 3

#define RPL3 3
#define TI_LDT 1
#define SEGMENT_SELECTOR(index) (RPL3 | (TI_LDT << 2) | (index << 3))

extern int test_passed, test_failed, test_errors;

void usage(void)
{
	printf("Usage: [NA][l][h]\n");
	printf("l      Test sldt exception\n");
	printf("h      Help\n");
}

static int setup_data_segments()
{
	int ret;
	struct user_desc desc = {
		.entry_number    = 0,
		.base_addr       = 0,
		.limit           = SEGMENT_SIZE,
		.seg_32bit       = 1,
		.contents        = 0, /* data */
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 0,
		.useable         = 1
	};

	desc.entry_number = DATA_FS_DESC_INDEX;
	desc.base_addr = (unsigned long)&data_fs;

	memset(data_fs, 0x66, SEGMENT_SIZE);

	ret = syscall(SYS_modify_ldt, 1, &desc, sizeof(desc));
	if (ret) {
		printf("Failed to install data segment [%d].\n", ret);
		return ret;
	}

	desc.entry_number = DATA_GS_DESC_INDEX;
	desc.base_addr = (unsigned long)&data_gs;

	memset(data_gs, 0x55, SEGMENT_SIZE);

	ret = syscall(SYS_modify_ldt, 1, &desc, sizeof(desc));
	if (ret) {
		printf("Failed to install data segment [%d].\n", ret);
		return ret;
	}

	return 0;
}

static void cleanup_segments(void)
{
	asm volatile("movw %0,%%fs" : :"m" (old_fs));
	asm volatile("movw %0, %%gs" : : "m" (old_gs));

	syscall(SYS_arch_prctl, ARCH_SET_FS, old_fsbase);
	syscall(SYS_arch_prctl, ARCH_SET_GS, old_gsbase);
}

int sldt_exception(void) {
	unsigned char val[10];

	asm volatile("sldt %0\n" NOP_SLED : "=m" (val));
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	unsigned short test_fs, test_gs;
	unsigned char *code;
	struct sigaction action;
	char parm;

	PRINT_BITNESS;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = &signal_handler;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);

#ifdef EMULATE_ALL
	exit_on_signal = 1;
#else
	exit_on_signal = 2;
#endif

	printf("Testresults, exit_on_signal=%d\n",exit_on_signal);
	cleanup = cleanup_segments;
	if (sigaction(SIGSEGV, &action, NULL) < 0) {
		pr_error(test_errors, "Could not set the signal handler!");
		goto err_out;
	}

	if (argc == 1) {
		printf ("No parameter, test as default.\n");
	} else {
		sscanf(argv[1], "%c", &parm);
		pr_info("1 parameters: parm=%c\n", parm);
		switch (parm) {
			case 'l':
				pr_info("Test sldt exception.\n");
				sldt_exception();
				break;
			case 'h':
				usage();
				exit(0);
				break;
			default:
				usage();
				exit(2);
		}
	}

	code = mmap(NULL, CODE_MEM_SIZE, PROT_WRITE | PROT_READ | PROT_EXEC,
		    MAP_PRIVATE | MAP_32BIT | MAP_ANONYMOUS, -1, 0);
	if (!code) {
		printf("Failed to allocate memory for code segment!\n");
		goto err_out;
	}

	memcpy(code, test_umip, test_umip_end - test_umip);

	test_fs = SEGMENT_SELECTOR(DATA_FS_DESC_INDEX);
	test_gs = SEGMENT_SELECTOR(DATA_GS_DESC_INDEX);

	ret = setup_data_segments();
	if (ret) {
		pr_error(test_errors, "Failed to setup segments [%d].\n", ret);
		goto err_out;
	}

	syscall(SYS_arch_prctl, ARCH_GET_FS, &old_fsbase);
	syscall(SYS_arch_prctl, ARCH_GET_GS, &old_gsbase);

	asm volatile("movw %%fs, %0" : "=m" (old_fs));
	asm volatile("movw %%gs, %0" : "=m" (old_gs));

	syscall(SYS_arch_prctl, ARCH_SET_FS, (unsigned long)data_fs);
	syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long)data_gs);

	asm(/* make a backup of everything */
	    "push %%rax\n\t"
	    "push %%rbx\n\t"
	    "push %%rcx\n\t"
	    "push %%rdx\n\t"
	    "push %%rdi\n\t"
	    "push %%rsi\n\t"
	    "push %%rbp\n\t"
	    "push %%r8\n\t"
	    "push %%r9\n\t"
	    "push %%r10\n\t"
	    "push %%r11\n\t"
	    "push %%r12\n\t"
	    "push %%r13\n\t"
	    "push %%r14\n\t"
	    "push %%r15\n\t"
	    /* set new data segment */
            /* jump to test code */
	    "call *%0\n\t"
	    /* After running tests, we return here */
            "finish_testing:\n\t"
	    /* restore everything */
	    "pop %%r15\n\t"
	    "pop %%r14\n\t"
	    "pop %%r13\n\t"
	    "pop %%r12\n\t"
	    "pop %%r11\n\t"
	    "pop %%r10\n\t"
	    "pop %%r9\n\t"
	    "pop %%r8\n\t"
	    "pop %%rbp\n\t"
	    "pop %%rsi\n\t"
	    "pop %%rdi\n\t"
	    "pop %%rdx\n\t"
	    "pop %%rcx\n\t"
	    "pop %%rbx\n\t"
	    "pop %%rax\n\t"
	    :
	    :"m"(code)
	   );

	cleanup_segments();

	printf("===Test results===\n");
	check_results();

	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);

	if (sigaction(SIGSEGV, &action, NULL) < 0) {
		pr_error(test_errors, "Could not remove signal handler!");
		print_results();
		exit(1);
	}
	printf("Exiting...\n");
	print_results();

	return 0;
err_out:
	pr_error(test_errors, "Could not run tests\n");
	print_results();

	printf("Now you will see a segmentation fault. This is under investigation.\n");
	return 1;
};
