#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd,ret;
	char led_ctrl[2];
	fd = open("/dev/led_drv", O_RDWR);
	if(fd < 0)
	{
		perror("open led_drv");
		return -1;		
	}
	while(1)
	{
		led_ctrl[1] = 9; led_ctrl[0] = 1; //D9 on
		ret = write(fd, led_ctrl, sizeof(led_ctrl));
		if(ret != 2)
		{
			perror("write");
		}
		led_ctrl[1] = 9; led_ctrl[0] = 0; //D9 off
		usleep(300*1000);
		ret = write(fd, led_ctrl, sizeof(led_ctrl));
		if(ret != 2)
		{
			perror("write");
		}
		usleep(300*1000);		
	}
	close(fd);
	return 0;
}