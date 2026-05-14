#
#
CC := gcc
AS := as
LD := ld
ARCH := i386
OBJDUMP := /usr/bin/objdump
CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector -Ilibc/include -Ikernel -Ishell
ASFLAGS := -32
include arch/Makefile.mk
include kernel/Makefile.mk
include shell/Makefile.mk
include drivers/Makefile.mk
QEMU_CMD := qemu-system-i386 -kernel kernel.bin -display curses -serial file:serial.log -monitor unix:qemu-monitor.sock,server,nowait


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

run: kernel.bin
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

.PHONY: clean
clean:
	cd libc && make clean
	find drivers -name '*.o' -delete
	rm -f *.o arch/$(ARCH)/*o kernel/*.o shell/*.o *.bin *.lst serial.log qemu-monitor.sock
