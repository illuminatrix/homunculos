int waitpid(int pid, int *status, int options)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(7), "b"(pid), "c"(status), "d"(options)
		: "memory"
	);

	return ret;
}
