int setpgid(int pid, int pgid)
{
	int ret;
	__asm__ volatile(
		"int $0x80"
		: "=a"(ret)
		: "a"(57), "b"(pid), "c"(pgid)
		: "memory"
	);
	return ret;
}
