int write(int fd, const void *buf, unsigned int len)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(4), "b"(fd), "c"(buf), "d"(len)
		: "memory");
	return ret;
}
