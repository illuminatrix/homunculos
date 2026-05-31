int readlink(const char *path, char *buf, unsigned int size)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(85), "b"(path), "c"(buf), "d"(size)
		: "memory");
	return ret;
}
