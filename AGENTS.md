# Illuminatrix -- Agent Context

## Build System

- **Compiler**: `gcc -std=gnu11 -nostdlib -ffreestanding -fno-pie -O0 -m32 -fno-stack-protector`
- **Assembler**: `as --32`
- **Linker**: `ld -melf_i386 -T arch/i386/kernel.ld`
- **Output**: `kernel.bin` (Multiboot flat binary, loaded at 1MB)
- **Include path**: `-Ilibc/include -Ikernel -Ishell -Iarch/i386/`
- **Build**: flat `OBJS` accumulated via `include` chains of `Makefile.mk` files

### Make Targets

| Target | Action |
|--------|--------|
| `make` | Build `kernel.bin` |
| `make test` | Runs all QEMU integration tests |
| `make run` | `qemu-system-i386 -kernel kernel.bin -display curses -serial file:serial.log -monitor unix:qemu-monitor.sock,server,nowait` |
| `make run-debug` | Same + `-S -s` (pause, wait for GDB on :1234) |
| `make debug` | Generate `kernel.lst` disassembly |
| `make gdb` | `gdb -ix gdb_script.gdb` (connects to :1234) |
| `make clean` | Remove all `.o`, `.bin`, `.lst`, `serial.log`, `qemu-monitor.sock` |
| `make quit` | Send `quit` to QEMU monitor via socat |

### Adding new files

1. Append `<path/to/file.o>` to the appropriate `Makefile.mk`
2. `drivers/Makefile.mk` includes sub-driver makefiles; add an `include` line there when adding a driver

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
     5. welcome()              -- printf("Illuminatrix Kernel!\n")
     6. init_mm(mmap, len)     -- identity paging 16MB, frame bitmap from multiboot map
     7. gdt_init()             -- GDT: ring 0/3 code+data + TSS
     8. scheduler_init()       -- PIT at 100Hz, callback = scheduler_tick
     9. setup_main_task(...)   -- create shell task (user context)
    10. pic_enable_irq(0)      -- timer
    11. pic_enable_irq(1)      -- keyboard
    12. while(1) hlt
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

### Interrupt Model

- IDT: 255 entries, 32-bit interrupt gates (type=0xE), seg_sel=0x08
- Vectors 0-31: CPU exceptions -- default handlers spin on halt
- Vectors 32-47: IRQs 0-15 (PIC remapped) -- dispatch via registered handler + EOI
- Vector 0x31: yield (software) -- DPL=3, calls `schedule()`
- Vector 0x80: syscall (software) -- DPL=3, dispatches via `systemcall_table[%eax]`
- PIC: master 0x20, slave 0xA0. EOI = `out(0x20, 0x20)`; for slave IRQs (8-15) also `out(0xA0, 0x20)`
- After `pic_init()`, all IRQs are masked. Must call `pic_enable_irq()` for each.

### Syscalls

| # | Name | Handler | Signature |
|---|------|---------|-----------|
| 1 | SYS_exit | `sys_exit` | `int sys_exit(int status)` |
| 2 | SYS_fork | `sys_fork` | `int sys_fork(void)` |
| 3 | SYS_read | `sys_read` | `int sys_read(int fd, void *buf, size_t len)` |
| 4 | SYS_write | `sys_write` | `int sys_write(int fd, const void *buf, size_t len)` |
| 24 | SYS_sched_yield | `sys_yield` | `int sys_yield(void)` |
| 88 | SYS_reboot | `sys_reboot` | `int sys_reboot(void)` |

Dispatch: `int $0x80` pushes edx, ecx, ebx; `call *systemcall_table(,%eax,4)`.

`sys_write`/`sys_read` fall back to global VFS files when `scheduler_get_current()` is NULL (early boot).

`sys_fork` reads eip/cs/eflags from the stack frame (ebp+20/24/28) to pass to `task_fork()`.

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
- `vfs_init()` iterates `__driver_init_start` to `__driver_init_end`, calling each
- Drivers set global `vfs_stdin`/`vfs_stdout`/`vfs_stderr` during init
- Accessor functions: `vfs_get_stdin()`, `vfs_get_stdout()`, `vfs_get_stderr()`
- Each task has `fd_table[16]` — copied on fork; default: 0=stdin, 1=stdout, 2=stderr

### Port I/O

| Function | Width | Used For |
|----------|-------|----------|
| `in(port)` | 8-bit read | PIC status, PS/2 data |
| `out(port, val)` | 8-bit write | PIC commands, PS/2 commands, VGA CRTC |
| `outw(port, val)` | 16-bit write | QEMU/Bochs ACPI shutdown |
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
kernel/              -- Main, syscalls, scheduler, task, VFS, PIC, IRQ, mm.h, interrupt.h, pio.h
drivers/vga/         -- VGA text-mode framebuffer
drivers/ps2/         -- PS/2 keyboard
shell/               -- Interactive shell (kernel task, ring 3)
libc/                -- Minimal freestanding libc (printf, string, unistd wrappers via int $0x80)
tests/               -- QEMU integration tests
```

## Important Gotchas

- **PIC IMR not cleared by init**: All IRQs masked after `pic_init()`. Call `pic_enable_irq()` for each.
- **EOI required after every IRQ**: `out(0x20, 0x20)`; for slave IRQs (8-15) also `out(0xA0, 0x20)`.
- **QEMU SMM breaks paging**: Use `-machine smm=off` when debugging paging issues.
- **TSS.ESP0 must be updated for user tasks**: `schedule()` calls `tss_set_kernel_stack(next->stack + 4096)` when next->is_user is set. If the TSS stack pointer is wrong, a ring 3 interrupt will corrupt kernel memory.
- **fork_return trampoline**: Child gets `eax=0` from a synthetic `int $0x80` return frame in `fork_return`, not from `sys_fork`.
- **Syscall fallback for early boot**: `sys_write`/`sys_read` use global VFS files when no current task exists.
- **vsprintf null terminator not counted**: `str[written] = '\0'`, not `str[written++]`, to avoid extra NUL in output.
- **`-fno-stack-protector` in root CFLAGS**: Stack buffers in kernel code (e.g. shell buf[64]) trigger __stack_chk_fail without this flag.
- **mm_map_at invlpg**: If mapping in the current page directory (`cr3 == pdir`), `mm_map_at` must `invlpg` the VA or the old mapping may be cached in the TLB.
- **vsprintf only handles %c, %s, %d**: No support for hex, unsigned, width, or precision.
- **No heap allocator**: Only `mm_frame_alloc()` (4KB pages). No malloc/kmalloc/brk.

## QEMU Testing / Debugging

```bash
make run              # curses display + serial.log + Unix socket monitor
make run-debug        # same + -S -s (wait for GDB)
make gdb              # gdb -ix gdb_script.gdb (connect to :1234)

# VGA dump via monitor socket
echo "xp /80bx 0xB8000" | socat - UNIX-CONNECT:qemu-monitor.sock

# Serial output
tail -f serial.log

# Interrupt log
qemu-system-i386 -d int -D qemu.log -kernel kernel.bin

# Stop VM
make quit
```
