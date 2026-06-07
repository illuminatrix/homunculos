int fsync(int fd)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(118), "b"(fd)
		: "ecx", "edx");
	return ret;
}
