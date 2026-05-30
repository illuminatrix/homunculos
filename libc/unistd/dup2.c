int dup2(int oldfd, int newfd)
{
	int res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(63), "b"(oldfd), "c"(newfd));
	return res;
}
