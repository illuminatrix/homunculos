int yield(void)
{
	int ret;
	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(24)
		: "ebx", "ecx", "edx", "memory");
	return ret;
}
