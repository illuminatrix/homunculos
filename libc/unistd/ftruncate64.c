int ftruncate64(int fd, unsigned long length)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(194), "b"(fd), "c"(length)
		: "edx");
	return ret;
}
