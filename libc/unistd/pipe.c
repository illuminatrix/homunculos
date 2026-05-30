int pipe(int fds[2])
{
	int res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(42), "b"(fds));
	return res;
}
