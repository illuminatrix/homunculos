int brk(void *addr)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(45), "b"(addr)
		: "ecx", "edx", "memory");
	return ret;
}

void *sbrk(int increment)
{
	int old;

	asm volatile("int $0x80"
		: "=a"(old)
		: "0"(45), "b"(0)
		: "ecx", "edx", "memory");
	if (old < 0)
		return (void *)-1;

	int new_brk;
	asm volatile("int $0x80"
		: "=a"(new_brk)
		: "0"(45), "b"(old + increment)
		: "ecx", "edx", "memory");
	if (new_brk != old + increment)
		return (void *)-1;

	return (void *)old;
}
