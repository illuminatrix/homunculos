int fchmod(int fd, int mode)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(94), "b"(fd), "c"(mode)
		: "edx");
	return ret;
}
