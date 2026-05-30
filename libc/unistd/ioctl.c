int ioctl(int fd, int cmd, void *arg)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(54), "b"(fd), "c"(cmd), "d"(arg)
		: "memory");
	return ret;
}
