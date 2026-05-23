int waitpid(int pid, int *status)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(7), "b"(pid), "c"(status)
		: "edx", "memory"
	);

	return ret;
}
