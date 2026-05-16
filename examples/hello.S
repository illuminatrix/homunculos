.section .text
.globl _start
_start:
	mov $4, %eax
	mov $1, %ebx
	mov $msg, %ecx
	mov $14, %edx
	int $0x80

	mov $1, %eax
	xor %ebx, %ebx
	int $0x80

.section .data
msg: .ascii "Hello, World!\n"
