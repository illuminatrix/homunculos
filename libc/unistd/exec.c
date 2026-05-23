int exec(const void *elf)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(11), "b"(elf)
		: "ecx", "edx", "memory"
	);

	return ret;
}
