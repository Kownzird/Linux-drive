#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
//1.定义一个cdev
static struct cdev gec6818_led_cdev;

static unsigned int led_major = 100; //主设备号
static unsigned int led_minor = 0; //次设备号
static dev_t  led_num; //设备号

static struct class * leds_class;
static struct device *leds_device;

static struct resource *  leds_res;
static void __iomem *gpioc_base_va;
static void __iomem *gpiocout_va;//0x00
static void __iomem *gpiocoutenb_va;//0x04
static void __iomem *gpiocaltfn0_va;//0x20
static void __iomem *gpiocaltfn1_va;//0x24
static void __iomem *gpiocpad_va;//0x18

//2、定义一个文件操作集，并初始化
int gec6818_led_open(struct inode *inode, struct file *filp)
{
	printk("led driver openning\n");
	return 0;
}

//应用程序write()-->系统调用-->驱动程序gec6818_led_write()
//定义一个数据格式：
//					user_buf[3]--->D11的状态：1--灯亮， 0--灯灭
//					user_buf[2]--->D10的状态：1--灯亮， 0--灯灭
//					user_buf[1]--->D9的状态：1--灯亮， 0--灯灭
//                  			user_buf[0]--->D8的状态：1--灯亮， 0--灯灭
ssize_t gec6818_led_read(struct file *filp, char __user *user_buf, size_t size, loff_t *off)
{
	//将驱动程序的数据发送给应用程序，这个数据代表LED的状态<---gpiocout_va
	char kbuf[4];
	int ret;
	if(size != 4)
		return -EINVAL;
	//通过读取寄存器的状态，得到每个LED的状态-->kbuf[4]
	//kbuf[4]=?????
	ret = copy_to_user(user_buf,kbuf,size);
	if(ret != 0)
		return -EFAULT;
	return size;
}

//应用程序write()-->系统调用-->驱动程序gec6818_led_write()
//const char __user *user_buf ---->应用程序写下来的数据
//size_t size --->应用程序写下来的数据大小
//定义一个数据格式：user_buf[1]--->LED的灯号：8/9/10/11
//                  user_buf[0]--->LED的状态：1--灯亮， 0--灯灭
ssize_t gec6818_led_write(struct file *filp, const char __user *user_buf, size_t size, loff_t *off)
{
	//接收用户写下来的数据，并利用这些数据来控制LED灯。
	char kbuf[2];
	int ret;
	if(size != 2)
		return -EINVAL;
	ret = copy_from_user(kbuf, user_buf, size);//得到用户空间的数据
	if(ret != 0)
		return -EFAULT;
	switch(kbuf[1]){
	case 8: //GPIOC17 --> D8
		if(user_buf[0] == 1)
			*(unsigned int *)gpiocout_va &= ~(1<<17);
		else if(user_buf[0] == 0)
			*(unsigned int *)gpiocout_va |= (1<<17);
		else
			return -EINVAL;
		break;
	case 9: //GPIOC8 --> D9
		if(user_buf[0] == 1)
			*(unsigned int *)gpiocout_va &= ~(1<<8);
		else if(user_buf[0] == 0)
			*(unsigned int *)gpiocout_va |= (1<<8);
		else
			return -EINVAL;
		break;	
	case 10: //GPIOC7 --> D10
		if(user_buf[0] == 1)
			*(unsigned int *)gpiocout_va &= ~(1<<7);
		else if(user_buf[0] == 0)
			*(unsigned int *)gpiocout_va |= (1<<7);
		else
			return -EINVAL;
		break;	
	case 11: //GPIOC12 --> D11
		if(user_buf[0] == 1)
			*(unsigned int *)gpiocout_va &= ~(1<<12);
		else if(user_buf[0] == 0)
			*(unsigned int *)gpiocout_va |= (1<<12);
		else
			return -EINVAL;
		break;	
	}
	return size;
}

int gec6818_led_release(struct inode *inode, struct file *filp)
{
	printk("led driver closing\n");
	return 0;
}

static const struct file_operations gec6818_led_fops = {
	.owner = THIS_MODULE,
	.open = gec6818_led_open,
	.read = gec6818_led_read,
	.write = gec6818_led_write,
	.release = gec6818_led_release,
};

//入口函数--->安装驱动
static int __init gec6818_led_init(void)
{
	int ret;
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
		unregister_chrdev_region(led_num, 1);
		return ret;		
	}
	
	//6. 创建class
	leds_class = class_create(THIS_MODULE, "gec210_leds");
	if(leds_class == NULL){
		printk("class create error\n");
		unregister_chrdev_region(led_num, 1);
		cdev_del(&gec6818_led_cdev);
		return -EBUSY;
	}
	
	//7.创建device
	leds_device = device_create(leds_class,NULL,led_num, NULL, "led_drv");
	if(leds_device == NULL){
		printk("device create error\n");
		class_destroy(leds_class);
		cdev_del(&gec6818_led_cdev);
		unregister_chrdev_region(led_num, 1);
		
		return -EBUSY;
	}
	
	//8申请物理内存区
	leds_res = request_mem_region(0xC001C000, 0x1000,"GPIOC_MEM");
	if(leds_res == NULL){
		printk("request memory error\n");
		device_destroy(leds_class, led_num);
		class_destroy(leds_class);
		cdev_del(&gec6818_led_cdev);
		unregister_chrdev_region(led_num, 1);
		
		return -EBUSY;
	}
	
	//9.IO内存动态映射，得到物理地址对应的虚拟地址
	gpioc_base_va = ioremap(0xC001C000, 0x1000);
	if(gpioc_base_va == NULL){
		printk("ioremap error\n");
		release_mem_region(0xC001C000, 0x1000);
		device_destroy(leds_class, led_num);
		class_destroy(leds_class);
		cdev_del(&gec6818_led_cdev);
		unregister_chrdev_region(led_num, 1);
		
		return -EBUSY;
	}
	//得到每个寄存器的虚拟地址
	gpiocout_va = gpioc_base_va + 0x00;
	gpiocoutenb_va = gpioc_base_va + 0x04;
	gpiocaltfn0_va = gpioc_base_va + 0x20;
	gpiocaltfn1_va = gpioc_base_va + 0x24;
	gpiocpad_va = gpioc_base_va + 0x18;

	printk("gpiocout_va = %p,gpiocpad_va=%p\n",gpiocout_va, gpiocpad_va);
	
	//10.访问虚拟地址
	//10.1 GPIOC7,8.12,17 --->function1,作为普通的GPIO
	*(unsigned int *)gpiocaltfn0_va &=~((3<<14)|(3<<16)|(3<<24));
	*(unsigned int *)gpiocaltfn1_va &=~(3<<2);
	*(unsigned int *)gpiocaltfn0_va |= ((1<<14)|(1<<16)|(1<<24));
	*(unsigned int *)gpiocaltfn1_va |= (1<<2);
	//10.2 GPIOC7,8.12,17 --->设置为输出
	*(unsigned int *)gpiocoutenb_va |= ((1<<7)|(1<<8)|(1<<12)|(1<<17));
	//10.3 GPIOC7,8.12,17 --->设置为输出高电平，D8~D11 off
	*(unsigned int *)gpiocout_va |= ((1<<7)|(1<<8)|(1<<12)|(1<<17));
	
	
	printk(KERN_WARNING "gec6818 led driver init \n");
	
	return 0;
}

//出口函数--->卸载驱动
static void __exit gec6818_led_exit(void)
{
	iounmap(gpioc_base_va);
	release_mem_region(0xC001C000, 0x1000);
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

