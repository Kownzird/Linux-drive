#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd,ret;
	char beep_ctrl[1];
	fd = open("/dev/beep_drv", O_RDWR);
	if(fd < 0)
	{
		perror("open beep_drv");
		return -1;		
	}
	while(1)
	{
		 beep_ctrl[0] = 1; //D9 on
		ret = write(fd, beep_ctrl, sizeof(beep_ctrl));
		if(ret != 2)
		{
			perror("write");
		}
		beep_ctrl[0] = 0; //D9 off
		usleep(300*1000);
		ret = write(fd, beep_ctrl, sizeof(beep_ctrl));
		if(ret != 2)
		{
			perror("write");
		}
		usleep(300*1000);		
	}
	close(fd);
	return 0;
}