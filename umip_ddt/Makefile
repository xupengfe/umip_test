CC  = gcc

MAKE_TARGETS    := umip_test_opnds_64 umip_test_basic_64 \
                   umip_exceptions_64 umip_test_basic_32 umip_test_opnds_32 \
                   umip_exceptions_32 umip_ldt_32 umip_ldt_16 umip_ldt_64 \
                   umip_gp_test

$(all):
	$(CC) -o $@ $<

all: $(MAKE_TARGETS)


umip_test_opnds_64:
	$(CC) -no-pie -c src/umip/umip_utils.c -o umip_utils_64.o
	$(CC) -no-pie -o umip_test_opnds_64 umip_utils_64.o src/umip/umip_test_opnds.c

umip_test_basic_64:
	$(CC) -no-pie -o umip_test_basic_64 umip_utils_64.o src/umip/umip_test_basic.c

umip_exceptions_64:
	$(CC) -o umip_exceptions_64 umip_utils_64.o src/umip/umip_exceptions.c

umip_test_basic_32:
	$(CC) -no-pie -c src/umip/umip_utils.c -m32 -o umip_utils_32.o
	$(CC) -no-pie -m32 -o umip_test_basic_32 umip_utils_32.o src/umip/umip_test_basic.c

umip_test_opnds_32:
	$(CC) -m32 -o umip_test_opnds_32 umip_utils_32.o src/umip/umip_test_opnds.c

umip_exceptions_32:
	$(CC) -m32 -o umip_exceptions_32 umip_utils_32.o src/umip/umip_exceptions.c

umip_ldt_32:
	./src/umip/umip_test_gen_32.py
	$(CC) -m32 -c test_umip_ldt_32.c -I ./src/umip
	$(CC) -m32 -c src/umip/umip_ldt_32.c -I ./
	$(CC) -m32 -o umip_ldt_32 test_umip_ldt_32.o umip_ldt_32.o umip_utils_32.o

umip_ldt_16:
	./src/umip/umip_test_gen_16.py
	$(CC) -c src/umip/umip_utils.c -m32 -o umip_utils_16.o
	$(CC) -m32 -c test_umip_ldt_16.c -I ./src/umip
	$(CC) -m32 -c src/umip/umip_ldt_16.c -I ./
	$(CC) -m32 -o umip_ldt_16 test_umip_ldt_16.o umip_ldt_16.o umip_utils_16.o

umip_ldt_64:
	./src/umip/umip_test_gen_64.py
	$(CC) -c test_umip_ldt_64.c -I ./src/umip
	$(CC) -c src/umip/umip_ldt_64.c -I ./
	$(CC) -o umip_ldt_64 test_umip_ldt_64.o umip_ldt_64.o umip_utils_64.o

umip_gp_test: src/umip/umip_gp_test.c
	$(CC) -o umip_gp_test src/umip/umip_gp_test.c


clean:
	rm -f $(MAKE_TARGETS) *.o *.c *.h
