#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define GEC6818_BEEP_ON  _IO('B', 0)
#define GEC6818_BEEP_OFF _IO('B', 1)

int main(void)
{
	int ret;
	int fd = open("/dev/beep_drv", O_WRONLY);
	if(fd <= 0)
	{
		perror("open():");
		return -1;
	}

	while(1)
	{
		ret = ioctl(fd, GEC6818_BEEP_ON);
		if(ret < 0)
			perror("beep ioctl");
		usleep(500*1000);
		ret = ioctl(fd, GEC6818_BEEP_OFF);
		if(ret < 0)
			perror("beep ioctl");
		usleep(500*1000);
	}
	close(fd);
	return 0;
}
