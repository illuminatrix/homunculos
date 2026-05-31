int unlink(const char *path)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(87), "b"(path)
		: "memory");
	return ret;
}
