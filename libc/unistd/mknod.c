int mknod(const char *path, int mode, int dev)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(14), "b"(path), "c"(mode), "d"(dev)
		: "memory");
	return ret;
}
