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

#define GEC6818_LED_ON		_IOW('L',  1, unsigned long)
#define GEC6818_LED_OFF		_IOW('L',  2, unsigned long)

//1.����һ��cdev
static struct cdev gec6818_led_cdev;

static unsigned int led_major = 0; //���豸��
static unsigned int led_minor = 0; //���豸��
static dev_t  led_num; //�豸��

static struct class * leds_class;
static struct device *leds_device;

static struct resource *  leds_res;
static void __iomem *gpioc_base_va;
static void __iomem *gpiocout_va;//0x00
static void __iomem *gpiocoutenb_va;//0x04
static void __iomem *gpiocaltfn0_va;//0x20
static void __iomem *gpiocaltfn1_va;//0x24
static void __iomem *gpiocpad_va;//0x18

//2������һ���ļ�������������ʼ��
//filp --->�ں��У�ָ�������ļ���ָ��
//cmd --->Ӧ�ó�������������
//args --->Ӧ�ó��������Ĳ���
static long gec6818_led_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	switch(cmd){
	case GEC6818_LED_ON:
		switch(args){
		case 8:
			writel(readl(gpiocout_va)&~(1<<17),gpiocout_va);
			break;
		case 9:
			writel(readl(gpiocout_va)&~(1<<8),gpiocout_va);
			break;
		case 10:
			writel(readl(gpiocout_va)&~(1<<7),gpiocout_va);
			break;
		case 11:
			writel(readl(gpiocout_va)&~(1<<12),gpiocout_va);
			break;
		default:
			return -EINVAL;
		}
		break;
	case GEC6818_LED_OFF:
		switch(args){
		case 8:
			writel(readl(gpiocout_va)|(1<<17),gpiocout_va);
			break;
		case 9:
			writel(readl(gpiocout_va)|(1<<8),gpiocout_va);
			break;
		case 10:
			writel(readl(gpiocout_va)|(1<<7),gpiocout_va);
			break;
		case 11:
			writel(readl(gpiocout_va)|(1<<12),gpiocout_va);
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

//��ں���--->��װ����
static int __init gec6818_led_init(void)
{
	int ret,temp;
	//3�������豸��
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
	
	//4.�ַ��豸�ĳ�ʼ��
	cdev_init(&gec6818_led_cdev, &gec6818_led_fops);
	
	//5.���ַ��豸�����ں�
	ret = cdev_add(&gec6818_led_cdev, led_num, 1);
	if(ret < 0){
		printk("can not add cdev \n");
		goto cdev_add_error;			
	}
	
	//6. ����class
	leds_class = class_create(THIS_MODULE, "gec210_leds");
	if(leds_class == NULL){
		printk("class create error\n");
		ret = -EBUSY;
		goto class_create_error;
	}
	
	//7.����device
	leds_device = device_create(leds_class,NULL,led_num, NULL, "led_drv");
	if(leds_device == NULL){
		printk("device create error\n");
		ret = -EBUSY;
		goto device_create_error;
	}
	
	//8���������ڴ���
	leds_res = request_mem_region(0xC001C000, 0x1000,"GPIOC_MEM");
	if(leds_res == NULL){
		printk("request memory error\n");
		ret = -EBUSY;
		goto request_mem_error;
	}
	
	//9.IO�ڴ涯̬ӳ�䣬�õ������ַ��Ӧ�������ַ
	gpioc_base_va = ioremap(0xC001C000, 0x1000);
	if(gpioc_base_va == NULL){
		printk("ioremap error\n");
		ret = -EBUSY;
		goto ioremap_error;
	}
	//�õ�ÿ���Ĵ����������ַ
	gpiocout_va = gpioc_base_va + 0x00;
	gpiocoutenb_va = gpioc_base_va + 0x04;
	gpiocaltfn0_va = gpioc_base_va + 0x20;
	gpiocaltfn1_va = gpioc_base_va + 0x24;
	gpiocpad_va = gpioc_base_va + 0x18;

	printk("gpiocout_va = %p,gpiocpad_va=%p\n",gpiocout_va, gpiocpad_va);
	
	//10.���������ַ
	//10.1 GPIOC7,8.12,17 --->function1,��Ϊ��ͨ��GPIO
	temp = readl(gpiocaltfn0_va);
	temp &=~((3<<14)|(3<<16)|(3<<24));
	temp |= ((1<<14)|(1<<16)|(1<<24));
	writel(temp,gpiocaltfn0_va);
	
	temp = readl(gpiocaltfn1_va);
	temp &=~(3<<2);
	temp |= (1<<2);
	writel(temp,gpiocaltfn1_va);
	
	//10.2 GPIOC7,8.12,17 --->����Ϊ���
	temp = readl(gpiocoutenb_va);
	temp |= ((1<<7)|(1<<8)|(1<<12)|(1<<17));
	writel(temp, gpiocoutenb_va);
	
	//10.3 GPIOC7,8.12,17 --->����Ϊ����ߵ�ƽ��D8~D11 off
	temp = readl(gpiocout_va);
	temp |= ((1<<7)|(1<<8)|(1<<12)|(1<<17));
	writel(temp,gpiocout_va);
	
	printk(KERN_WARNING "gec6818 led driver init \n");
	
	return 0;
	
ioremap_error:	
	release_mem_region(0xC001C000, 0x1000);
request_mem_error:	
	device_destroy(leds_class, led_num);
device_create_error:	
	class_destroy(leds_class);
class_create_error:		
	cdev_del(&gec6818_led_cdev);
cdev_add_error:	
	unregister_chrdev_region(led_num, 1);
	return ret;	
}

//���ں���--->ж������
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

//�����������ڣ�#insmod led_drv.ko -->module_init()-->gec6818_led_init()
module_init(gec6818_led_init);
//��������ĳ��ڣ�#rmmod led_drv.ko --->module_exit()-->gec6818_led_exit()
module_exit(gec6818_led_exit);

//module��������#modinfo led_drv.ko
MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("LED driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

