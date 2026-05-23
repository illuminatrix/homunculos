int getppid(void)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(64)
		: "ebx", "ecx", "edx");
	return ret;
}
