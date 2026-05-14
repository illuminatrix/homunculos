# Illuminatrix -- Agent Context

## Project Overview

Illuminatrix is a freestanding i386 (x86 32-bit) kernel written in C with GNU Assembler, booting via Multiboot (GRUB/QEMU). It is a teaching/hobby OS with multitasking, a round-robin scheduler, VFS abstraction, VGA text output, PS/2 keyboard input, fork(), and a simple shell. No libc -- implements its own minimal subset with inline syscall via `int $0x80`.

## Build System

- **Compiler**: `gcc` with `-std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -m32 -fno-stack-protector`
- **Assembler**: `as --32`
- **Linker**: `ld -melf_i386 -T arch/i386/kernel.ld`
- **Make**: flat `OBJS` variable accumulated via `include` chains of `Makefile.mk` files
- **Output**: `kernel.bin` (Multiboot- compliant flat binary)
- **Include path**: `-Ilibc/include -Ikernel -Ishell` (+ `-Iarch/i386/` per-file via pattern rule)

### Make Targets

| Target | Action |
|--------|--------|
| `make` | Build `kernel.bin` |
| `make test` | Run QEMU integration tests (multiboot, boot, VGA, shell) |
| `make run` | `qemu-system-i386 -kernel kernel.bin -display curses -serial file:serial.log -monitor unix:qemu-monitor.sock,server,nowait` |
| `make run-debug` | Same + `-S -s` (pause, wait for GDB on :1234) |
| `make debug` | Generate `kernel.lst` disassembly |
| `make gdb` | `gdb -ix gdb_script.gdb` (connects to :1234) |
| `make clean` | Remove all `.o`, `.bin`, `.lst`, `serial.log`, `qemu-monitor.sock` |
| `make quit` | Send `quit` to QEMU monitor via socat |

### Committing

When making git commits, load the **git-committer** skill.

### Adding new files

1. Append `<path/to/file.o>` to the appropriate `Makefile.mk` in the subdirectory
2. `drivers/Makefile.mk` includes sub-driver makefiles; add a new `include` line there when adding a new driver directory

## Directory Layout

```
arch/i386/           -- Architecture-specific code (boot, GDT/IDT, paging, context switch, PIT, port I/O)
kernel/              -- Architecture-independent kernel (main, syscalls, scheduler, task, VFS, PIC, IRQ)
drivers/vga/         -- VGA text-mode framebuffer driver
drivers/ps2/         -- PS/2 keyboard driver
shell/               -- Interactive shell (kernel task)
libc/                -- Minimal freestanding libc (printf, string, unistd wrappers)
tests/               -- Behavioral integration tests (QEMU-based)
```

## Key Files and Their Roles

| File | Purpose |
|------|---------|
| `arch/i386/kernel_head.S` | Multiboot header, 16KB BSS stack, `_start` calls `kernel_main` |
| `arch/i386/idt.h` | IDT descriptor struct + stack frame context structs |
| `arch/i386/interrupts.c` | 255-entry IDT setup, default exception handlers (spin on halt) |
| `arch/i386/isrs.S` | ISR/IRQ stubs, `syscall_handler` (indirect call via table), `yield_handler`, `fork_return` |
| `arch/i386/context_switch.S` | `switch_context(prev,next)` and `context_restore(ctx)` |
| `arch/i386/mm.c` | Identity paging (16MB), bitmap frame allocator, `mm_clone_pdir` for fork |
| `arch/i386/task.c` | Arch-specific `task_init_context` and `task_init_fork_context` |
| `arch/i386/kernel.ld` | Load at 1MB, `.driver_init` section for VFS driver registration |
| `kernel/kernel.c` | `kernel_main` -- init sequence (see below) |
| `kernel/syscall.c` | 255-entry syscall table, 6 syscalls implemented |
| `kernel/syscall.h` | Syscall number defines |
| `kernel/scheduler.c` | Round-robin, linked-list run queue, TIME_SLICE=10 ticks (100ms at 100Hz) |
| `kernel/task.c` | Static pool of 256 tasks with 4KB stacks, `task_alloc`, `task_fork`, `task_exit` |
| `kernel/task.h` | Task struct, MAX_TASKS=256, states READY/RUNNING/BLOCKED/EXITED |
| `kernel/vfs.c` | Iterates `.driver_init` section, calls each driver's init |
| `kernel/vfs.h` | `struct vfs_ops`, `struct file`, `VFS_DRIVER_INIT` macro, VFS_MAX_FD=16 |
| `kernel/irq.c` | 32-entry IRQ handler table, dispatch + EOI |
| `kernel/pic.c` | 8259 PIC init (ICW1-ICW4), remap to 0x20-0x2F, IRQ masking |
| `kernel/mm.h` | FRAME=0x1000, DIR_SIZE=1024, MAX_MEMORY_BYTES=256MB |
| `drivers/vga/text.c` | VGA 80x25 text mode, cursor via 0x3D4/0x3D5, scroll, stdout (0x0F) / stderr (0x04) |
| `drivers/ps2/kbd.c` | PS/2 keyboard, scancode set 1 with shift/caps, 256-byte ring buffer, IRQ1 |
| `shell/shell.c` | Prompt "> ", commands: `greeting` (prints hello), `poweroff` (shutdown) |
| `libc/stdio/printf.c` | printf via vsprintf + `int $0x80` SYS_write |
| `libc/stdio/vsprintf.c` | %c, %s, %d |
| `libc/unistd/fork.c` | fork via `int $0x80` eax=2 |
| `libc/unistd/read.c` | read via `int $0x80` eax=3 |
| `libc/unistd/reboot.c` | reboot via `int $0x80` eax=88 |
| `libc/string/*.c` | memcmp, memcpy, memmove, memset, strlen, strncpy, strcmp (asm-optimized where possible) |
| `tests/Makefile` | Test runner: `make test` runs all QEMU integration tests |
| `tests/helpers.sh` | Shared QEMU lifecycle (start/stop), monitor interaction (xp, sendkey), VGA decode |
| `tests/test-multiboot.sh` | Verifies Multiboot magic/flags/checksum via objdump + xxd |
| `tests/test-boot.sh` | Boots kernel, checks "Illuminatrix Kernel!" in VGA memory |
| `tests/test-vga.sh` | Dumps VGA buffer, validates output and attribute 0x0F |
| `tests/test-shell.sh` | Sends "greeting" → checks "hello", sends "poweroff" → checks shutdown |

## Init Sequence

```
_start (kernel_head.S)
  -> kernel_main(multiboot_info_t*)  [kernel/kernel.c]
      1. syscall_init()       -- populate systemcall_table[255]
      2. pic_init()           -- 8259 PIC with ICW1-ICW4, remap IRQs
      3. load_idt()           -- build 255-entry IDT, load IDTR, STI
      4. vfs_init()           -- iterate .driver_init, init VGA + PS/2
      5. welcome()            -- printf("Illuminatrix Kernel!\n")
      6. init_mm()            -- identity paging 16MB, frame bitmap init
      7. scheduler_init()     -- PIT at 100Hz, callback = scheduler_tick
      8. setup_main_task(shell_main, 0) -- create shell task
      9. pic_enable_irq(0)    -- timer
     10. pic_enable_irq(1)    -- keyboard
     11. while(1) hlt
```

## Syscalls

| # | Name | Handler | Signature |
|---|------|---------|-----------|
| 1 | SYS_exit | `sys_exit` | `int sys_exit(int status)` |
| 2 | SYS_fork | `sys_fork` | `int sys_fork(void)` |
| 3 | SYS_read | `sys_read` | `int sys_read(int fd, void *buf, size_t len)` |
| 4 | SYS_write | `sys_write` | `int sys_write(int fd, const void *buf, size_t len)` |
| 24 | SYS_sched_yield | `sys_yield` | `int sys_yield(void)` |
| 88 | SYS_reboot | `sys_reboot` | `int sys_reboot(void)` |

Dispatch: `int $0x80` -> `syscall_handler` (isrs.S) pushes edx, ecx, ebx, then `call *systemcall_table(,%eax,4)`.

## Interrupt Model

- IDT: 255 entries, all 32-bit interrupt gates (type=0xE), seg_sel=0x08
- Vectors 0-31: CPU exceptions (ISRs), default handlers spin on halt
- Vectors 32-47: IRQs 0-15 (PIC remapped). Dispatch calls registered handler then sends EOI.
- Vector 0x31: yield (software), calls `schedule()`
- Vector 0x80: syscall (software), dispatches via table
- PIC: master at 0x20, slave at 0xA0. EOI sent to 0x20 for all; also to 0xA0 for slave IRQs (8-15)
- After PIC init, IMR is NOT cleared -- must `pic_enable_irq()` for each needed IRQ

## Task/Scheduler Model

- Static pool: `tasks[256]`, each with `task_stacks[pid][4096]`
- Task struct: pid, state, context*, stack*, name[32], next*, fd_table[16], pdir*
- Context: esp, ebp, ebx, esi, edi, pdir (6 x 4 bytes)
- Scheduler: round-robin, singly-linked run queue, TIME_SLICE=10 ticks (100ms)
- `scheduler_tick()` called from PIT IRQ0; every 10th tick calls `schedule()`
- `schedule()`: rotate current to tail, pick next READY from head, switch_context or context_restore
- Task init: push arg, task_exit, entry onto stack; context.esp points there; context_restore ret's into entry
- Fork: deep stack copy, synthetic `int $0x80` return frame with `fork_return` (sets eax=0, iretl)

## Memory Management

- Identity paging: 16MB (4 page tables, 4MB each)
- Page table entries: `0x3` (Present + Read/Write), no User bit (ring 0 only)
- Frame allocator: bitmap over max 256MB, linear scan
- `mm_clone_pdir()`: deep-copies entire page table tree for fork (allocates new frames, memcpy content)
- Kernel loaded at 1MB, `__kernel_end` marks end of BSS (used to reserve kernel pages in allocator)

## VFS Architecture

```
struct vfs_ops { int (*write)(struct file *, const void *, size_t); int (*read)(struct file *, void *, size_t); };
struct file { struct vfs_ops *ops; void *private_data; };
```

- Global file pointers: `vfs_stdin`, `vfs_stdout`, `vfs_stderr`
- Driver registration: `VFS_DRIVER_INIT(fn)` macro places fn pointer in `.driver_init` linker section
- `vfs_init()` iterates `__driver_init_start` to `__driver_init_end`, calling each
- Each task has `fd_table[16]` -- copied on fork

## Port I/O

| Function | Width | Used For |
|----------|-------|----------|
| `in(port)` | 8-bit read | PIC status, PS/2 data |
| `out(port, val)` | 8-bit write | PIC commands, PS/2 commands, VGA CRTC |
| `outw(port, val)` | 16-bit write | QEMU/Bochs ACPI shutdown |
| `outl(port, val)` | 32-bit write | VirtualBox ACPI shutdown |

## Coding Conventions

- **Tabs** for indentation
- **K&R brace style** (opening brace on same line)
- **snake_case** for functions and variables
- **UPPER_CASE** for macros and constants
- Structs defined directly in `.h` files (no opaque structs in kernel code)
- `__attribute__((packed))` for hardware-facing structs
- No dynamic allocation -- static arrays and fixed pools
- No create/destroy -- init functions on static globals
- Function naming: module prefix first (e.g., `pic_init`, `task_alloc`, `mm_frame_alloc`)

## Important Gotchas / Lessons Learned

- **PIC IMR is NOT cleared by init**: After `pic_init()`, all IRQs are masked. Must call `pic_enable_irq()` for each needed IRQ.
- **EOI must be sent after every IRQ**: `irq_handler()` sends EOI after dispatching. For slave IRQs (8-15), send to both 0xA0 and 0x20.
- **QEMU SMM breaks paging**: Use `-machine smm=off` if debugging paging issues.
- **fork_return is a trampoline**: Child gets `eax=0` from `fork_return` trampoline, not from sys_fork.
- **No GDT setup exists**: Code relies on GRUB's flat GDT (CS=0x08, DS=0x10). No ring 3 segments.
- **No heap allocator**: Only page-level `mm_frame_alloc()`. No `malloc`/`kmalloc`/`brk`.
- **Syscall fallback for early boot**: `sys_write` and `sys_read` check `scheduler_get_current()` -- if NULL, fall back to global VFS files.
- **vsprintf null terminator not counted**: `str[written] = '\0'`, not `str[written++]`, to avoid extra NUL in output.
- **`-fno-stack-protector` in root CFLAGS**: Required when kernel code has local stack buffers.

## QEMU Testing / Debugging

```bash
# Run with curses display
make run

# Run with GDB server
make run-debug
# In another terminal:
make gdb

# Check VGA memory via monitor
echo "xp /80bx 0xB8000" | nc -q 1 127.0.0.1 4444

# With Unix socket monitor:
echo "xp /80bx 0xB8000" | socat - UNIX-CONNECT:qemu-monitor.sock

# Serial output goes to serial.log
tail -f serial.log

# Dump QEMU interrupt log
qemu-system-i386 -d int -D qemu.log -kernel kernel.bin ...

# Quick VGA dump script:
qemu-system-i386 -kernel kernel.bin -display none \
  -monitor tcp:127.0.0.1:4444,server,nowait &
sleep 3
echo "xp /4000bx 0xB8000" | nc -q 1 127.0.0.1 4444
kill %1
```
