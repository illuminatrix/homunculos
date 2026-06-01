#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
	int i;
	for (i = 0; environ[i]; i++)
		printf("%s\n", environ[i]);
	return 0;
}
