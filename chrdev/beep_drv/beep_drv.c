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
//1.����һ��cdev
static struct cdev gec6818_beep_cdev;

static unsigned int beep_major = 0; //���豸��
static unsigned int beep_minor = 0; //���豸��
static dev_t  beep_num; //�豸��

static struct class * beeps_class;
static struct device *beeps_device;

static struct resource *  beeps_res;
static void __iomem *gpioc_base_va;

static void __iomem *gpiocout_va;//0x00
static void __iomem *gpiocoutenb_va;//0x04
static void __iomem *gpiocaltfn0_va;
static void __iomem *gpiocaltfn1_va;//0x24

//2������һ���ļ�������������ʼ��
int gec6818_beep_open(struct inode *inode, struct file *filp)
{
	printk("beep driver openning\n");
	return 0;
}

//Ӧ�ó���write()-->ϵͳ����-->��������gec6818_beep_write()
//const char __user *user_buf ---->Ӧ�ó���д����������
//size_t size --->Ӧ�ó���д���������ݴ�С
//����һ�����ݸ�ʽ��user_buf[0]--->beep��״̬��1--beep on�� 0--beep off
ssize_t gec6818_beep_write(struct file *filp, const char __user *user_buf, size_t size, loff_t *off)
{
	//�����û�д���������ݣ���������Щ����������beep�ơ�
	char kbuf[1];
	int ret;
	if(size != 1)
		return -EINVAL;
	ret = copy_from_user(kbuf, user_buf, size);//�õ��û��ռ������
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

//��ں���--->��װ����
static int __init gec6818_beep_init(void)
{
	int ret;
	//3�������豸��
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
	
	//4.�ַ��豸�ĳ�ʼ��
	cdev_init(&gec6818_beep_cdev, &gec6818_beep_fops);
	
	//5.���ַ��豸�����ں�
	ret = cdev_add(&gec6818_beep_cdev, beep_num, 1);
	if(ret < 0){
		printk("can not add cdev \n");
		unregister_chrdev_region(beep_num, 1);
		return ret;		
	}
	
	//6. ����class
	beeps_class = class_create(THIS_MODULE, "gec210_beeps");
	if(beeps_class == NULL){
		printk("class create error\n");
		unregister_chrdev_region(beep_num, 1);
		cdev_del(&gec6818_beep_cdev);
		return -EBUSY;
	}
	
	//7.����device
	beeps_device = device_create(beeps_class,NULL,beep_num, NULL, "beep_drv");
	if(beeps_device == NULL){
		printk("device create error\n");
		class_destroy(beeps_class);
		cdev_del(&gec6818_beep_cdev);
		unregister_chrdev_region(beep_num, 1);
		
		return -EBUSY;
	}
	
	//8���������ڴ���
	beeps_res = request_mem_region(0xC001C000, 0x1000,"GPIOC_MEM");
	if(beeps_res == NULL){
		printk("request memory error\n");
		device_destroy(beeps_class, beep_num);
		class_destroy(beeps_class);
		cdev_del(&gec6818_beep_cdev);
		unregister_chrdev_region(beep_num, 1);
		
		return -EBUSY;
	}
	
	//9.IO�ڴ涯̬ӳ�䣬�õ������ַ��Ӧ�������ַ
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
	//�õ�ÿ���Ĵ����������ַ
	gpiocout_va = gpioc_base_va + 0x00;
	gpiocoutenb_va = gpioc_base_va + 0x04;
	gpiocaltfn0_va = gpioc_base_va + 0x20;
	gpiocaltfn1_va = gpioc_base_va + 0x24;

	
	//10.���������ַ
	//10.1 GPIOC7,8.12,17 --->function1,��Ϊ��ͨ��GPIO
	*(unsigned int *)gpiocaltfn1_va &=~(2<<28);
	//10.2 GPIOC7,8.12,17 --->����Ϊ���
	*(unsigned int *)gpiocoutenb_va |=(1<<14);
	//10.3 GPIOC7,8.12,17 --->����Ϊ����ߵ�ƽ��D8~D11 off
	//*(unsigned int *)gpiocout_va |= (1<<14);
	
	*(unsigned int *)gpiocout_va &= ~(1<<14);
	printk(KERN_WARNING "gec6818 beep driver init \n");
	
	return 0;
}

//���ں���--->ж������
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

//�����������ڣ�#insmod beep_drv.ko -->module_init()-->gec6818_beep_init()
module_init(gec6818_beep_init);
//��������ĳ��ڣ�#rmmod beep_drv.ko --->module_exit()-->gec6818_beep_exit()
module_exit(gec6818_beep_exit);

//module��������#modinfo beep_drv.ko
MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("beep driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

