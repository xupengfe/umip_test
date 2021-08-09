#!/usr/bin/env python2
#
# Copyright (c) 2017, Intel Corporation.
#
# Test-case generator for UMIP
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# Contributors:
#      Neri, Ricardo <ricardo.neri@intel.com>
#
# This file generates opcdoes for all (well, most) of the combinations
# of memory operands for the UMIP-protected instructions. This includes
# all the ModRM values and segments, auto generate below files:
# test_umip_code_32.h, test_umip_ldt_32.c and test_umip_ldt_32.h
#

import argparse

MODRM_MO0 = 0
MODRM_MO1 = 1
MODRM_MO2 = 2
MODRM_MO3 = 3

#TODO: need to be based on the number test cases
SEGMENT_SIZE = 262144
CODE_MEM_SIZE = 262144

TEST_PASS_CTR_VAR = "test_passed"
TEST_FAIL_CTR_VAR = "test_failed"
TEST_ERROR_CTR_VAR = "test_errors"


class Instruction:
	def __init__(self, name, opcode, modrm_reg, result_bytes, expected_val):
		self.name = name
		self.opcode = opcode
		self.modrm_reg = modrm_reg
		self.result_bytes = result_bytes
		self.expected_val = expected_val


class Register:
	def __init__(self, mnemonic, modrm_rm, name):
		self.mnemonic = mnemonic
		self.modrm_rm = modrm_rm
		self.name = name

class Segment:
	def __init__(self, name, prefix, array):
		self.name = name
		self.prefix = prefix
		self.array = array


EAX = Register("%eax", 0, "eax")
ECX = Register("%ecx", 1, "ecx")
EDX = Register("%edx", 2, "edx")
EBX = Register("%ebx", 3, "ebx")
ESP = Register("%esp", 4, "esp")
EBP = Register("%ebp", 5, "ebp")
ESI = Register("%esi", 6, "esi")
EDI = Register("%edi", 7, "edi")

SMSW = Instruction("smsw", "0xf, 0x1", 4, 2, "expected_msw")
SLDT = Instruction("sldt", "0xf, 0x0", 0, 2, "expected_ldt")
STR  = Instruction("str", "0xf, 0x0", 1, 2, "expected_tr")
SIDT = Instruction("sidt", "0xf, 0x1", 1, 6, "&expected_idt")
SGDT = Instruction("sgdt", "0xf, 0x1", 0, 6, "&expected_gdt")

CS = Segment("cs", "", "code")
DS = Segment("ds", "", "data")
SS = Segment("ss", "", "stack")
ES = Segment("es", "0x26", "data_es")
FS = Segment("fs", "0x64", "data_fs")
GS = Segment("gs", "0x65", "data_gs")

DATA_SEGS = [ DS, ES, FS, GS ]
INSTS = [SMSW, SLDT, STR, SGDT, SIDT ]

MO0 = [ EAX, ECX, EDX, EBX, ESI, EDI ]
MO1 = [ EAX, ECX, EDX, EBX, EBP, ESI, EDI ]
MO2 = MO1

SIB_index = [ EAX, ECX, EDX, EBX, EBP, ESI, EDI ]

SIB_base_MO0 = [ EAX, ECX, EDX, EBX, ESI, EDI ]
SIB_base_MO1 = [ EAX, ECX, EDX, EBX, EBP, ESI, EDI]
SIB_base_MO2 = SIB_base_MO1

def two_comp_32(val):
	if (val > 0):
		return val
	else:
		return ((abs(val) ^ 0xffffffff) + 1) & 0xffffffff

def two_comp_8(val):
	if (val > 0):
		return val
	else:
		return ((abs(val) ^ 0xff) + 1) & 0xff

def my_hex(val):
	return hex(val).rstrip("L")

def find_backup_reg(do_not_use):
	regs = [ EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI]
	for u in do_not_use:
		regs.remove(u)
	return regs[0]

def get_segment_prefix(segment, register, modrm, sib=0):
	""" default segments """
	if (segment.prefix == ""):
		segment_str = ""
		if ((register == EBP) or (register == ESP)):
			# This is for a pure disp32, no register involved. Thus, default is data
			if (((modrm >> 6) == 0) and (modrm & 0x7) == 5):
				segment_chk_str = "data"
			# If using a sib byte and the base is EBP, use DS as the base is ignored
			elif (((modrm >> 6) == 0) and ((modrm & 0x7) == 4) and ((sib & 0x7) == 5)):
				segment_chk_str = "data"
			else:
				segment_chk_str = "stack"
		else:
			segment_chk_str = "data"
	else:
		segment_str = segment.prefix + ", "
		segment_chk_str = segment.array

	return segment_str, segment_chk_str

def generate_check_code(comment, segment_chk_str, address, inst, pass_ctr, fail_ctr):
	# TODO: Use an enum here
	if (inst.result_bytes == 2):
		checkcode = "\tgot = *(unsigned short *)(" + segment_chk_str +" + " + str(my_hex(address)) + ");\n"
		checkcode += "\tpr_result(got, expected, \"" + comment + "\", " + pass_ctr + ", " + fail_ctr + ");\n"
	elif (inst.result_bytes == 6):
		checkcode = "\tgot = (struct table_desc *)(" + segment_chk_str +" + " + str(my_hex(address)) + ");\n"
		checkcode += "\tpr_result_table(got, expected, \"" + comment + "\", " + pass_ctr + ", " + fail_ctr + ");\n"
	return checkcode

def generate_disp(modrm, disp, sib=0):
	modrm_mod = modrm >> 6
	if (modrm_mod == 0):

		# if a sib byte is used, disp 32 is used
		if (((sib &7) == 5) and (modrm & 7) == 4):
			disp_2comp = two_comp_32(disp)
			disp_str = ", " + str(disp_2comp & 0xff) + ", "
			disp_str += str((disp_2comp >> 8)  & 0xff) + ", "
			disp_str += str((disp_2comp >> 16) & 0xff) + ", "
			disp_str += str((disp_2comp >> 24) & 0xff)
		else:
			disp_str = ""
	elif (modrm_mod == 1):
		disp_2comp = two_comp_8(disp)
		disp_str = ", " + str(disp_2comp)
	elif (modrm_mod == 2):
		disp_2comp = two_comp_32(disp)
		disp_str = ", " + str(disp_2comp & 0xff) + ", "
		disp_str += str((disp_2comp >> 8)  & 0xff) + ", "
		disp_str += str((disp_2comp >> 16) & 0xff) + ", "
		disp_str += str((disp_2comp >> 24) & 0xff)

	return disp_str

def generate_code(tc_nr, segment, inst, register, modrm_mod, index, disp):
	code_start = "\t\".byte "
	code_end = "\\n\\t\"\n"

	modrm = (modrm_mod << 6) | (inst.modrm_reg << 3) | register.modrm_rm
	modrm_str = ", " + str(my_hex(modrm))
	mov_reg_str = "\t\"mov $" + str(my_hex(two_comp_32(index))) + ", " + register.mnemonic + "\\n\\t\"\n"

	segment_str, segment_chk_str = get_segment_prefix(segment, register, modrm)

	opcode_str = inst.opcode

	disp_str = generate_disp(modrm, disp)

	comment = "Test case " + str(tc_nr) + ": "
	comment += "SEG[" + segment_chk_str + "] "
	comment += "INSN: " + inst.name + "(" + register.name
	if (modrm_mod == 0):
		comment += "). "
	elif (modrm_mod == 1):
		comment += " + disp8). "
	elif (modrm_mod == 2):
		comment += " + disp32). "
	comment += "EFF_ADDR[" + str(my_hex(index + disp)) + "]. "
	comment += register.name + "[" + str(my_hex(index)) + "] "
	if (modrm_mod == 0):
			comment += ""
	elif (modrm_mod == 1):
			comment += "disp8[" + str(my_hex(disp)) + "]"
	elif (modrm_mod == 2):
			comment += "disp32[" + str(my_hex(disp)) + "]"

	code = "\t/* " + comment + " */\n"
	code += mov_reg_str
	code += code_start + segment_str + opcode_str + modrm_str + disp_str + code_end

	checkcode = generate_check_code(comment, segment_chk_str, index + disp, inst, TEST_PASS_CTR_VAR, TEST_FAIL_CTR_VAR)

	return code, checkcode, inst.result_bytes

def generate_code_sib(tc_nr, segment, inst, reg_base, reg_index, modrm_mod, sib_index, sib_base, sib_scale, disp, special=None):
	code_start = "\t\".byte "
	code_end = "\\n\\t\"\n"

	modrm = (modrm_mod << 6) | (inst.modrm_reg << 3) | ESP.modrm_rm
	sib = (sib_scale << 6) | (reg_index.modrm_rm << 3) | reg_base.modrm_rm
	modrm_str = ", " + str(my_hex(modrm))
	sib_str = ", " + str(my_hex(sib))
	mov_reg_str = "\t\"mov $" + str(my_hex(two_comp_32(sib_base))) + ", " + reg_base.mnemonic + "\\n\\t\"\n"
	mov_reg_str += "\t\"mov $" + str(my_hex(two_comp_32(sib_index))) + ", " + reg_index.mnemonic + "\\n\\t\"\n"

	if ((reg_index == ESP) or (reg_base == ESP)):
		backup_reg = find_backup_reg([reg_base, reg_index])
		backup_str = "\t\"mov " + ESP.mnemonic + ", " + backup_reg.mnemonic + "\\n\\t\"\n"
		restore_str = "\t\"mov " + backup_reg.mnemonic + ", " + ESP.mnemonic + "\\n\\t\"\n"
	else:
		backup_str = ""
		restore_str = ""

	segment_str, segment_chk_str = get_segment_prefix(segment, reg_base, modrm, sib)

	opcode_str = inst.opcode

	disp_str = generate_disp(modrm, disp, sib)

	if (special != None):
		comment = special + " Test case " + str(tc_nr) + ": "
	else:
		comment = "Test case " + str(tc_nr) + ": "
	comment += "SEG[" + segment_chk_str + "] "
	comment += "INSN: " + inst.name + " SIB(b:" + reg_base.name + " i:" + reg_index.name + " s:" + str(sib_scale)
	if (modrm_mod == 0):
		comment += "). "
	elif (modrm_mod == 1):
		comment += " + disp8). "
	elif (modrm_mod == 2):
		comment += " + disp32). "

	# ignore index value when computing effective address
	calc_index = sib_index
	calc_base = sib_base
	if (reg_index == ESP):
		calc_index = 0
	# ignore base
	if ((reg_base == EBP) and (modrm_mod == 0)):
		calc_base = 0

	eff_addr = calc_base + calc_index*(1<<sib_scale) + disp

	comment += "EFF_ADDR[" + str(my_hex(eff_addr)) + "]. "
	comment += 	"b[" + str(my_hex(sib_base)) + "] i[" + str(my_hex(sib_index)) + "] "
	if (modrm_mod == 0):
		if(reg_base == EBP):
			comment += "disp32[" + str(my_hex(disp)) + "]"
		else:
			comment += ""
	elif (modrm_mod == 1):
			comment += "disp8[" + str(my_hex(disp)) + "]"
	elif (modrm_mod == 2):
			comment += "disp32[" + str(my_hex(disp)) + "]"

	code = "\t/* " + comment + " */\n"
	code += backup_str
	code += mov_reg_str
	code += code_start + segment_str + opcode_str + modrm_str + sib_str + disp_str + code_end
	code += restore_str

	checkcode = generate_check_code(comment, segment_chk_str, eff_addr, inst, TEST_PASS_CTR_VAR, TEST_FAIL_CTR_VAR)

	return code, checkcode, inst.result_bytes

#TODO: ESP is a valid base but we need to make a backup before use.
def generate_unit_tests_sib(segment, inst, start_idx, start_tc_nr):
	index = start_idx
	testcase_nr = start_tc_nr
	code = ""
	check_code = ""

	for scale in range(0,4):
		for idx_reg in SIB_index:
			sib_base = index - 3*(1<<scale)
			sib_index = 3
			for base_reg in SIB_base_MO0:
				# while using the same register as base and index is not forbidden, it complicates our address calculation very greatly
				# hence, just skip this case
				if (base_reg == idx_reg):
					continue
				c, chk , i = generate_code_sib(testcase_nr, segment, inst, base_reg, idx_reg, MODRM_MO0, sib_index, sib_base, scale, 0)
				sib_base += i
				code += c
				check_code += chk
				testcase_nr += 1
			index = sib_base + sib_index*(1<<scale)

	for scale in range(0,4):
		for idx_reg in SIB_index:
			if (index > 127):
				disp = 0
			else:
				disp = index
				index = 0
			sib_base = index - 3*(1<<scale)
			sib_index = 3
			for base_reg in SIB_base_MO1:
				# while using the same register as base and index is not forbidden, it complicates our address calculation very greatly
				# hence, just skip this case
				if (base_reg == idx_reg):
					continue
				c, chk, i = generate_code_sib(testcase_nr, segment, inst, base_reg, idx_reg, MODRM_MO1, sib_index, sib_base, scale, disp)
				disp += i
				code += c
				check_code += chk
				testcase_nr += 1
			index = sib_base + sib_index*(1<<scale) + disp


	# the four scale values for MODRM_MOD=2 are split in several loops to
	# tests for various negative values
	# start here with negative indexes
	scale = 0
	disp = 0
	for idx_reg in SIB_index:
		sib_base = index - 3*(1<<scale)
		sib_index = 3

		# force a negative index
		sib_base += 100*(1<<scale)
		sib_index -= 100*(1<<scale)
		for base_reg in SIB_base_MO2:
			# while using the same register as base and index is not forbidden, it complicates our address calculation very greatly
			# hence, just skip this case
			if (base_reg == idx_reg):
				continue
			c, chk, i = generate_code_sib(testcase_nr, segment, inst, base_reg, idx_reg, MODRM_MO2, sib_index, sib_base, scale, disp)
			disp += i
			code += c
			check_code += chk
			testcase_nr += 1
		index = sib_base + sib_index*(1<<scale) + disp

	for scale in range(0,2):
		# use a negative displacement
		disp = -200
		for idx_reg in SIB_index:
			# offset here for the negative value of disp
			sib_base = index - 3*(1<<scale) + 200
			sib_index = 3
			for base_reg in SIB_base_MO2:
				# while using the same register as base and index is not forbidden, it complicates our address calculation very greatly
				# hence, just skip this case
				if (base_reg == idx_reg):
					continue
				c, chk, i = generate_code_sib(testcase_nr, segment, inst, base_reg, idx_reg, MODRM_MO2, sib_index, sib_base, scale, disp)
				disp += i
				code += c
				check_code += chk
				testcase_nr += 1
			index = sib_base + sib_index*(3<<scale) + disp

	for scale in range(3,4):
		# prepare use a negative base
		disp = index
		for idx_reg in SIB_index:
			sib_base = -3*(1<<scale)
			sib_index = 3
			for base_reg in SIB_base_MO2:
				# while using the same register as base and index is not forbidden, it complicates our address calculation very greatly
				# hence, just skip this case
				if (base_reg == idx_reg):
					continue
				c, chk, i = generate_code_sib(testcase_nr, segment, inst, base_reg, idx_reg, MODRM_MO2, sib_index, sib_base, scale, disp)
				disp += i
				code += c
				check_code += chk
				testcase_nr += 1
			index = sib_base + sib_index*(3<<scale) + disp

	return code, check_code, index, testcase_nr

def generate_special_unit_tests(start_tc_nr, segment, inst, start_idx):
	code = ""
	checkcode = ""
	index = start_idx
	tc_nr = start_tc_nr
	code_start = "\t\".byte "
	code_end = "\\n\\t\"\n"
	opcode_str = inst.opcode

	# MOD = 0, r/m = 5 (EBP), no index is used, only a disp32
	modrm = (MODRM_MO0 << 6) | (inst.modrm_reg << 3) | EBP.modrm_rm
	modrm_str = ", " + str(my_hex(modrm))

	segment_str, segment_chk_str = get_segment_prefix(segment, EBP, modrm)

	disp_str = ", " + str(my_hex(index & 0xff)) + ", "
	disp_str += str(my_hex((index >> 8)  & 0xff)) + ", "
	disp_str += str(my_hex((index >> 16) & 0xff)) + ", "
	disp_str += str(my_hex((index >> 24) & 0xff))

	comment = "Special Test case " + str(tc_nr) + ": "
	comment += "SEG[" + segment.name + "] "
	comment += "INSN: " + inst.name + " (disp32)."
	comment += "EFF_ADDR[" + str(my_hex(index)) + "]."
	comment += " disp32[" + str(my_hex(index)) + "]"

	code = "\t/* " + comment + " */\n"
	code += code_start + segment_str + opcode_str + modrm_str + disp_str +code_end

	checkcode += generate_check_code(comment, segment_chk_str, index, inst, TEST_PASS_CTR_VAR, TEST_FAIL_CTR_VAR)

	index += inst.result_bytes
	tc_nr += 1

	# With SIB, index is ignored if such index is ESP. This also means
	# that the scale is ignored
	for reg_base in SIB_base_MO0:
		c, chk , i = generate_code_sib(tc_nr, segment, inst, reg_base, ESP, MODRM_MO0, 0xffff, index, 3, 0, "Special")
		index += i
		code += c
		checkcode += chk
		tc_nr += 1

	disp = 0
	for reg_base in SIB_base_MO1:
		c, chk , i = generate_code_sib(tc_nr, segment, inst, reg_base, ESP, MODRM_MO1, 0xffff, index, 3, disp, "Special")
		disp += i
		code += c
		checkcode += chk
		tc_nr += 1

	index = index + disp
	disp = 0

	for reg_base in SIB_base_MO2:
		c, chk , i = generate_code_sib(tc_nr, segment, inst, reg_base, ESP, MODRM_MO2, 0xfff, index, 3, disp, "Special")
		disp += i
		code += c
		checkcode += chk
		tc_nr += 1

	index += disp

	# with SIB and base register as EBP and mod = 0, the base register is ignored and a disp32 is used. The default register is DS
	disp = 0
	for sib_scale in range (0,4):
		new_index = (index >> sib_scale) << sib_scale
		disp = index - new_index
		new_index >>= sib_scale
		for sib_index in SIB_base_MO0:
			c, chk, i = generate_code_sib(tc_nr, segment, inst, EBP, sib_index, MODRM_MO0, new_index, 0xeeee, sib_scale, disp, "Special")
			code += c
			disp += i
			checkcode += chk
			tc_nr += 1

	index += disp

	#with SIB, base as EBP and base as ESP, only disp32 is considered in the calculation
	c, chk, i = generate_code_sib(tc_nr, segment, inst, EBP, ESP, MODRM_MO0, 0xbbbb, 0xcccc, 3, index, "Special")
	code += c
	disp += i
	checkcode += chk
	tc_nr += 1

	index += i

	return code, checkcode, index, tc_nr

def generate_unit_tests(segment, inst, start_idx, start_tc_nr):
	code = ""
	check_code = ""
	index = start_idx
	testcase_nr = start_tc_nr

	code += "\t /* ==================== Test code for " + inst.name + " ==================== */\n"
	code += "\t\"test_umip_" + inst.name + "_" + segment.name + ":\\t\\n\"\n"

	check_code += "\n/* AUTOGENERATED CODE */\n"
	if (inst.result_bytes == 2):
		check_code += "static void check_tests_" + inst.name + "_" + segment.name + "(const unsigned short expected)\n"
		check_code += "{\n"
		check_code += "\tunsigned short got;\n\n"
	elif (inst.result_bytes == 6):
		check_code += "static void check_tests_" + inst.name + "_" + segment.name + "(const struct table_desc *expected)\n"
		check_code += "{\n"
		check_code += "\tstruct table_desc *got;\n\n"

	run_check_code = "\tcheck_tests_" + inst.name + "_" + segment.name + "(" + inst.expected_val + ");\n"
	check_code += "\tprintf(\"=======Results for " + inst.name + " in segment " + segment.name + "=============\\n\");\n"

	for reg in MO0:
		c, chk, idx = generate_code(testcase_nr, segment, inst, reg, MODRM_MO0, index, 0)
		code += c
		check_code += chk
		index += idx
		testcase_nr += 1

	# in signed 8-bit displacements, the max value is 255. Thus, correct, by adding
	# the remainder to index
	start_addr = index
	if (start_addr > 127):
		remainder = start_addr -127
		index = remainder
		start_addr = 127
	else:
		index = 0

	for reg in MO1:
		c, chk, idx = generate_code(testcase_nr, segment, inst, reg, MODRM_MO1, index, start_addr)
		code += c
		check_code += chk
		index += idx
		testcase_nr += 1

	start_addr += index
	index = 0

	# Force some indexes to be negative
	start_addr += 127
	index -=127

	for reg in MO2:
		c, chk, idx = generate_code(testcase_nr, segment, inst, reg, MODRM_MO2, index, start_addr)
		code += c
		check_code += chk
		index += idx
		testcase_nr += 1

	start_addr += index

	c, chk, start_addr, testcase_nr = generate_unit_tests_sib(segment, inst, start_addr, testcase_nr)
	code += c
	check_code += chk

	c, chk, idx, tc_nr = generate_special_unit_tests(testcase_nr, segment, inst, start_addr)
	code += c
	check_code += chk
	testcase_nr = tc_nr
	start_addr = idx

	code += "\t\"test_umip_" + inst.name + "_" + segment.name + "_end:\\t\\n\"\n"
	check_code += "}\n\n"

	return code, check_code, run_check_code, start_addr, testcase_nr

def generate_tests_all_insts(seg, start_index, start_test_nr):
	run_check_code = ""
	test_code = ""
	check_code = ""
	index = start_index
	test_nr = start_test_nr

	for inst in INSTS:
		tc, chkc, hdr, index, test_nr = generate_unit_tests(seg, inst, index, test_nr)
		run_check_code += hdr
		test_code += tc
		check_code += chkc

	return test_code, check_code, run_check_code, index, test_nr

def generate_test_cases(test_code, check_code):
	header_info = "/* ******************** AUTOGENERATED CODE ******************** */\n"
	header_info += "#define SEGMENT_SIZE " + str(SEGMENT_SIZE) + "\n"
	header_info += "#define CODE_MEM_SIZE " + str(CODE_MEM_SIZE) + "\n"
	header_info += "\n"
	header_info += "void check_results(void);\n"
	header_info += "\n"

	check_code += "/* ******************** AUTOGENERATED CODE ******************** */\n"
	check_code += "#include <stdio.h>\n"
	check_code += "#include \"test_umip_ldt_32.h\"\n\n"
	check_code += "#include \"umip_test_defs.h\"\n\n"
	check_code += "\n"
	check_code +="int " + TEST_PASS_CTR_VAR + ";\n"
	check_code +="int " + TEST_FAIL_CTR_VAR + ";\n"
	check_code +="int " + TEST_ERROR_CTR_VAR + ";\n"
	check_code += "\n"
	check_code += "unsigned char data[SEGMENT_SIZE];\n"
	check_code += "unsigned char data_es[SEGMENT_SIZE];\n"
	check_code += "unsigned char data_fs[SEGMENT_SIZE];\n"
	check_code += "unsigned char data_gs[SEGMENT_SIZE];\n"
	check_code += "unsigned char stack[SEGMENT_SIZE];\n"
	check_code += "\n"

	run_check_code = ""

	test_code += "\tasm(\n"
	test_code += "\t /* ******************** AUTOGENERATED CODE ******************** */\n"
	test_code += "\t\".pushsection .rodata\\n\\t\"\n"
	test_code += "\t\"test_umip:\\t\\n\"\n"
	test_code += "\t/* setup stack */\n"
	test_code += "\t\"mov $" + str(SEGMENT_SIZE) + ", %esp\\n\\t\"\n"
	test_code += "\t\"mov %ecx, %ss\\n\\t\"\n"
	test_code += "\t/* save old cs as passed by us before retf'ing here */\n"
	test_code += "\t\"push %edx\\n\\t\"\n"
	test_code += "\t/* save old esp as passed by us before retf'ing here */\n"
	test_code += "\t\"push %eax\\n\\t\"\n"
	test_code += "\t/* save old ss as passed by us before retf'ing here */\n"
	test_code += "\t\"push %ebx\\n\\t\"\n"


	test_nr = 0

	for seg in DATA_SEGS:
		index = 0
		tc, chkc, rchk, index, test_nr = generate_tests_all_insts(seg, index, test_nr)
		run_check_code += rchk
		test_code += tc
		check_code += chkc

	test_code += "\t/* preparing to return */\n"
	test_code += "\t/* restore ss */\n"
	test_code += "\t\"pop %ebx\\n\\t\"\n"
	test_code += "\t/* restore esp */\n"
	test_code += "\t\"pop %eax\\n\\t\"\n"
	test_code += "\t/* setting return IP, CS is already in stack */\n"
	test_code += "\t\"push $finish_testing\\n\\t\"\n"
	test_code += "\t\"retf\\n\\t\"\n"
	test_code += "\t\"test_umip_end:\\t\\n\"\n"
	test_code += "\t\".popsection\\n\\t\"\n"
	test_code += "\t);\n"

	check_code += "\n"
	check_code += "void check_results(void)\n"
	check_code += "{\n"
	check_code += run_check_code
	check_code += "}\n"
	check_code += "\n"

	return test_code, check_code, header_info, index

def parse_args():
	parser = argparse.ArgumentParser()
	parser.add_argument("--emulate-all", help="Test all UMIP-protected instructions. Otherwise, test code for STR and SLDT will not be generated.",
			    action="store_true")

	args = parser.parse_args()
	if args.emulate_all == False:
		print ("Test code will not be generated for instructions SLDT and STR")
		INSTS.remove(SLDT)
		INSTS.remove(STR)
	else:
		print ("Generate test code for all UMIP-protected instructions")

def write_test_files():
	parse_args()

	check_code = ""
	test_code = "/* This is an autogenerated file. If you intend to debug, better to debug the generating script. */\n\n"
	test_code += "\n"
	test_code, check_code, check_code_hdr, global_index = generate_test_cases(test_code, check_code)

	fcheck_code = open("test_umip_ldt_32.c", "w")
	fheader = open("test_umip_ldt_32.h", "w")
	ftest_code = open("test_umip_code_32.h", "w")
	ftest_code.writelines(test_code)
	fheader.writelines(check_code_hdr)
	fcheck_code.writelines(check_code)
	ftest_code.close()
	fcheck_code.close()
	fheader.close()

write_test_files()
