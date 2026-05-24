# Illuminatrix -- Agent Context

## Build System

- **Compiler**: `gcc -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -Wextra -m32 -fno-stack-protector -mno-sse`
- **Assembler**: `as --32`
- **Linker**: `ld -melf_i386 -T arch/i386/kernel.ld`
- **Output**: `kernel.bin` (Multiboot flat binary, loaded at 1MB)
- **Include path**: `-Ilibc/include -Ikernel -Ishell -Iarch/i386/`
- **Build**: flat `OBJS` accumulated via `include` chains of `Makefile.mk` files. libc is built separately via `cd libc && make` during link step.

### Make Targets

| Target | Action |
|--------|--------|
| `make kernel.bin` | Build `kernel.bin` (default target `make` only builds `hello.elf`) |
| `make test` | Runs all QEMU integration tests |
| `make run` | `qemu-system-i386 -kernel kernel.bin -drive file=disk.img,format=raw,if=ide -display curses -serial file:serial.log -monitor unix:qemu-monitor.sock,server,nowait` |
| `make run-debug` | Same + `-S -s` (pause, wait for GDB on :1234) |
| `make debug` | Generate `kernel.lst` disassembly |
| `make gdb` | `gdb -ix gdb_script.gdb` (connects to :1234) |
| `make disk` | Create 32MB ext2 rev-0 disk image (`disk.img`) |
| `make clean` | Remove all `.o`, `.bin`, `.lst`, `serial.log`, `qemu-monitor.sock`, `disk.img` |
| `make quit` | Send `quit` to QEMU monitor via socat |

### Adding new files

1. Append `<path/to/file.o>` to the appropriate `Makefile.mk`
2. `drivers/Makefile.mk` includes sub-driver makefiles; add an `include` line there when adding a driver

### Before committing

Update `README.md` and `TODO.txt` if the change affects the feature list
or the porting roadmap.

### Committing

Use `git commit -s` and append `Assisted-by: big-pickle:opencode/big-pickle` trailer.

## Init Sequence

```
_start (arch/i386/kernel_head.S)
  -> kernel_main(multiboot_info_t*)
     1. syscall_init()         -- populate systemcall_table[255]
     2. pic_init()             -- 8259 PIC, remap IRQs to 0x20-0x2F
     3. load_idt()             -- 255-entry IDT, DPL=3 for 0x31 (yield) & 0x80 (syscall)
     4. vfs_init()             -- iterate .driver_init, init VGA + PS/2
     5. vfs_inode_init()       -- init VFS inode pool
     6. tmpfs_init()           -- bootstrap tmpfs root inode, create /dev + /mnt dirs
     7. vfs_create_device_nodes() + hello_driver_init()
                               -- populate bootstrap /dev (needed for early printf)
     8. welcome()              -- printf("Illuminatrix Kernel!\n")
     9. init_mm(mmap, len)     -- identity paging 16MB, frame bitmap from multiboot map
    10. gdt_init()             -- GDT: ring 0/3 code+data + TSS
    11. ata_init()             -- ATA PIO detection (registers block devices)
    12. ext2_mount(block_dev)  -- mount ext2 from detected block device
    13. vfs_mount_create(vfs_root_inode(), ext2_root)
                               -- mount ext2 at /
    14. vfs_resolve_path("/dev") -> tmpfs_create_mount() -> vfs_mount_create()
                               -- mount tmpfs at /dev
    15. vfs_create_device_nodes() + hello_driver_init() (2nd call)
                               -- populate final tmpfs /dev
    16. scheduler_init()       -- PIT at 100Hz, callback = scheduler_tick
    17. setup_main_task(...)   -- create shell task (user context), fds via /dev/kbd/vga/vgaerr
    18. pic_enable_irq(0)      -- timer
    19. pic_enable_irq(1)      -- keyboard
    20. while(1) hlt
```

## Kernel Architecture

All structs are fully defined in `.h` files (no opaque structs). No dynamic allocation -- static arrays and fixed pools. No create/destroy -- init functions on static globals.

### Memory Management

- Identity paging: 16MB (4 page tables, 4MB each) in `kernel_pdir`/`kernel_pt`
- Frame allocator: bitmap over 4096 frames (16MB), linear scan, `mm_frame_alloc()` / `mm_frame_free()`
- Kernel pages reserved up to `__kernel_end` (BSS end symbol from linker script)
- `mm_map_at(pdir, va, pa, flags)` -- map an existing physical page at arbitrary VA in any pdir. Allocates page tables on demand. Uses `invlpg` if mapping in current pdir.
- `mm_alloc_at(pdir, va, flags)` -- allocates a frame + maps it at VA
- `mm_clone_pdir(parent)` -- deep-copies entire page table tree (each page frame copied via memcpy) for fork
- Kernel pdir/page tables use `0x7` (Present+RW+User). User mappings use `MM_USER | MM_RW | MM_PRESENT`.

### GDT / User Mode

- 6 entries: null(0), ring 0 code(1), ring 0 data(2), ring 3 code(3), ring 3 data(4), TSS(5)
- Selectors: `GDT_KERNEL_CODE=0x08`, `GDT_KERNEL_DATA=0x10`, `GDT_USER_CODE=0x1B`, `GDT_USER_DATA=0x23`, `GDT_TSS=0x28`
- TSS.ESP0 updated per-task in `schedule()` when switching to a user task
- `task_init_user_context()` sets up user stack + iretl frame (SS, ESP, EFLAGS, CS, EIP) for ring 3 entry via `context_restore_user` (which uses `iretl`)

### Important: fork() + user stack isolation

ELF-loaded user tasks MUST have their page directory cloned on fork (`mm_clone_pdir`)
so the child gets its own copy of user pages (including the user stack). Without this,
the child overwrites the parent's stack variables (e.g., overwriting the fork return
value `pid` from 3 to 0), causing the parent to misbehave after the child exits.

`task_fork` in `kernel/task.c` calls `mm_clone_pdir` for `is_user` tasks. `mm_clone_pdir`
in `arch/i386/mm.c` shares kernel identity page tables (first 4 PDEs, 0-16MB) and
deep-copies only user page table entries (PDE index >= 4). Each user page frame is
allocated and copied via `memcpy`.

### Interrupt Model

- IDT: 255 entries, 32-bit interrupt gates (type=0xE), seg_sel=0x08
- Vectors 0-31: CPU exceptions -- default handlers spin on halt
- Vectors 32-47: IRQs 0-15 (PIC remapped) -- dispatch via registered handler + EOI
- Vector 0x31: yield (software) -- DPL=3, calls `schedule()`
- Vector 0x80: syscall (software) -- DPL=3, dispatches via `systemcall_table[%eax]`
- PIC: master 0x20, slave 0xA0. EOI = `out(0x20, 0x20)`; for slave IRQs (8-15) also `out(0xA0, 0x20)`
- After `pic_init()`, all IRQs are masked. Must call `pic_enable_irq()` for each.

### Syscalls

Syscall numbers follow Linux i386 conventions.
Reference: https://faculty.nps.edu/cseagle/assembly/sys_call.html

| # | Name | Handler | Signature |
|---|------|---------|-----------|
| 1 | SYS_exit | `sys_exit` | `int sys_exit(int status)` |
| 2 | SYS_fork | `sys_fork` | `int sys_fork(void)` |
| 3 | SYS_read | `sys_read` | `int sys_read(int fd, void *buf, size_t len)` |
| 4 | SYS_write | `sys_write` | `int sys_write(int fd, const void *buf, size_t len)` |
| 5 | SYS_open | `sys_open` | `int sys_open(const char *path, int flags)` |
| 6 | SYS_close | `sys_close` | `int sys_close(int fd)` |
| 7 | SYS_waitpid | `sys_waitpid` | `int sys_waitpid(int pid, int *status, int options)` |
| 11 | SYS_execve | `sys_exec` | `int sys_exec(const void *elf_buf)` |
| 19 | SYS_lseek | `sys_lseek` | `int sys_lseek(int fd, int offset, int whence)` |
| 20 | SYS_getpid | `sys_getpid` | `int sys_getpid(void)` |
| 45 | SYS_brk | `sys_brk` | `int sys_brk(void *addr)` |
| 106 | SYS_stat | `sys_stat` | `int sys_stat(const char *path, struct stat *buf)` |
| 107 | SYS_lstat | `sys_lstat` | `int sys_lstat(const char *path, struct stat *buf)` |
| 108 | SYS_fstat | `sys_fstat` | `int sys_fstat(int fd, struct stat *buf)` |
| 21 | SYS_mount | `sys_mount` | `int sys_mount(const char *source, const char *target, const char *fstype)` |
| 22 | SYS_umount | `sys_unmount` | `int sys_unmount(const char *target)` |
| 24 | SYS_sched_yield | `sys_yield` | `int sys_yield(void)` |
| 64 | SYS_getppid | `sys_getppid` | `int sys_getppid(void)` |
| 88 | SYS_reboot | `sys_reboot` | `int sys_reboot(void)` |
| 122 | SYS_uname | `sys_uname` | `int sys_uname(struct utsname *buf)` |

Dispatch: `int $0x80` pushes edx, ecx, ebx; `call *systemcall_table(,%eax,4)`.

`sys_write`/`sys_read` fall back to global VFS files when `scheduler_get_current()` is NULL (early boot).

`sys_fork` reads eip/cs/eflags from the stack frame (ebp+20/24/28) to pass to `task_fork()`.

`sys_exec` validates and loads an ELF32 binary, creates a new page directory with user mappings, and overwrites the current task's iretl frame with the ELF entry point. ELF loading is implemented in `kernel/elf.c` (validate, load segments, copy content).

`sys_waitpid` blocks until a child task exits, then returns the child PID. Currently ignores the `pid` and `options` parameters.

### Task / Scheduler

- Static pool: `tasks[256]`, each with `task_stacks[pid][4096]`
- Task struct: pid, state, context*, stack*, name[32], next*, fd_table[16], pdir*, is_user
- Context: esp, ebp, ebx, esi, edi, pdir (6 x 4 bytes)
- `task_init_context()` -- ring 0 task: push arg, task_exit, entry; set context.esp
- `task_init_user_context()` -- ring 3 task: user stack (lower half of task stack), iretl frame (upper half), kernel stack at TASK_STACK_SIZE for TSS.ESP0
- Scheduler: round-robin, singly-linked run queue, TIME_SLICE=10 ticks (100ms at 100Hz)
- `schedule()`: if next is user task, set `tss_set_kernel_stack(next->stack + 4096)` before switching
- Fork: `task_fork()` deep-copies the parent's stack with a synthetic `int $0x80` return frame and `fork_return` trampoline (child sees eax=0)

### VFS Architecture

```
struct vfs_ops { int (*write)(struct file *, const void *, size_t); int (*read)(struct file *, void *, size_t); };
struct file { const struct vfs_ops *ops; void *private_data; };
```

- Driver registration: `VFS_DRIVER_INIT(fn)` macro places fn pointer in `.driver_init` linker section
- `vfs_init()` iterates `__driver_start` to `__driver_end`, calling each
- Drivers set global `vfs_stdin`/`vfs_stdout`/`vfs_stderr` during init
- Accessor functions: `vfs_get_stdin()`, `vfs_get_stdout()`, `vfs_get_stderr()`
- Each task has `fd_table[16]` — copied on fork; default: 0=stdin, 1=stdout, 2=stderr
- VFS mount layer: flat array `mounts[VFS_MAX_MOUNTS=8]`. Mounted as: ext2 at root `/`, tmpfs at `/dev`
- `vfs_inode` struct (in `kernel/vfs.h`): `{ i_no, i_size, i_type, ops, private_data }`
- Inode pool: static array of 256 inodes (`VFS_MAX_INODES`)
- File operations go through `vfs_open_file(inode)` which wraps an inode as a `struct file`
- **Mount resolution**: `vfs_resolve_mount()` compares by `i_no` AND `ops` pointer (not by VFS inode pointer), because filesystem lookups like `ext2_lookup()` allocate **new VFS inode objects** for the same underlying resource each time. `ext2_lookup()` sets `child->i_no = found_inode` (the on-disk ext2 inode number) to enable stable identification.

### Block Device Layer

- `kernel/block.h` + `kernel/block.c` — flat device table (`BLOCK_MAX_DEVICES=4`)
- `struct block_device`: `{ name[4], block_size, num_blocks, ops*, private_data* }`
- `struct block_device_ops`: `{ read_sector, write_sector }`
- `block_register_device()`, `block_find_device(name)`, `block_read()/block_write()` (multi-sector)

### ATA PIO Driver

- `drivers/ata/ata.h` + `drivers/ata/ata.c` — primary (0x1F0) + secondary (0x170) channels
- Detection: issue `IDENTIFY` (0xEC), poll BSY→DRQ with timeouts and `io_delay()`
- Registers detected drives as `hda`, `hdb`, etc. via `block_register_device()`
- 28-bit LBA, PIO polling (no IRQ), single-sector reads/writes

### ext2 Filesystem (Read-Only)

- `kernel/ext2.h` + `kernel/ext2.c` — on-disk structs (`ext2_sb`, `ext2_bgdesc`, `ext2_inode`, `ext2_dirent`)
- Parses superblock, block group descriptors, inodes, directory entries, indirect blocks (single/double/triple)
- `ext2_mount(block_dev)` → root `vfs_inode*` — mounted at `/` in `kernel_main()`
- Single 4KB block buffer (`ext2_buf`) — not reentrant, single-threaded only
- Data pool of 64 entries for per-inode private data (`ext2_data_pool`)
- Disk format: ext2 revision 0, 1KB block size, 128-byte inodes

### Port I/O

| Function | Width | Used For |
|----------|-------|----------|
| `in(port)` | 8-bit read | PIC status, PS/2 data, ATA status |
| `inw(port)` | 16-bit read | ATA data port (PIO data transfer) |
| `out(port, val)` | 8-bit write | PIC commands, PS/2 commands, VGA CRTC, ATA registers |
| `outw(port, val)` | 16-bit write | QEMU/Bochs ACPI shutdown, ATA data port |
| `outl(port, val)` | 32-bit write | VirtualBox ACPI shutdown |

## Coding Conventions

- **Tabs** for indentation, **K&R brace style**
- **snake_case** for functions/variables, **UPPER_CASE** for macros/constants
- **__attribute__((packed))** for hardware-facing structs
- Function naming: module prefix first (e.g. `pic_init`, `mm_frame_alloc`)
- `#include "kernel.h"` resolves to `arch/i386/kernel.h` (multiboot structs) via `-Iarch/i386/`
- No `kernel/kernel.h` exists; arch-specific headers under `arch/i386/`

## Directory Layout

```
arch/i386/           -- Boot, GDT/IDT, paging, task context, PIT, port I/O, kernel.ld
kernel/              -- Main, syscalls, scheduler, task, VFS, PIC, IRQ, block, ext2, mm.h, interrupt.h, pio.h
drivers/vga/         -- VGA text-mode framebuffer
drivers/ps2/         -- PS/2 keyboard
drivers/ata/         -- ATA PIO driver (block device)
shell/               -- Interactive shell (kernel task, ring 3)
libc/                -- Minimal freestanding libc (printf, string, unistd wrappers via int $0x80)
tests/               -- QEMU integration tests
```

## Important Gotchas

- **PIC IMR not cleared by init**: All IRQs masked after `pic_init()`. Call `pic_enable_irq()` for each.
- **EOI required after every IRQ**: `out(0x20, 0x20)`; for slave IRQs (8-15) also `out(0xA0, 0x20)`.
- **QEMU SMM breaks paging**: Use `-machine smm=off` when debugging paging issues.
- **TSS.ESP0 must be updated for user tasks**: `schedule()` calls `tss_set_kernel_stack(next->stack + 4096)` when next->is_user is set. If the TSS stack pointer is wrong, a ring 3 interrupt will corrupt kernel memory.
- **fork_return trampoline**: `task_fork()` pushes `fork_return` as the context_restore return address on the child's stack. `fork_return` does `mov $0, %eax; iretl`, so the child sees eax=0 without actually executing `sys_fork`.
- **Syscall fallback for early boot**: `sys_write`/`sys_read` use global VFS files when no current task exists.
- **vsprintf null terminator not counted**: `str[written] = '\0'`, not `str[written++]`, to avoid extra NUL in output.
- **Mount resolution compares i_no + ops, not pointers**: `vfs_resolve_mount()` matches by `i_no` and `ops` because `ext2_lookup()` allocates a new `vfs_inode` each time. The stored mount_point inode has a different pointer than the newly-looked-up inode, even for the same underlying ext2 directory. This means a mount at `/dev` resolves correctly even on repeated lookups.
- **ext2_lookup sets child->i_no to the on-disk inode number**: This is required for mount resolution to work. Without this, the VFS-level `i_no` would be a sequential pool counter, making cross-lookup identification impossible.
- **`-fno-stack-protector` in root CFLAGS**: Stack buffers in kernel code (e.g. shell buf[64]) trigger __stack_chk_fail without this flag.
- **`-mno-sse` in root CFLAGS**: GCC at `-O0` may generate SSE instructions (`movdqu`/`movaps`) for struct copies, causing #UD since CR4.OSFXSR is not set. Must use `-mno-sse`.
- **`make kernel.bin` not `make`**: Default target is `hello.elf` (legacy). Use `make kernel.bin` to build the kernel.
- **mm_map_at invlpg**: If mapping in the current page directory (`cr3 == pdir`), `mm_map_at` must `invlpg` the VA or the old mapping may be cached in the TLB.
- **ATA polling in QEMU TCG**: Tight polling loops on PIO status registers may hang because QEMU TCG virtual time doesn't advance for the device model. Always include `io_delay()` (volatile delay loop) between polling iterations.
- **vsprintf only handles %c, %s, %d**: No support for hex, unsigned, width, or precision.
- **No heap allocator**: Only `mm_frame_alloc()` (4KB pages). No malloc/kmalloc/brk.
- **memcmp broken in inline asm**: `libc/string/memcmp.c` had a bug where label `2:` (set result to 0 for "equal") always executed AFTER label `1:` (negate result for "less than"), causing memcmp to ALWAYS return 0 regardless of input. Fixed by using a simple C loop. If you add inline asm with multiple labels, make sure label paths are exclusive (use `jmp` after `1:` to skip `2:` when `a > b`).

## QEMU Testing / Debugging

```bash
make run              # curses display + serial.log + Unix socket monitor
make run-debug        # same + -S -s (wait for GDB)
make gdb              # gdb -ix gdb_script.gdb (connect to :1234)

# Run integration tests
make test             # runs tests/test-*.sh via tests/Makefile

# Run a single test
make -C tests test-boot
make -C tests test-vga
make -C tests test-shell
make -C tests test-usermode
make -C tests test-mmap
make -C tests test-multiboot
make -C tests test-tmpfs
make -C tests test-ext2

# VGA dump via monitor socket
echo "xp /80bx 0xB8000" | socat - UNIX-CONNECT:qemu-monitor.sock

# Interrupt log
qemu-system-i386 -d int -D qemu.log -kernel kernel.bin

# Stop VM
make quit
```

Test infrastructure (`tests/helpers.sh`): Starts QEMU with `-display none -no-reboot -monitor unix:...`, polls VGA buffer `0xB8000` via monitor `xp` commands, sends keystrokes via `sendkey`. Tests are shell scripts that source `helpers.sh`.
