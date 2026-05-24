#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>

void _start(void)
{
	struct utsname uts;
	if (uname(&uts) < 0) {
		printf("uname failed\n");
		exit(1);
	}
	printf("%s %s %s %s %s\n",
	       uts.sysname, uts.nodename,
	       uts.release, uts.version, uts.machine);
	exit(0);
}
