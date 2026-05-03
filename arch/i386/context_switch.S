.global switch_context
.global context_restore

.section .text

# switch_context(prev, next)
# Save current context to prev, load context from next.
# After this function "returns", we're in the next task.
switch_context:
	movl 4(%esp), %eax   # eax = prev
	movl 8(%esp), %ecx   # ecx = next

	# Save current registers to prev's context
	movl %esp, (%eax)    # prev->context.esp
	movl %ebp, 4(%eax)   # prev->context.ebp
	movl %ebx, 8(%eax)   # prev->context.ebx
	movl %esi, 12(%eax)  # prev->context.esi
	movl %edi, 16(%eax)  # prev->context.edi

	# Load next's context
	movl (%ecx), %esp    # esp = next->context.esp
	movl 4(%ecx), %ebp
	movl 8(%ecx), %ebx
	movl 12(%ecx), %esi
	movl 16(%ecx), %edi

	# "Return" to next task - pops return address from next's stack
	ret

# context_restore(context)
# Load context and jump to it. Used for starting the first task.
context_restore:
	movl 4(%esp), %eax   # eax = context

	movl (%eax), %esp    # esp = context.esp
	movl 4(%eax), %ebp
	movl 8(%eax), %ebx
	movl 12(%eax), %esi
	movl 16(%eax), %edi

	# "Return" to the task
	ret
