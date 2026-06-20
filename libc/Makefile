CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector
FREEOBJS:=\
_start.o \
stdlib/env.o \
stdio/printf.o \
stdio/vsprintf.o \
stdio/putchar.o \
string/memcmp.o \
string/memcpy.o \
string/memmove.o \
string/memset.o \
string/strlen.o \
string/strncpy.o \
string/strcmp.o \
string/strncmp.o \
string/strcpy.o \
unistd/fork.o \
unistd/read.o \
unistd/open.o \
unistd/close.o \
unistd/exec.o \
unistd/wait.o \
unistd/exit.o \
unistd/reboot.o \
unistd/mount.o \
unistd/unmount.o \
unistd/yield.o \
unistd/getpid.o \
unistd/getppid.o \
unistd/uname.o \
unistd/lseek.o \
unistd/brk.o \
unistd/stat.o \
unistd/lstat.o \
unistd/fstat.o \
unistd/getdents.o \
unistd/chdir.o \
unistd/getcwd.o \
unistd/ioctl.o \
unistd/pipe.o \
unistd/dup2.o \
unistd/access.o \
unistd/mkdir.o \
unistd/rmdir.o \
unistd/unlink.o \
unistd/symlink.o \
unistd/readlink.o \
unistd/write.o \
unistd/time.o \
unistd/gettimeofday.o \
unistd/nanosleep.o \
unistd/sleep.o \
unistd/sync.o \
unistd/fsync.o \
unistd/dup.o \
unistd/chmod.o \
unistd/fchmod.o \
unistd/link.o \
unistd/fcntl.o \
unistd/times.o \
unistd/mknod.o \
unistd/rename.o \
unistd/truncate64.o \
unistd/ftruncate64.o \
unistd/mmap.o \
unistd/munmap.o \
unistd/mprotect.o \

OBJS:=\
$(FREEOBJS) \


.PHONY: all clean

all: $(OBJS)

%.o: %.c
	$(CC) -Iinclude -I../include -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(BINARIES) $(OBJS) *.o */*.o */*/*.o
