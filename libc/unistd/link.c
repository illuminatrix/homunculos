int link(const char *old_path, const char *new_path)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(9), "b"(old_path), "c"(new_path)
		: "edx");
	return ret;
}
