TESTS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
KERNEL_DIR := $(TESTS_DIR)/..
KERNEL_BIN := $(KERNEL_DIR)/kernel.bin

CC := gcc
LD := ld
CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 \
	  -fno-stack-protector -mno-sse \
	  -I$(KERNEL_DIR)/libc/include -I$(KERNEL_DIR)/kernel -I$(KERNEL_DIR)/shell

IOCTL_TEST_BIN := $(TESTS_DIR)/ioctl_test.elf

TEST_SCRIPTS := test-multiboot test-boot test-panic test-panic-init test-vga test-shell test-usermode test-mmap test-exec test-tmpfs test-ext2 test-stat test-execv test-chdir test-ioctl
TEST_SH := $(addprefix $(TESTS_DIR)/,$(addsuffix .sh,$(TEST_SCRIPTS)))

.PHONY: all test $(TEST_SCRIPTS) clean

all: test

$(KERNEL_BIN):
	@echo "Building kernel..."
	@$(MAKE) -C $(KERNEL_DIR) 2>&1

test: $(KERNEL_BIN) $(TEST_SCRIPTS)

test-multiboot: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-multiboot.sh

test-boot: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-boot.sh

test-panic: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-panic.sh

test-panic-init: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-panic-init.sh

test-vga: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-vga.sh

test-shell: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-shell.sh

test-usermode: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-usermode.sh

test-mmap: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-mmap.sh

test-exec: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-exec.sh

test-tmpfs: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-tmpfs.sh

test-ext2: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-ext2.sh

test-stat: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-stat.sh

test-execv: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-execv.sh

test-chdir: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-chdir.sh

$(IOCTL_TEST_BIN): $(TESTS_DIR)/ioctl_test.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

test-ioctl: $(KERNEL_BIN) $(IOCTL_TEST_BIN)
	@$(TESTS_DIR)/test-ioctl.sh

clean:
	rm -f $(TESTS_DIR)/*.sock
	rm -f $(KERNEL_DIR)/serial.log
	rm -f $(IOCTL_TEST_BIN)

.PHONY: clean
