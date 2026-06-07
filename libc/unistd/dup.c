int dup(int oldfd)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(41), "b"(oldfd)
		: "ecx", "edx");
	return ret;
}
