#include <stdio.h>
#include <unistd.h>
#include "task.h"

static void shell_prompt(void)
{
	printf("> ");
}

void shell_main(void *arg)
{
	(void)arg;
	char buf[64];
	int pos = 0;

	shell_prompt();

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
			if (pos == 8 && buf[0] == 'p' && buf[1] == 'o'
			    && buf[2] == 'w' && buf[3] == 'e'
			    && buf[4] == 'r' && buf[5] == 'o'
			    && buf[6] == 'f' && buf[7] == 'f')
				reboot();
			else if (pos == 8 && buf[0] == 'g' && buf[1] == 'r'
			    && buf[2] == 'e' && buf[3] == 'e'
			    && buf[4] == 't' && buf[5] == 'i'
			    && buf[6] == 'n' && buf[7] == 'g')
				printf("hello\n");
			else if (pos > 0)
				printf("unknown\n");
			pos = 0;
			shell_prompt();
		} else if (c == '\b') {
			if (pos > 0) {
				pos--;
				printf("\b \b");
			}
		} else if (pos < (int)sizeof(buf) - 1) {
			buf[pos++] = c;
			printf("%c", c);
		}
	}
}
