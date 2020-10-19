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

#define GEC6818_ALLKEY_STATE	_IOW('K', 1, unsigned long)
#define GEC6818_KEY2_STATE		_IOW('K', 2, unsigned long)
#define GEC6818_KEY3_STATE		_IOW('K', 3, unsigned long)
#define GEC6818_KEY4_STATE		_IOW('K', 4, unsigned long)
#define GEC6818_KEY6_STATE		_IOW('K', 6, unsigned long)


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


/*
struct cdev {
	struct kobject kobj; --->内核管理驱动的时候，使用的一个object
	struct module *owner; --->cdev是属于哪个module，一般写成THIS_MODULE
	const struct file_operations *ops; --->cdev的文件操作集
	struct list_head list; --->内核管理cdev的链表
	dev_t dev; --->设备号
	unsigned int count; --->次设备的数量
};
*/


/****************************************************************/
/*
struct file_operations {
	struct module *owner;
	...............................
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	..............................
}
*/


//初始化文件操作集
static long gec6818_key_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	int i = 0;
	int value = 0, ret;
	unsigned int __user *p_user = (unsigned int *)args;
	
	switch(cmd){
	case GEC6818_KEY2_STATE:
		value =  gpio_get_value( key_info[0].gpio_num);
		printk("key2 value = %d\n", value);
		ret = copy_to_user(p_user, &value, sizeof(int));
		if(ret != 0)
			return -EFAULT;
		break;
	case GEC6818_KEY3_STATE:
		value =  gpio_get_value( key_info[1].gpio_num);
		printk("key3 value = %d\n", value);
		ret = copy_to_user(p_user, &value, sizeof(int));
		if(ret != 0)
			return -EFAULT;
		break;
	case GEC6818_KEY4_STATE:
		value =  gpio_get_value( key_info[2].gpio_num);
		printk("key4 value = %d\n", value);
		ret = copy_to_user(p_user, &value, sizeof(int));
		if(ret != 0)
			return -EFAULT;
		break;
	case GEC6818_KEY6_STATE:
		value =  gpio_get_value( key_info[3].gpio_num);
		printk("key6 value = %d\n", value);
		ret = copy_to_user(p_user, &value, sizeof(int));
		if(ret != 0)
			return -EFAULT;
		break;
	case GEC6818_ALLKEY_STATE:
		for(i=0; i<4; i++)
			value = value|(gpio_get_value( key_info[i].gpio_num)<<1);
		ret = copy_to_user(p_user, &value, sizeof(int));
		if(ret != 0)
			return -EFAULT;
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

//1.定义一个文件操作集
static struct file_operations gec6818_key_fps = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gec6818_key_ioctl,
	//.write = gec6818_led_write,
	//.read = gec6818_led_read,
};

//2、定义一个混杂设备，并初始化
static  struct miscdevice gec6818_key_misc = {
	.minor = MISC_DYNAMIC_MINOR, //动态分配次设备号
	.name = "key_drv",
	.fops = &gec6818_key_fps,
};

/****************************************************************/
/*
void cdev_init(struct cdev *cdev, const struct file_operations *fops);//初始化字符设备函数
void unregister_chrdev_region(dev_t from, unsigned count);//卸载字符设备函数

int cdev_add(struct cdev *p, dev_t dev, unsigned count);//将字符设备加入内核
void cdev_del(struct cdev *p);//将字符设备从内核删除

struct class *class_create(struct module *owner, const char *name);//创建class函数
void class_destroy(struct class *cls);//删除class函数

struct device *device_create(struct class *class, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...);//创建device
void device_destroy(struct class *class, dev_t devt);//删除device

struct resource *  request_mem_region(resource_size_t start, resource_size_t n,
				   const char *name);//申请物理内存
void release_mem_region(resource_size_t start, resource_size_t n);//释放物理内存	

将一段物理地址内存区映射成一段虚拟地址内存区
void __iomem *ioremap(phys_addr_t offset, unsigned long size);
解除IO内存动态映射
void iounmap(void __iomem *addr)			 
*/

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
