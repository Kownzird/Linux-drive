#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>

#define GEC6818_WDT_START		_IO('W',  1)
#define GEC6818_WDT_STOP		_IO('W',  2)
#define GEC6818_WDT_ALIVE		_IO('W',  3)

int main(void)
{
	int fd,ret,i=0;
	fd = open("/dev/wdt_drv", O_RDWR);
	if(fd < 0)
	{
		perror("open wdt_drv");
		return -1;		
	}

	ret = ioctl(fd, GEC6818_WDT_START);
	if(ret < 0)
		perror("ioctl");

	while(1)
	{
		printf("%d second\n",++i);
		sleep(1);		
	}
		
	close(fd);
	return 0;
}