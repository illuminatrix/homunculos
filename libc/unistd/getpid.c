int getpid(void)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(20)
		: "ebx", "ecx", "edx");
	return ret;
}
