SHELL_ELF := shell/shell
SHELL_EMBED := shell/shell_embed.o

$(SHELL_ELF): shell/shell_elf.c
	cd libc && make
	@echo "CC shell/shell_elf.c    ->    shell/shell_elf.o"
	@$(CC) -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector -mno-sse -Ilibc/include -c $< -o shell/shell_elf.o
	@echo "LD shell/shell_elf.o    ->    $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ shell/shell_elf.o libc/stdio/*.o libc/string/*.o libc/unistd/*.o

$(SHELL_EMBED): $(SHELL_ELF)
	@echo "EMBED $^    ->    $@"
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 $^ $@

OBJS += $(SHELL_EMBED)
