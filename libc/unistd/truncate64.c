int truncate64(const char *path, unsigned long length)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(193), "b"(path), "c"(length)
		: "edx");
	return ret;
}
