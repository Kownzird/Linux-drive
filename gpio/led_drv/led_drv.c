#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ioctl.h>

#include <linux/gpio.h>
#include <cfg_type.h>


#define GEC6818_LED_ON		_IOW('L',  1, unsigned long)
#define GEC6818_LED_OFF		_IOW('L',  2, unsigned long)

//1.定义一个cdev
static struct cdev gec6818_led_cdev;

static unsigned int led_major = 0; //主设备号
static unsigned int led_minor = 0; //次设备号
static dev_t  led_num; //设备号

static struct class * leds_class;
static struct device *leds_device;

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

//2、定义一个文件操作集，并初始化
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

//入口函数--->安装驱动
static int __init gec6818_led_init(void)
{
	int ret,i;
	//3、申请设备号
	if(led_major == 0)
		ret = alloc_chrdev_region(&led_num, led_minor, 1, "led_device");
	else {
		led_num = MKDEV(led_major, led_minor);
		ret = register_chrdev_region(led_num, 1, "led_device");
	}
	if(ret < 0){
		printk("can not get a device number \n");
		return ret;		
	}
	
	//4.字符设备的初始化
	cdev_init(&gec6818_led_cdev, &gec6818_led_fops);
	
	//5.将字符设备加入内核
	ret = cdev_add(&gec6818_led_cdev, led_num, 1);
	if(ret < 0){
		printk("can not add cdev \n");
		goto cdev_add_error;			
	}
	
	//6. 创建class
	leds_class = class_create(THIS_MODULE, "gec210_leds");
	if(leds_class == NULL){
		printk("class create error\n");
		ret = -EBUSY;
		goto class_create_error;
	}
	
	//7.创建device
	leds_device = device_create(leds_class,NULL,led_num, NULL, "led_drv");
	if(leds_device == NULL){
		printk("device create error\n");
		ret = -EBUSY;
		goto device_create_error;
	}
	
	//8.申请GPIO*4
	for(i=0;i<4;i++){
		ret = gpio_request(led_info[i].gpio_num, led_info[i].gpio_name);
		if(ret < 0){
			printk("cannot request gpio:%s\n",led_info[i].gpio_name);
			goto gpio_request_error;
		}
		gpio_direction_output(led_info[i].gpio_num, 1);		
	}
	
	printk(KERN_WARNING "gec6818 led driver init \n");
	
	return 0;
	

gpio_request_error:
	while(i--)//--i
		gpio_free(led_info[i].gpio_num);
	
	device_destroy(leds_class, led_num);
device_create_error:	
	class_destroy(leds_class);
class_create_error:		
	cdev_del(&gec6818_led_cdev);
cdev_add_error:	
	unregister_chrdev_region(led_num, 1);
	return ret;	
}

//出口函数--->卸载驱动
static void __exit gec6818_led_exit(void)
{
	int i=4;
	while(i--)//--i
		gpio_free(led_info[i].gpio_num);
		
	device_destroy(leds_class, led_num);
	class_destroy(leds_class);
	cdev_del(&gec6818_led_cdev);
	unregister_chrdev_region(led_num, 1);
	
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

