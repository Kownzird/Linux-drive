//GPIOC14 --->beep
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
static struct cdev gec6818_beep_cdev;

static unsigned int beep_major = 0; //主设备号
static unsigned int beep_minor = 0; //次设备号
static dev_t  beep_num; //设备号

static struct class * beeps_class;
static struct device *beeps_device;

static struct resource *  beeps_res;
static void __iomem *gpioc_base_va;

static void __iomem *gpiocout_va;//0x00
static void __iomem *gpiocoutenb_va;//0x04
static void __iomem *gpiocaltfn0_va;
static void __iomem *gpiocaltfn1_va;//0x24

//2、定义一个文件操作集，并初始化
int gec6818_beep_open(struct inode *inode, struct file *filp)
{
	printk("beep driver openning\n");
	return 0;
}

//应用程序write()-->系统调用-->驱动程序gec6818_beep_write()
//const char __user *user_buf ---->应用程序写下来的数据
//size_t size --->应用程序写下来的数据大小
//定义一个数据格式：user_buf[0]--->beep的状态：1--beep on， 0--beep off
ssize_t gec6818_beep_write(struct file *filp, const char __user *user_buf, size_t size, loff_t *off)
{
	//接收用户写下来的数据，并利用这些数据来控制beep灯。
	char kbuf[1];
	int ret;
	if(size != 1)
		return -EINVAL;
	ret = copy_from_user(kbuf, user_buf, size);//得到用户空间的数据
	if(ret != 0)
		return -EFAULT;
	if(kbuf[0]==1)
	{
		*(unsigned int *)gpiocout_va |= (1<<14);//on
	}
	else if(kbuf[0]==0)
	{
		*(unsigned int *)gpiocout_va &= ~(1<<14);//off
	}
	
	return size;
}

int gec6818_beep_release(struct inode *inode, struct file *filp)
{
	printk("beep driver closing\n");
	return 0;
}

static const struct file_operations gec6818_beep_fops = {
	.owner = THIS_MODULE,
	.open = gec6818_beep_open,
	.write = gec6818_beep_write,
	.release = gec6818_beep_release,
};

//入口函数--->安装驱动
static int __init gec6818_beep_init(void)
{
	int ret;
	//3、申请设备号
	if(beep_major == 0)
		ret = alloc_chrdev_region(&beep_num, beep_minor, 1, "beep_device");
	else {
		beep_num = MKDEV(beep_major, beep_minor);
		ret = register_chrdev_region(beep_num, 1, "beep_device");
	}
	if(ret < 0){
		printk("can not get a device number \n");
		return ret;		
	}
	
	//4.字符设备的初始化
	cdev_init(&gec6818_beep_cdev, &gec6818_beep_fops);
	
	//5.将字符设备加入内核
	ret = cdev_add(&gec6818_beep_cdev, beep_num, 1);
	if(ret < 0){
		printk("can not add cdev \n");
		unregister_chrdev_region(beep_num, 1);
		return ret;		
	}
	
	//6. 创建class
	beeps_class = class_create(THIS_MODULE, "gec210_beeps");
	if(beeps_class == NULL){
		printk("class create error\n");
		unregister_chrdev_region(beep_num, 1);
		cdev_del(&gec6818_beep_cdev);
		return -EBUSY;
	}
	
	//7.创建device
	beeps_device = device_create(beeps_class,NULL,beep_num, NULL, "beep_drv");
	if(beeps_device == NULL){
		printk("device create error\n");
		class_destroy(beeps_class);
		cdev_del(&gec6818_beep_cdev);
		unregister_chrdev_region(beep_num, 1);
		
		return -EBUSY;
	}
	
	//8申请物理内存区
	beeps_res = request_mem_region(0xC001C000, 0x1000,"GPIOC_MEM");
	if(beeps_res == NULL){
		printk("request memory error\n");
		device_destroy(beeps_class, beep_num);
		class_destroy(beeps_class);
		cdev_del(&gec6818_beep_cdev);
		unregister_chrdev_region(beep_num, 1);
		
		return -EBUSY;
	}
	
	//9.IO内存动态映射，得到物理地址对应的虚拟地址
	gpioc_base_va = ioremap(0xC001C000, 0x1000);
	if(gpioc_base_va == NULL){
		printk("ioremap error\n");
		release_mem_region(0xC001C000, 0x1000);
		device_destroy(beeps_class, beep_num);
		class_destroy(beeps_class);
		cdev_del(&gec6818_beep_cdev);
		unregister_chrdev_region(beep_num, 1);
		
		return -EBUSY;
	}
	//得到每个寄存器的虚拟地址
	gpiocout_va = gpioc_base_va + 0x00;
	gpiocoutenb_va = gpioc_base_va + 0x04;
	gpiocaltfn0_va = gpioc_base_va + 0x20;
	gpiocaltfn1_va = gpioc_base_va + 0x24;

	
	//10.访问虚拟地址
	//10.1 GPIOC7,8.12,17 --->function1,作为普通的GPIO
	*(unsigned int *)gpiocaltfn1_va &=~(2<<28);
	//10.2 GPIOC7,8.12,17 --->设置为输出
	*(unsigned int *)gpiocoutenb_va |=(1<<14);
	//10.3 GPIOC7,8.12,17 --->设置为输出高电平，D8~D11 off
	//*(unsigned int *)gpiocout_va |= (1<<14);
	
	*(unsigned int *)gpiocout_va &= ~(1<<14);
	printk(KERN_WARNING "gec6818 beep driver init \n");
	
	return 0;
}

//出口函数--->卸载驱动
static void __exit gec6818_beep_exit(void)
{
	iounmap(gpioc_base_va);
	release_mem_region(0xC001C000, 0x1000);
	device_destroy(beeps_class, beep_num);
	class_destroy(beeps_class);
	cdev_del(&gec6818_beep_cdev);
	unregister_chrdev_region(beep_num, 1);
	
	printk("<4>" "gec6818 beep driver exit \n");
}

//驱动程序的入口：#insmod beep_drv.ko -->module_init()-->gec6818_beep_init()
module_init(gec6818_beep_init);
//驱动程序的出口：#rmmod beep_drv.ko --->module_exit()-->gec6818_beep_exit()
module_exit(gec6818_beep_exit);

//module的描述。#modinfo beep_drv.ko
MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("beep driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

