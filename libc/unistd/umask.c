int umask(int mask)
{
	int ret;
	__asm__ volatile(
		"int $0x80"
		: "=a"(ret)
		: "a"(60), "b"(mask)
		: "memory"
	);
	return ret;
}
