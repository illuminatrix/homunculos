int chmod(const char *path, int mode)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(15), "b"(path), "c"(mode)
		: "edx");
	return ret;
}
