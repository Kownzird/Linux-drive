#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd,ret,i;
	char key_flag[4]={1,1,1,1};
	fd = open("/dev/key_drv", O_RDONLY);
	if(fd < 0)
	{
		perror("open key_drv");
		return -1;		
	}
	while(1)
	{
		ret = read(fd, key_flag, sizeof(key_flag)); //×èÈû£¬ÔÚÇý¶¯³ÌÐòread()
		if(ret != 4)
			perror("read");

		for(i=0;i<4;i++)
			printf("key_flag[%d] = %d\n",i,key_flag[i]);
		//usleep(100*1000);
	}
	close(fd);
	return 0;
}