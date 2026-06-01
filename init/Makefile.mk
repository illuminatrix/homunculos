INIT_ELF := init/init

$(INIT_ELF): init/init_elf.c
	cd libc && make
	@echo "CC init/init_elf.c    ->    init/init_elf.o"
	@$(CC) $(CFLAGS) -c $< -o init/init_elf.o
	@echo "LD init/init_elf.o    ->    $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ libc/_start.o init/init_elf.o libc/stdio/*.o libc/string/*.o libc/stdlib/*.o libc/unistd/*.o
