int lseek(int fd, int offset, int whence)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(19), "b"(fd), "c"(offset), "d"(whence)
		: "memory");
	return ret;
}
