#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "task.h"

#define SHELL_BUF_SIZE 64
#define SHELL_ARGS_MAX 4

extern const uint8_t _binary_examples_hello_elf_start[];
extern const uint8_t _binary_examples_hello_elf_end[];

static void shell_prompt(void)
{
	printf("> ");
}

static int shell_readline(char *buf, int size)
{
	int pos = 0;

	while (1) {
		char c;
		int n = read(0, &c, 1);
		if (n <= 0) {
			task_yield();
			continue;
		}

		if (c == '\n') {
			printf("\n");
			buf[pos] = '\0';
			return pos;
		} else if (c == '\b') {
			if (pos > 0) {
				pos--;
				printf("\b \b");
			}
		} else if (pos < size - 1) {
			buf[pos++] = c;
			printf("%c", c);
		}
	}
}

static void cmd_run(void)
{
	int pid = fork();
	if (pid < 0) {
		printf("fork failed\n");
		return;
	}

	if (pid == 0) {
		int ret = exec(_binary_examples_hello_elf_start);
		if (ret < 0)
			printf("exec failed\n");
		exit(1);
	}

	int child_pid = join();
	printf("child %d done\n", child_pid);
}

static void shell_execute(const char *buf)
{
	if (strcmp(buf, "poweroff") == 0)
		reboot();
	else if (strcmp(buf, "greeting") == 0)
		printf("hello\n");
	else if (strcmp(buf, "run") == 0)
		cmd_run();
	else if (buf[0] != '\0')
		printf("unknown\n");
}

void shell_main(void *arg)
{
	(void)arg;
	char buf[SHELL_BUF_SIZE];

	while (1) {
		shell_prompt();
		shell_readline(buf, SHELL_BUF_SIZE);
		shell_execute(buf);
	}
}
