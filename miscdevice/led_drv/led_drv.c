#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <cfg_type.h>


#define GEC6818_LED_ON		_IOW('L',  1, unsigned long)
#define GEC6818_LED_OFF		_IOW('L',  2, unsigned long)


struct led_gpio_info{
	unsigned gpio_num;
	char gpio_name[20];	
};

static const struct led_gpio_info  led_info[4]={
	{
		.gpio_num = PAD_GPIO_C+17, 
		.gpio_name = "gpioc17_d8",
	},
	{
		.gpio_num = PAD_GPIO_C+8, 
		.gpio_name = "gpioc8_d9",
	},
	{
		.gpio_num = PAD_GPIO_C+7, 
		.gpio_name = "gpioc7_d10",
	},
	{
		.gpio_num = PAD_GPIO_C+12, 
		.gpio_name = "gpioc12_d11",
	},
};

//1、定义一个文件操作集，并初始化
//filp --->内核中，指向驱动文件的指针
//cmd --->应用程序发下来的命令
//args --->应用程序发下来的参数
static long gec6818_led_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	switch(cmd){
	case GEC6818_LED_ON:
		switch(args){
		case 8:
			gpio_set_value(led_info[0].gpio_num, 0);
			break;
		case 9:
			gpio_set_value(led_info[1].gpio_num, 0);
			break;
		case 10:
			gpio_set_value(led_info[2].gpio_num, 0);
			break;
		case 11:
			gpio_set_value(led_info[3].gpio_num, 0);
			break;
		default:
			return -EINVAL;
		}
		break;
	case GEC6818_LED_OFF:
		switch(args){
		case 8:
			gpio_set_value(led_info[0].gpio_num, 1);
			break;
		case 9:
			gpio_set_value(led_info[1].gpio_num, 1);
			break;
		case 10:
			gpio_set_value(led_info[2].gpio_num, 1);
			break;
		case 11:
			gpio_set_value(led_info[3].gpio_num, 1);
			break;	
		default:
			return -EINVAL;
		}
		break;
	default:
		return -ENOIOCTLCMD;
		
	}
	return 0;	
}

static const struct file_operations gec6818_led_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gec6818_led_ioctl,
};

//2、定义一个混杂设备，并初始化
static  struct miscdevice gec6818_led_misc = {
	.minor = MISC_DYNAMIC_MINOR, //动态分配次设备号
	.name = "led_drv",
	.fops = &gec6818_led_fops,
};


//入口函数--->安装驱动
static int __init gec6818_led_init(void)
{
	int ret,i;
	
		
	//3.申请GPIO*4
	for(i=0;i<4;i++){
		ret = gpio_request(led_info[i].gpio_num, led_info[i].gpio_name);
		if(ret < 0){
			printk("cannot request gpio:%s\n",led_info[i].gpio_name);
			goto gpio_request_error;
		}
		gpio_direction_output(led_info[i].gpio_num, 1);		
	}
	
	ret = misc_register(&gec6818_led_misc);
	if(ret < 0){
		printk("misc register is error\n");
		goto misc_register_error;		
	}
	
	printk(KERN_WARNING "gec6818 led driver init \n");
	
	return 0;
	
misc_register_error:
gpio_request_error:
	while(i--)//--i
		gpio_free(led_info[i].gpio_num);
	return ret;	
}

//出口函数--->卸载驱动
static void __exit gec6818_led_exit(void)
{
	int i=4;
	while(i--)//--i
		gpio_free(led_info[i].gpio_num);
		
	misc_deregister(&gec6818_led_misc);
	
	printk("<4>" "gec6818 led driver exit \n");
}

//驱动程序的入口：#insmod led_drv.ko -->module_init()-->gec6818_led_init()
module_init(gec6818_led_init);
//驱动程序的出口：#rmmod led_drv.ko --->module_exit()-->gec6818_led_exit()
module_exit(gec6818_led_exit);

//module的描述。#modinfo led_drv.ko
MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("LED driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

