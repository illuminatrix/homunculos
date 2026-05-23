#
#
CC := gcc
AS := as
LD := ld
ARCH := i386
OBJDUMP := /usr/bin/objdump
OBJCOPY := objcopy
CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector -mno-sse -Ilibc/include -Ikernel -Ishell
ASFLAGS := -32
include arch/Makefile.mk
include kernel/Makefile.mk
include shell/Makefile.mk
include drivers/Makefile.mk
DISK_IMG := disk.img
QEMU_CMD := qemu-system-i386 -kernel kernel.bin -drive file=$(DISK_IMG),format=raw,if=ide -display curses -serial file:serial.log -monitor unix:qemu-monitor.sock,server,nowait


%.o: %.S
	@echo "AS $^    ->     $@"
	@$(AS) $(ASFLAGS) -I arch/$(ARCH)/ -I . -c -o $@ $^

%.o: %.c
	@echo "CC $^    ->    $@"
	@$(CC) $(CFLAGS) -I arch/$(ARCH)/ -I . -c -o $@ $^

kernel.bin: $(OBJS)
	cd libc && make
	@echo "LD $^    ->    $@"
	$(LD) -T arch/$(ARCH)/kernel.ld -melf_i386 -o $@ libc/stdio/*.o libc/string/*.o libc/unistd/*.o $^

debug: kernel.bin
	$(OBJDUMP) -lSdx kernel.bin > kernel.lst

run: kernel.bin $(DISK_IMG)
	$(QEMU_CMD)

run-debug: kernel.bin
	$(QEMU_CMD) -S -s

gdb:
	gdb -ix gdb_script.gdb

.PHONY: test clean quit

test: kernel.bin
	$(MAKE) -C tests test

quit:
	echo quit | socat - UNIX-CONNECT:qemu-monitor.sock 2>/dev/null || echo "VM not running"

DISK_SIZE_MB ?= 32

$(DISK_IMG):
	dd if=/dev/zero of=$@ bs=1M count=$(DISK_SIZE_MB) 2>/dev/null
	# Create ext2 partition image with /dev directory
	dd if=/dev/zero of=.part.img bs=1M count=31 2>/dev/null
	mkfs.ext2 -F -E revision=0 -b 1024 .part.img 2>/dev/null
	debugfs -w .part.img -R "mkdir /dev" 2>/dev/null
	# Create MBR partition table on disk image
	printf '2048,,L,*\n' | sfdisk $@ 2>/dev/null
	# Write ext2 partition at sector 2048 (1MB offset)
	dd if=.part.img of=$@ bs=512 seek=2048 conv=notrunc 2>/dev/null
	rm -f .part.img

disk: $(DISK_IMG)

.PHONY: clean
clean:
	cd libc && make clean
	find drivers -name '*.o' -delete
	rm -f *.o arch/$(ARCH)/*o kernel/*.o shell/*.o *.bin *.lst serial.log qemu-monitor.sock $(DISK_IMG)
