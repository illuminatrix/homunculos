int uname(void *buf)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(122), "b"(buf)
		: "ecx", "edx", "memory");
	return ret;
}
