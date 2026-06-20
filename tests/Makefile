TESTS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
KERNEL_DIR := $(TESTS_DIR)/..
KERNEL_BIN := $(KERNEL_DIR)/kernel.bin
SHELL_BIN := $(KERNEL_DIR)/shell/shell
INIT_ELF := $(KERNEL_DIR)/init/init
BIN_CMDS := shell/bin/poweroff shell/bin/reboot shell/bin/greeting shell/bin/uname shell/bin/ls shell/bin/cat shell/bin/stat shell/bin/hello shell/bin/pwd shell/bin/cd shell/bin/echo shell/bin/touch shell/bin/write shell/bin/mkdir shell/bin/rm shell/bin/ln shell/bin/readlink shell/bin/sleep shell/bin/systime shell/bin/env shell/bin/kill shell/bin/trap shell/bin/sync shell/bin/times shell/bin/chmod shell/bin/mv shell/bin/fchmod shell/bin/mknod shell/bin/truncate shell/bin/cp shell/bin/dd
BIN_CMDS := $(addprefix $(KERNEL_DIR)/,$(BIN_CMDS))
DISK_IMG := $(TESTS_DIR)/ext2-disk.img

CC := gcc
LD := ld
CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 \
	  -fno-stack-protector -mno-sse \
	  -I$(KERNEL_DIR)/libc/include -I$(KERNEL_DIR)/kernel -I$(KERNEL_DIR)/shell

IOCTL_TEST_BIN := $(TESTS_DIR)/ioctl_test.elf
PIPE_TEST_BIN := $(TESTS_DIR)/prepend.elf
SIGNAL_TEST_BIN := $(TESTS_DIR)/signal_test.elf
DUP_TEST_BIN := $(TESTS_DIR)/dup_test.elf
FCNTL_TEST_BIN := $(TESTS_DIR)/fcntl_test.elf
MMAP_TEST_BIN := $(TESTS_DIR)/mmap_test.elf

TEST_SCRIPTS := test-multiboot test-boot test-panic test-panic-init test-vga test-shell test-usermode test-mmap test-exec test-devtmpfs test-ext2 test-ext2-write test-stat test-execv test-chdir test-ioctl test-pipe test-waitpid test-time test-env test-signal test-null test-sync test-times test-chmod test-mv test-dup test-fchmod test-link test-fcntl test-mknod test-truncate test-mmap2
TEST_SH := $(addprefix $(TESTS_DIR)/,$(addsuffix .sh,$(TEST_SCRIPTS)))
DISK_TESTS := test-vga test-shell test-usermode test-mmap test-exec test-devtmpfs test-ext2 test-stat test-execv test-chdir test-ioctl test-pipe test-waitpid test-time test-signal test-null test-sync test-times test-chmod test-mv test-dup test-fchmod test-link test-fcntl test-mknod test-mmap2

.PHONY: all test $(TEST_SCRIPTS) clean

all: test

$(KERNEL_BIN):
	@echo "Building kernel..."
	@$(MAKE) -C $(KERNEL_DIR) kernel.bin 2>&1

$(DISK_IMG): $(KERNEL_BIN) $(SHELL_BIN) $(INIT_ELF) $(PIPE_TEST_BIN) $(SIGNAL_TEST_BIN) $(DUP_TEST_BIN) $(FCNTL_TEST_BIN) $(MMAP_TEST_BIN) $(BIN_CMDS)
	@echo "Building shared disk image..."
	@$(MAKE) -C $(KERNEL_DIR) $(patsubst $(KERNEL_DIR)/%,%,$(BIN_CMDS)) 2>&1
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

test-devtmpfs: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-devtmpfs.sh

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
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(KERNEL_DIR)/libc/_start.o $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/stdlib/*.o $(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

$(PIPE_TEST_BIN): $(TESTS_DIR)/prepend.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(KERNEL_DIR)/libc/_start.o $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/stdlib/*.o $(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

$(SIGNAL_TEST_BIN): $(TESTS_DIR)/signal_test.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(KERNEL_DIR)/libc/_start.o $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/stdlib/*.o $(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

$(DUP_TEST_BIN): $(TESTS_DIR)/dup_test.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(KERNEL_DIR)/libc/_start.o $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/stdlib/*.o $(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

$(FCNTL_TEST_BIN): $(TESTS_DIR)/fcntl_test.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(KERNEL_DIR)/libc/_start.o $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/stdlib/*.o $(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

test-ioctl: $(KERNEL_BIN) $(IOCTL_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-ioctl.sh

test-pipe: $(KERNEL_BIN) $(PIPE_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-pipe.sh

test-ext2-write: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-ext2-write.sh

test-waitpid: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-waitpid.sh

test-time: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-time.sh

test-env: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-env.sh

test-signal: $(KERNEL_BIN) $(SIGNAL_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-signal.sh

test-null: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-null.sh

test-sync: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-sync.sh

test-times: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-times.sh

test-chmod: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-chmod.sh

test-mv: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-mv.sh

test-dup: $(KERNEL_BIN) $(DUP_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-dup.sh

test-fchmod: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-fchmod.sh

test-link: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-link.sh

test-fcntl: $(KERNEL_BIN) $(FCNTL_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-fcntl.sh

test-mknod: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-mknod.sh

test-truncate: $(KERNEL_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-truncate.sh

$(MMAP_TEST_BIN): $(TESTS_DIR)/mmap_test.c
	@echo "CC  $<  ->  $@"
	@$(CC) $(CFLAGS) -c -o $@.o $<
	@echo "LD  $@.o ->  $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(KERNEL_DIR)/libc/_start.o $@.o \
		$(KERNEL_DIR)/libc/stdio/*.o $(KERNEL_DIR)/libc/string/*.o \
		$(KERNEL_DIR)/libc/stdlib/*.o $(KERNEL_DIR)/libc/unistd/*.o
	@rm -f $@.o

test-mmap2: $(KERNEL_BIN) $(MMAP_TEST_BIN) $(DISK_IMG)
	@$(TESTS_DIR)/test-mmap2.sh

clean:
	rm -f $(TESTS_DIR)/*.sock
	rm -f $(KERNEL_DIR)/serial.log
	rm -f $(IOCTL_TEST_BIN) $(PIPE_TEST_BIN) $(SIGNAL_TEST_BIN) $(DUP_TEST_BIN) $(FCNTL_TEST_BIN) $(MMAP_TEST_BIN)
	rm -f $(DISK_IMG)

.PHONY: clean
