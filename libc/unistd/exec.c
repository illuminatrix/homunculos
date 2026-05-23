int exec(const char *path)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(11), "b"(path)
		: "ecx", "edx", "memory"
	);

	return ret;
}
