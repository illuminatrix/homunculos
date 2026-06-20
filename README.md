HomunculOS
==========

A minimal i386 (32-bit) kernel with ext2 filesystem, devtmpfs, ATA PIO, PS/2
keyboard, VGA text-mode display, multitasking, ELF execution, and an
interactive shell — all running on bare metal in QEMU.

Building
--------

```
make kernel.bin
```

Running
-------

```
make disk    # create a 32MB ext2 disk image (disk.img)
make run     # launch QEMU with curses display
```

Building the disk image is automatic — `make run` depends on it.

Commands
--------

Once the shell prompt (`>`) appears, you can type:

| Command | Description |
|---------|-------------|
| `ls [path]` | List directory contents |
| `cat <file>` | Print file contents to screen |
| `touch <path>` | Create empty file |
| `write <path> <content...>` | Write text to a file |
| `mkdir <path>` | Create a directory |
| `rm <path>` | Remove a file (unlink) |
| `ln -s <target> <link>` | Create a symbolic link |
| `readlink <path>` | Print symlink target |
| `cd [path]` | Change current directory |
| `pwd` | Print current directory |
| `stat <path>` | Show file inode metadata |
| `uname` | Print kernel name, version, arch |
| `sleep <seconds>` | Sleep for N seconds |
| `systime` | Print seconds since boot |
| `echo <args...>` | Print arguments to stdout |
| `greeting` | Print "hello" (tests syscall read/write from ring 3) |
| `run <path>` | Fork and exec an ELF binary (e.g. `run /bin/hello`), child prints binary output, parent prints "exec: child N done" |
| `poweroff` | Shut down the VM |
| `env` | Print environment variables |
| `kill <pid> <signum>` | Send a signal to a process |
| `trap <signum> [kill]` | Test signal handling |
| `trap <signum>` | Catch a signal and print "CAUGHT" |
| `sync` | Flush filesystem buffers (no-op) |
| `times` | Print process times (ticks) |
| `chmod <octal> <path>` | Change file permissions |
| `mv <old> <new>` | Rename/move a file |

Filesystem layout
-----------------

```
/           -- ext2 (read/write, from disk.img)
/dev        -- devtmpfs (writable, populated at boot)
/dev/vga    -- VGA text-mode output
/dev/kbd    -- PS/2 keyboard input
/dev/vgaerr -- VGA stderr
/dev/hello  -- devtmpfs demo file ("hello fs")
/dev/null   -- writes discarded, reads return EOF
```

Disk image
----------

The disk image is a 32MB ext2 revision 0 filesystem. You can populate it
with `debugfs`:

```
debugfs -w disk.img -R "mkdir mydir"
debugfs -w disk.img -R "write myfile.txt /mydir/myfile.txt"
```

Debugging
---------

```
make run-debug  # launch QEMU paused, waiting for GDB on :1234
make debug      # generate kernel.lst (disassembly listing)
make gdb        # connect GDB
make quit       # stop QEMU
```

Testing
-------

```
make test       # run all integration tests
```

Or run a single test:

```
make -C tests test-boot
make -C tests test-vga
make -C tests test-shell
make -C tests test-usermode
make -C tests test-mmap
make -C tests test-multiboot
make -C tests test-devtmpfs
make -C tests test-ext2
make -C tests test-ext2-write
make -C tests test-time
make -C tests test-signal
make -C tests test-null
make -C tests test-stat
make -C tests test-execv
make -C tests test-chdir
make -C tests test-ioctl
make -C tests test-pipe
make -C tests test-waitpid
make -C tests test-sync
make -C tests test-times
make -C tests test-chmod
make -C tests test-mv
make -C tests test-dup
make -C tests test-mmap2
```

Clean
-----

```
make clean      # remove all build artifacts
```
