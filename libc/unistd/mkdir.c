int mkdir(const char *path)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(39), "b"(path)
		: "memory");
	return ret;
}
