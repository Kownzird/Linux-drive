#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>
#define GEC6818_LED_ON		_IOW('L',  1, unsigned long)
#define GEC6818_LED_OFF		_IOW('L',  2, unsigned long)

int main(void)
{
	int fd,ret;
	fd = open("/dev/led_drv", O_RDWR);
	if(fd < 0)
	{
		perror("open led_drv");
		return -1;		
	}
	while(1)
	{
		ret = ioctl(fd, GEC6818_LED_ON, 11);//D11--on
		if(ret < 0)
			perror("ioctl");
		usleep(200*1000);
		ret = ioctl(fd, GEC6818_LED_OFF, 11);//D11--off
		if(ret < 0)
			perror("ioctl");
		usleep(200*1000);

	}
	close(fd);
	return 0;
}