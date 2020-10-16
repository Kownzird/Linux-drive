//gpiob14 --->beep
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

#define GEC6818_KEY_READ		 _IOR('K',1,unsigned long)

//1.定义一个cdev
static struct cdev gec6818_key_cdev;

static unsigned int key_major = 0; //主设备号
static unsigned int key_minor = 0; //次设备号
static dev_t  key_num; //设备号

static struct class * keys_class;
static struct device *keys_device;

static struct resource *  keys_res;
static void __iomem *gpiob_base_va;

static void __iomem *gpiobout_va;//0x00
static void __iomem *gpioboutenb_va;//0x04
static void __iomem *gpiobaltfn0_va;
static void __iomem *gpiobaltfn1_va;//0x24
static void __iomem *gpiobpad_va;//0x24
//2、定义一个文件操作集，并初始化



//应用程序write()-->系统调用-->驱动程序gec6818_beep_write()
//const char __user *user_buf ---->应用程序写下来的数据
//size_t size --->应用程序写下来的数据大小
//定义一个数据格式：user_buf[0]--->beep的状态：1--beep on， 0--beep off
int gec6818_key_open(struct inode *inode, struct file *filp)
{
	printk("key9 driver openning\n");
	return 0;
}

//应用程序write()-->系统调用-->驱动程序gec6818_beep_write()
//const char __user *user_buf ---->应用程序写下来的数据
//size_t size --->应用程序写下来的数据大小
//定义一个数据格式：user_buf[0]--->beep的状态：1--beep on， 0--beep off


int gec6818_key_release(struct inode *inode, struct file *filp)
{
	printk("beep driver closing\n");
	return 0;
}


static long gec6818_key_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	int key9_flag=0,ret;
	unsigned int __user *p_user = (unsigned int *)args;
	switch(cmd){
	case GEC6818_KEY_READ:	
		key9_flag = (readl(gpiobpad_va)>>9) & 0x01;
		printk("key_flag = %d\n",key9_flag);
		ret = copy_to_user(p_user, &key9_flag, sizeof(int));
		if(ret != 0)
			return -EFAULT;
		break;
	default:
		return -ENOIOCTLCMD;		
	}	
			
	return 0;
}

static const struct file_operations gec6818_key_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gec6818_key_ioctl,
	.open = gec6818_key_open,
	.release = gec6818_key_release
};

//入口函数--->安装驱动
static int __init gec6818_key_init(void)
{
	int ret;
	//3、申请设备号
	if(key_major == 0)
		ret = alloc_chrdev_region(&key_num, key_minor, 1, "key_device");
	else {
		key_num = MKDEV(key_major, key_minor);
		ret = register_chrdev_region(key_num, 1, "key_device");
	}
	if(ret < 0){
		printk("can not get a device number \n");
		return ret;		
	}
	
	//4.字符设备的初始化
	cdev_init(&gec6818_key_cdev, &gec6818_key_fops);
	
	//5.将字符设备加入内核
	ret = cdev_add(&gec6818_key_cdev, key_num, 1);
	if(ret < 0){
		printk("can not add cdev \n");
		unregister_chrdev_region(key_num, 1);
		return ret;		
	}
	
	//6. 创建class
	keys_class = class_create(THIS_MODULE, "gec210_keys");
	if(keys_class == NULL){
		printk("class create error\n");
		unregister_chrdev_region(key_num, 1);
		cdev_del(&gec6818_key_cdev);
		return -EBUSY;
	}
	
	//7.创建device
	keys_device = device_create(keys_class,NULL,key_num, NULL, "key_drv");
	if(keys_device == NULL){
		printk("device create error\n");
		class_destroy(keys_class);
		cdev_del(&gec6818_key_cdev);
		unregister_chrdev_region(key_num, 1);
		
		return -EBUSY;
	}
	
	//8申请物理内存区
	keys_res = request_mem_region(0xC001B000, 0x1000,"GPIOB_MEM");
	if(keys_res == NULL){
		printk("request memory error\n");
		device_destroy(keys_class, key_num);
		class_destroy(keys_class);
		cdev_del(&gec6818_key_cdev);
		unregister_chrdev_region(key_num, 1);
		
		return -EBUSY;
	}

	//9.IO内存动态映射，得到物理地址对应的虚拟地址
	gpiob_base_va = ioremap(0xC001B000, 0x1000);
	if(gpiob_base_va == NULL){
		printk("ioremap error\n");
		release_mem_region(0xC001B000, 0x1000);
		device_destroy(keys_class, key_num);
		class_destroy(keys_class);
		cdev_del(&gec6818_key_cdev);
		unregister_chrdev_region(key_num, 1);
		
		return -EBUSY;
	}
	//得到每个寄存器的虚拟地址
	gpiobout_va = gpiob_base_va + 0x00;
	gpioboutenb_va = gpiob_base_va + 0x04;
	gpiobaltfn0_va = gpiob_base_va + 0x20;
	//gpiobaltfn1_va = gpiobpad_va + 0x24;
	//gpiobpad_va = gpiobpad_va + 0x18;
	gpiobaltfn1_va = gpiob_base_va + 0x24;
	gpiobpad_va = gpiob_base_va + 0x18;
	
	//10.访问虚拟地址
	//10.1 gpiob9（K6） --->function0,作为普通的GPIO
	*(unsigned int *)gpiobaltfn0_va &=~(3<<18);
	
	//10.2 gpiob9 --->设置为输入
	*(unsigned int *)gpioboutenb_va &=~(1<<9);
	

	printk(KERN_WARNING "gec6818 key9 driver init \n");
	
	return 0;
}

//出口函数--->卸载驱动
static void __exit gec6818_key_exit(void)
{
	iounmap(gpiob_base_va);
	release_mem_region(0xC001B000, 0x1000);
	device_destroy(keys_class, key_num);
	class_destroy(keys_class);
	cdev_del(&gec6818_key_cdev);
	unregister_chrdev_region(key_num, 1);
	
	printk("<4>" "gec6818 key9 driver exit \n");
}

//驱动程序的入口：#insmod beep_drv.ko -->module_init()-->gec6818_beep_init()
module_init(gec6818_key_init);
//驱动程序的出口：#rmmod beep_drv.ko --->module_exit()-->gec6818_beep_exit()
module_exit(gec6818_key_exit);

//module的描述。#modinfo beep_drv.ko
MODULE_AUTHOR("ltp@163.com");
MODULE_DESCRIPTION("key driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

