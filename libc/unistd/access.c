int access(const char *path)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(33), "b"(path)
		: "memory");
	return ret;
}
