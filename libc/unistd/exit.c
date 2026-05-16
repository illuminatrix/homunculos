void exit(int status)
{
	asm volatile(
		"int $0x80"
		:
		: "a"(1), "b"(status)
		: "ecx", "edx", "memory"
	);
}
