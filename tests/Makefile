TESTS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
KERNEL_DIR := $(TESTS_DIR)/..
KERNEL_BIN := $(KERNEL_DIR)/kernel.bin
SHELL_BIN := $(KERNEL_DIR)/shell/shell
INIT_ELF := $(KERNEL_DIR)/init/init
BIN_CMDS := shell/bin/poweroff shell/bin/greeting shell/bin/uname shell/bin/ls shell/bin/cat shell/bin/stat shell/bin/hello shell/bin/pwd shell/bin/cd
BIN_CMDS := $(addprefix $(KERNEL_DIR)/,$(BIN_CMDS))
DISK_IMG := $(TESTS_DIR)/ext2-disk.img

CC := gcc
LD := ld
CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 \
	  -fno-stack-protector -mno-sse \
	  -I$(KERNEL_DIR)/libc/include -I$(KERNEL_DIR)/kernel -I$(KERNEL_DIR)/shell

IOCTL_TEST_BIN := $(TESTS_DIR)/ioctl_test.elf
PIPE_TEST_BIN := $(TESTS_DIR)/prepend.elf

TEST_SCRIPTS := test-multiboot test-boot test-panic test-panic-init test-vga test-shell test-usermode test-mmap test-exec test-tmpfs test-ext2 test-stat test-execv test-chdir test-ioctl test-pipe test-waitpid
TEST_SH := $(addprefix $(TESTS_DIR)/,$(addsuffix .sh,$(TEST_SCRIPTS)))
DISK_TESTS := test-vga test-shell test-usermode test-mmap test-exec test-tmpfs test-ext2 test-stat test-execv test-chdir test-ioctl test-pipe test-waitpid

.PHONY: all test $(TEST_SCRIPTS) clean

all: test

$(KERNEL_BIN):
	@echo "Building kernel..."
	@$(MAKE) -C $(KERNEL_DIR) 2>&1

$(DISK_IMG): $(KERNEL_BIN) $(SHELL_BIN) $(INIT_ELF) $(BIN_CMDS) $(PIPE_TEST_BIN)
	@echo "Building shared disk image..."
	@$(TESTS_DIR)/build-disk.sh

test: $(KERNEL_BIN) $(DISK_IMG) $(TEST_SCRIPTS)

test-multiboot: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-multiboot.sh

test-boot: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-boot.sh

test-panic: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-panic.sh

test-panic-init: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-panic-init.sh

test-vga: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-vga.sh

test-shell: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-shell.sh

test-usermode: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-usermode.sh

test-mmap: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-mmap.sh

test-exec: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-exec.sh

test-tmpfs: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-tmpfs.sh

test-ext2: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-ext2.sh

test-stat: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-stat.sh

test-execv: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-execv.sh

test-chdir: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-chdir.sh

$(IOCTL_TEST_BIN): $(TESTS_DIR)/ioctl_test.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

$(PIPE_TEST_BIN): $(TESTS_DIR)/prepend.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

test-ioctl: $(KERNEL_BIN) $(IOCTL_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-ioctl.sh

test-pipe: $(KERNEL_BIN) $(PIPE_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-pipe.sh

test-waitpid: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-waitpid.sh

clean:
	rm -f $(TESTS_DIR)/*.sock
	rm -f $(KERNEL_DIR)/serial.log
	rm -f $(IOCTL_TEST_BIN) $(PIPE_TEST_BIN)
	rm -f $(DISK_IMG)

.PHONY: clean
