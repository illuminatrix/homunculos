# Error code is pushed to the stack automatically by HW. See intel's system programing manual -  6.13 Error code

.macro ISR_ERROR_CODE vector
.global isr_\vector
isr_\vector:
        pushal
        # error code is at [esp + 32] (above pushal), iret frame at [esp + 36]
        lea 36(%esp), %ebx      # ebx = pointer to iret frame
        pushl %ebx              # push iret frame (rightmost, pushed first)
        mov $(\vector), %eax
        pushl %eax              # push vector number
        call common_handler_error_code
        addl $8, %esp           # pop both params
        addl $4, %esp           # discard error code pushed by cpu hw
        popal
        addl $4, %esp           # discard interrupt vector number
        iretl
.endm

.macro ISR vector
.global isr_\vector
isr_\vector:
        pushal
        lea 32(%esp), %ebx      # ebx = pointer to iret frame
        pushl %ebx              # push iret frame (rightmost)
        mov $(\vector), %eax
        pushl %eax              # push vector number
        call common_handler
        addl $8, %esp           # pop both params
        popal
        iretl
.endm
# For reference see intel's system programing manual -  6.3.1 External interrupts
## interrupt_vectors
ISR                  0   # divide error
ISR                  1   # reserved
ISR                  2   # nmi
ISR                  3   # breakpoint
ISR                  4   # overflow
ISR                  5   # bound range exceeded
ISR                  6   # invalid opcode
ISR                  7   # no math coprocessor
ISR_ERROR_CODE       8   # double fault
ISR                  9   # coporcessor segment overrun
ISR_ERROR_CODE       10  # invalid tss
ISR_ERROR_CODE       11  # segment not present
ISR_ERROR_CODE       12  # stack segment fault
ISR_ERROR_CODE       13  # general protection
ISR_ERROR_CODE       14  # page fault
ISR                  15  # reserved
ISR                  16  # fpu error
ISR_ERROR_CODE       17  # alignment check
ISR                  18  # machine check
ISR                  19  # SIMD Floating-Point exception
ISR                  20  # Virtualization exception
ISR                  21
ISR                  22
ISR                  23
ISR                  24
ISR                  25
ISR                  26
ISR                  27
ISR                  28
ISR                  29
ISR                  30
ISR                  31

.macro IRQ irq_no
.global irq_\irq_no
irq_\irq_no:
        cld
	pushal
	mov %esp, %ecx          # ecx = pointer to edi (after pushal)
	lea 32(%ecx), %ebx      # ebx = pointer to iret frame (eip)
	mov $(\irq_no), %eax    # irq number
	pushl %ebx              # push iret frame pointer (2nd param)
	pushl %eax              # push irq number (1st param)
	call irq_handler
	addl $8, %esp           # pop both params
        cli
	popal
	iretl
.endm

IRQ 0  # Timer interrupt
IRQ 1  # Keyboard interrupt
IRQ 2  # Cascade interrupt
IRQ 3  # COM2 interrupt
IRQ 4  # COM1 interrupt
IRQ 5  # LPT2 interrupt
IRQ 6  # Floppy disk interrupt
IRQ 7  # LPT1 interrupt
IRQ 8  # CMOS clock interrupt
IRQ 9  # Real time clock interrupt
IRQ 10 # Auxiliary storage interrupt
IRQ 11 # Reserved
IRQ 12 # Reserved
IRQ 13 # Reserved
IRQ 14 # Reserved
IRQ 15 # Reserved

.global syscall_handler
syscall_handler:
	# 6-arg convention: ebp=arg6, edi=arg5, esi=arg4, edx=arg3, ecx=arg2, ebx=arg1
	pushl %ebp # arg 6 (offset for mmap2)
	pushl %edi # arg 5 (fd for mmap2)
	pushl %esi # arg 4 (flags for mmap2)
	pushl %edx # arg 3 (prot for mmap2)
	pushl %ecx # arg 2 (length for mmap2)
	pushl %ebx # arg 1 (addr for mmap2)
	call *systemcall_table(,%eax,4) # call systemcall function position %eax moving 4 bytes
	# Check for pending signals before returning to user mode
	# iret frame (eip/cs/eflags/uesp/ss) is at esp + 24 (6 params * 4 bytes)
	lea 24(%esp), %ebx      # ebx = pointer to iret frame
	pushl %eax              # save syscall return value
	pushl %ebx              # push iret frame pointer
	call signal_check_and_deliver
	addl $4, %esp           # pop pointer
	popl %eax               # restore return value (may be unused if signal delivered)
	add $24, %esp # cleaning stack (6 params)
	# return is available in %eax
	iretl

.global fork_return
fork_return:
	movl $0, %eax
	iretl

