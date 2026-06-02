#include <sys/reboot.h>

int main(void)
{
	reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
	       LINUX_REBOOT_CMD_RESTART);
	return 0;
}
