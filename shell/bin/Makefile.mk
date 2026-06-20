BIN_CMDS := shell/bin/poweroff shell/bin/reboot shell/bin/greeting shell/bin/uname shell/bin/ls shell/bin/cat shell/bin/stat shell/bin/hello shell/bin/pwd shell/bin/cd shell/bin/echo shell/bin/touch shell/bin/write shell/bin/mkdir shell/bin/rm shell/bin/ln shell/bin/readlink shell/bin/sleep shell/bin/systime shell/bin/env shell/bin/kill shell/bin/trap shell/bin/sync shell/bin/times shell/bin/chmod shell/bin/mv shell/bin/fchmod shell/bin/mknod shell/bin/truncate shell/bin/cp shell/bin/dd

$(BIN_CMDS): shell/bin/%: shell/bin/%.c
	cd libc && make
	@echo "CC $<    ->    shell/bin/$*.o"
	@$(CC) $(CFLAGS) -c $< -o shell/bin/$*.o
	@echo "LD shell/bin/$*.o    ->    $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ libc/_start.o shell/bin/$*.o libc/stdio/*.o libc/string/*.o libc/stdlib/*.o libc/unistd/*.o
