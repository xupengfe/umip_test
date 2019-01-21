
LDFLAGS         += -fno-omit-frame-pointer
CETFLAGS        = -fcf-protection=full -fno-stack-check -fno-stack-protector

GCC_VER_MAJOR := $(shell gcc --version | grep gcc | cut -d . -f1 | awk '{print $$NF}')
GCC_GE_8 := $(shell [ $(GCC_VER_MAJOR) -ge 8 ] && echo true)

MAKE_TARGETS  := umip_test umip_test_basic_64

ifeq ($(GCC_GE_8),true)
MAKE_TARGETS += umip_test_basic_64_cet
else
$(info Warning: GCC major ${GCC_VER_MAJOR}, older than GCC 8)
$(info Warning: Will not handle umip_test_basic_64_cet)
endif

all: $(MAKE_TARGETS)

$(all):
	$(CC) -o $@ $<

umip_test:
	$(CC) $(LDFLAGS) umip_test.c -o umip_test

umip_test_basic_64:
	$(CC) -c umip_utils.c -o umip_utils_64.o
	$(CC) -o umip_test_basic_64 umip_utils_64.o umip_test_basic.c

umip_test_basic_64_cet:
	$(CC) $(CETLFLAGS) -c umip_utils.c -o umip_utils_64_cet.o
	$(CC) $(CETLFLAGS) -o umip_test_basic_64_cet umip_utils_64_cet.o umip_test_basic.c

clean:
	rm -f umip_test umip_test_basic_64 umip_test_basic_64_cet *.o
