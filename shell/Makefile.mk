SHELL_ELF := shell/shell

$(SHELL_ELF): shell/shell_elf.c
	cd libc && make
	@echo "CC shell/shell_elf.c    ->    shell/shell_elf.o"
	@$(CC) -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector -mno-sse -Ilibc/include -c $< -o shell/shell_elf.o
	@echo "LD shell/shell_elf.o    ->    $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ libc/_start.o shell/shell_elf.o libc/stdio/*.o libc/string/*.o libc/stdlib/*.o libc/unistd/*.o
