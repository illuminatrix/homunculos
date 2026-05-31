Illuminatrix
============

A minimal i386 (32-bit) kernel with ext2 filesystem, tmpfs, ATA PIO, PS/2
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
| `greeting` | Print "hello" (tests syscall read/write from ring 3) |
| `run <path>` | Fork and exec an ELF binary (e.g. `run /bin/hello`), child prints binary output, parent prints "exec: child N done" |
| `poweroff` | Shut down the VM |
| `cmd1 \| cmd2` | Pipe output of cmd1 to input of cmd2 |

Filesystem layout
-----------------

```
/           -- ext2 (read/write, from disk.img)
/dev        -- tmpfs (writable, populated at boot)
/dev/vga    -- VGA text-mode output
/dev/kbd    -- PS/2 keyboard input
/dev/vgaerr -- VGA stderr
/dev/hello  -- tmpfs demo file ("hello fs")
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
make -C tests test-tmpfs
make -C tests test-ext2
make -C tests test-ext2-write
```

Clean
-----

```
make clean      # remove all build artifacts
```
