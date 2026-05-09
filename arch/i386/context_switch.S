.global switch_context
.global context_restore

.section .text

# struct task_context layout:
#   esp  = 0
#   ebp  = 4
#   ebx  = 8
#   esi  = 12
#   edi  = 16
#   pdir = 20

# switch_context(prev, next)
switch_context:
	movl 4(%esp), %eax   # eax = prev
	movl 8(%esp), %ecx   # ecx = next

	# Save current registers to prev's context
	movl %esp, (%eax)    # prev->context.esp
	movl %ebp, 4(%eax)   # prev->context.ebp
	movl %ebx, 8(%eax)   # prev->context.ebx
	movl %esi, 12(%eax)  # prev->context.esi
	movl %edi, 16(%eax)  # prev->context.edi

	# Switch CR3 to next's page directory
	movl 20(%ecx), %edx  # next->context.pdir
	movl %edx, %cr3

	# Load next's context
	movl (%ecx), %esp    # esp = next->context.esp
	movl 4(%ecx), %ebp
	movl 8(%ecx), %ebx
	movl 12(%ecx), %esi
	movl 16(%ecx), %edi

	ret

# context_restore(context)
context_restore:
	movl 4(%esp), %eax   # eax = context

	# Switch CR3 to the task's page directory
	movl 20(%eax), %edx  # context->pdir
	movl %edx, %cr3

	movl (%eax), %esp    # esp = context.esp
	movl 4(%eax), %ebp
	movl 8(%eax), %ebx
	movl 12(%eax), %esi
	movl 16(%eax), %edi

	ret
