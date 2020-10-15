#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
#include <asm/ioctl.h>

#define GEC6818_BEEP_ON  _IO('B', 0)
#define GEC6818_BEEP_OFF _IO('B', 1)

//1.定义一个cdev
static struct cdev gec6818_beep_cdev;
static dev_t  beep_num;
static unsigned  int beep_major=0;
static unsigned  int beep_minor=0; 

static struct class *beep_class;
static struct device *beep_device;

static struct resource * beep_res;
static volatile void __iomem *GPIOCOUT_VA;  //0x00
static volatile void __iomem *GPIOCOUTENB_VA; //0x04
//static volatile void __iomem *GPIOCPAD_VA;   //0x18
static volatile void __iomem *GPIOCALTFN0_VA;//0x20
static volatile void __iomem *GPIOCALTFN1_VA;//0x24

//2.定义一个文件操作集，并完成初始化
//cmd --->应用程序发下来的命令；args--->哪一盏灯的参数：8~11
long gec6818_beep_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	switch(cmd){
	case GEC6818_BEEP_ON:
		writel(readl(GPIOCOUT_VA)|(1<<14),GPIOCOUT_VA);
		break;
	case GEC6818_BEEP_OFF:
		writel(readl(GPIOCOUT_VA)&~(1<<14),GPIOCOUT_VA);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}
int gec6818_beep_release(struct inode *inode, struct file *filp)
{
	writel(readl(GPIOCOUT_VA) &~(1<<14),GPIOCOUT_VA);
	printk("closing led_driver\n");
	return 0;
}


static const struct file_operations gec6818_beep_fops={
	.owner = THIS_MODULE,
	.unlocked_ioctl = gec6818_beep_ioctl,
	.release = gec6818_beep_release,
};

static int __init gec6818_beep_init(void)
{
	//3.设备号的申请
	int ret;
	int temp;
	if(beep_major == 0){
		ret = alloc_chrdev_region(&beep_num, beep_minor, 1, "beep_driver");
	}else{
		beep_num = MKDEV(beep_major,beep_minor);
		ret = register_chrdev_region(beep_num, 1, "beep_driver");
	}
	if(ret < 0){
		printk("chrdev region is error\n");
		return ret;
	}
	
	//4.字符设备的初始化
	cdev_init(&gec6818_beep_cdev, &gec6818_beep_fops);
	
	//5.将cdev加入内核
	ret = cdev_add(&gec6818_beep_cdev, beep_num, 1);
	if(ret < 0){
		printk("cdev_add is error\n");
		goto cdev_add_error;
	}
	//6.创建class
	beep_class = class_create(THIS_MODULE, "beep_class");
	if(beep_class == NULL){
		printk("class create is error\n");
		ret = -EBUSY;
		goto class_create_error;
	}
	
	//7.创建设备文件
	beep_device = device_create(beep_class, NULL,
			     beep_num, NULL, "beep_drv");
	if(beep_device == NULL){
		printk("deivce create is error\n");
		ret = -EBUSY;
		goto device_create_error;
	}			 
	
	//8.申请物理内存作为资源
	beep_res = request_mem_region(0xC001C000, 0x1000, "gec6818_beep_res");
	if(beep_res == NULL){
		printk("request memory is error\n");
		ret = -EBUSY;
		goto request_mem_error;
	}
	
	//9.IO内存的动态映射，得到虚拟地址
	GPIOCOUT_VA = ioremap(0xC001C000, 0x1000);
	if(GPIOCOUT_VA == NULL){
		printk("ioremap is error\n");
		return -EBUSY;
		goto ioremap_error;
	}
	GPIOCOUTENB_VA = GPIOCOUT_VA + 0x04;
	GPIOCALTFN0_VA = GPIOCOUT_VA + 0x20;
	GPIOCALTFN1_VA = GPIOCOUT_VA + 0x24;
	printk("GPIOCOUT_VA=%p, GPIOCOUTENB_VA=%p\n",GPIOCOUT_VA ,GPIOCOUTENB_VA);
	
	//10、使用虚拟地址
	//10.1 将引脚初始化成function1--GPIOC
	writel((readl(GPIOCALTFN0_VA)&~(3<<28)) | (1<<28),GPIOCALTFN0_VA);//GPIOC14-->func1->GPIO
	//10.2将CPIOC设置为OUTPUT
	writel(readl(GPIOCOUTENB_VA)|(1<<14),GPIOCOUTENB_VA);
	//10.3 设置GPIOC output 1，BEEP*4-->off
	writel(readl(GPIOCOUT_VA) &~(1<<14),GPIOCOUT_VA);

	printk("gec6818 beep driver init\n");
	return 0;
	
ioremap_error:
	release_mem_region(0xC001C000, 0x1000);	
request_mem_error:
	device_destroy(beep_class, beep_num);
device_create_error:
	class_destroy(beep_class);
class_create_error:
	cdev_del(&gec6818_beep_cdev);
cdev_add_error:
	unregister_chrdev_region(beep_num, 1);
	return ret;
}

static void __exit gec6818_beep_exit(void)
{
	iounmap(GPIOCOUT_VA);
	release_mem_region(0xC001C000, 0x1000);
	device_destroy(beep_class, beep_num);
	class_destroy(beep_class);
	cdev_del(&gec6818_beep_cdev);
	unregister_chrdev_region(beep_num, 1);
	printk("gec6818 beep driver exit\n");
}


module_init(gec6818_beep_init);
module_exit(gec6818_beep_exit);

MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("GEC6818 beep driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");
