CFLAGS := -g -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector
FREEOBJS:=\
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
unistd/join.o \
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

OBJS:=\
$(FREEOBJS) \


.PHONY: all clean

all: $(OBJS)

%.o: %.c
	$(CC) -Iinclude -I../include -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(BINARIES) $(OBJS) *.o */*.o */*/*.o
