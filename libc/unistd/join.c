int join(void)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(6)
		: "ebx", "ecx", "edx", "memory"
	);

	return ret;
}
