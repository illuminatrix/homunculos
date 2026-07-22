int setsid(void)
{
	int ret;
	__asm__ volatile(
		"int $0x80"
		: "=a"(ret)
		: "a"(66)
		: "memory"
	);
	return ret;
}
