int execve(const char *path, char *const argv[], char *const envp[])
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(11), "b"(path), "c"(argv), "d"(envp)
		: "memory"
	);

	return ret;
}

extern char **environ;

int execv(const char *path, char *const argv[])
{
	return execve(path, argv, environ);
}

int exec(const char *path)
{
	return execve(path, 0, environ);
}
