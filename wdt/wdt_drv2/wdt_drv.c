#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>

#include <cfg_type.h>
#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#define NXP_WDOGREG(x) ((x) + IO_ADDRESS(PHY_BASEADDR_WDT))

#define NXP_WTCON      NXP_WDOGREG(0x00)
#define NXP_WTDAT      NXP_WDOGREG(0x04)
#define NXP_WTCNT      NXP_WDOGREG(0x08)
#define NXP_WTCLRINT   NXP_WDOGREG(0x0c)

#define GEC6818_WDT_START		_IO('W',  1)
#define GEC6818_WDT_STOP		_IO('W',  2)
#define GEC6818_WDT_ALIVE		_IO('W',  3)

//1、定义一个文件操作集，并初始化
//filp --->内核中，指向驱动文件的指针
//cmd --->应用程序发下来的命令
//args --->应用程序发下来的参数
static long gec6818_wdt_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	switch(cmd){
	case GEC6818_WDT_START:
		//打开看门狗
		writel((255<<8)+(3<<3)+(1<<2)+(1<<0),NXP_WTCON);
		writel(readl(NXP_WTCON)|(1<<5),NXP_WTCON);
		break;
		
	case GEC6818_WDT_STOP:
		//关闭看门狗	
		writel(0x0,NXP_WTCON);	
		break;
		
	case GEC6818_WDT_ALIVE:
		//看门狗喂狗
		writel(0xEE66,NXP_WTCNT);
		break;
			
	default:
		return -ENOIOCTLCMD;
		
	}
	return 0;	
}

static const struct file_operations gec6818_wdt_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gec6818_wdt_ioctl,
};

//2、定义一个混杂设备，并初始化
static  struct miscdevice gec6818_wdt_misc = {
	.minor = MISC_DYNAMIC_MINOR, //动态分配次设备号
	.name = "wdt_drv",
	.fops = &gec6818_wdt_fops,
};


//入口函数--->安装驱动
static int __init gec6818_wdt_init(void)
{
	int ret;

	nxp_soc_peri_reset_set(RESET_ID_WDT);
	nxp_soc_peri_reset_set(RESET_ID_WDT_POR);
	
	ret = misc_register(&gec6818_wdt_misc);
	if(ret < 0){
		printk("misc register is error\n");
		return ret;		
	}
	
	//看门狗初始化
	writel(0,NXP_WTCON);
	writel(0xEE66,NXP_WTDAT);
	writel(0xEE66,NXP_WTCNT);
	writel(0xEE66,NXP_WTCLRINT);
	
	printk(KERN_WARNING "gec6818 wdt driver init \n");
	
	return 0;
}

//出口函数--->卸载驱动
static void __exit gec6818_wdt_exit(void)
{
	misc_deregister(&gec6818_wdt_misc);
	
	printk("<4>" "gec6818 wdt driver exit \n");
}

module_init(gec6818_wdt_init);
module_exit(gec6818_wdt_exit);

MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("wdt driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");