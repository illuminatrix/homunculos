void sync(void)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(36)
		: "ebx", "ecx", "edx");
}
