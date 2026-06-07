#include <unistd.h>
#include <stdio.h>

int main(void)
{
	sync();
	printf("sync done\n");
	return 0;
}
