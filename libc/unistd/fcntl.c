int fcntl(int fd, int cmd, int arg)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(221), "b"(fd), "c"(cmd), "d"(arg)
		: );
	return ret;
}
