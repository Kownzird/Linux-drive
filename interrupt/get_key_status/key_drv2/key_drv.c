#include <linux/cdev.h>		//cdev 的头文件
#include <linux/module.h>	//THIS_MODULE的头文件
#include <linux/errno.h>	//使用错误码的有文件
#include <linux/ioctl.h>	//使用_IOW的头文件
#include <linux/init.h>		//初始化设备用到的头文件
#include <linux/kernel.h>
#include <linux/device.h>	//创建class的头文件	
#include <linux/io.h>		//物理地址->虚拟地址
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>//IO标准接口函数
#include <cfg_type.h>//IO口号
#include <linux/miscdevice.h>//混杂设备头文件

//定义GPIOC29的IO口号和名字
struct key_gpio_info{
	unsigned gpio_num;
	char gpio_name[20];
};

static const struct key_gpio_info key_info[4] = {
	{.gpio_num = PAD_GPIO_A + 28,
	.gpio_name = "K2_gpioA28_f0"},
	{.gpio_num = PAD_GPIO_B + 30,
	.gpio_name = "K3_gpioB30_f1"},
	{.gpio_num = PAD_GPIO_B + 31,
	.gpio_name = "K4_gpioB31_f1"},
	{.gpio_num = PAD_GPIO_B + 9,
	.gpio_name = "K6_gpioB9_f0"},
};

ssize_t gec6818_led_read(struct file *filp, char __user *user_buf, size_t size, loff_t *off)
{
	//将驱动程序的数据发送给应用程序，这个数据代表LED的状态<---gpiocout_va
	char kbuf[4] = {1,1,1,1}; //安装的状态：0--K2，1--K3，2--K4，3--K6
	int ret,i;
	if(size != 4)
		return -EINVAL;
	for(i=0;i<4;i++)
		kbuf[i] = gpio_get_value(key_info[i].gpio_num);
	ret = copy_to_user(user_buf, kbuf,size);
	if(ret != 0)
		return -EFAULT;
	return size;
}


//1.定义一个文件操作集
static struct file_operations gec6818_key_fps = {
	.owner = THIS_MODULE,
	.read = gec6818_led_read,
};

//2、定义一个混杂设备，并初始化
static  struct miscdevice gec6818_key_misc = {
	.minor = MISC_DYNAMIC_MINOR, //动态分配次设备号
	.name = "key_drv",
	.fops = &gec6818_key_fps,
};

static int __init gec6818_key_init(void)
{
	int ret,i,j;
	//2.申请物理内存
	for( i=0; i<4; i++){
		ret = gpio_request( key_info[i].gpio_num, key_info[i].gpio_name);
		if(ret < 0){
			printk("request gpio error:%s\n", key_info[i].gpio_name);
			goto request_gpio_error;
		}
	}
	
	//3.将GPIOA28,GPIOB30,GPIOB31,GPIOB9,设置为输入
	for( j=0; j<4; j++)
		gpio_direction_input(key_info[j].gpio_num);

	//4.注册混杂设备
	ret = misc_register( &gec6818_key_misc);
	if(ret < 0){
		printk("misc register error\n");
		goto misc_register_error;
	}
	
	printk("led_drv init success!");
	return 0;

misc_register_error:
	
request_gpio_error:
	while(i--){
		gpio_free(key_info[i].gpio_num);
	}
	return ret;

}

static void __exit gec6818_key_exit(void)
{
	int i = 4;
	while(i--){
		gpio_free(key_info[i].gpio_num);
	}
	misc_deregister(&gec6818_key_misc);
	printk("key_drv exit \n");
}

/**************************************************************************/
//驱动程序的入口：
module_init(gec6818_key_init);
//驱动程序的出口：
module_exit(gec6818_key_exit);

//module的描述
MODULE_AUTHOR("zheng_shuixin@163.com");
MODULE_DESCRIPTION("key sensor driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");
