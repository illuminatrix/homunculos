#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>

int main(void)
{
	struct utsname uts;
	if (uname(&uts) < 0) {
		printf("uname failed\n");
		return 1;
	}
	printf("%s %s %s %s %s\n",
	       uts.sysname, uts.nodename,
	       uts.release, uts.version, uts.machine);
	return 0;
}
