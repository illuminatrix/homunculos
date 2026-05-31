int symlink(const char *target, const char *path)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(83), "b"(target), "c"(path)
		: "memory");
	return ret;
}
