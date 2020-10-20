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
#include <linux/ioport.h>
#include <linux/clk.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>


#define GEC6818_WDT_START		_IO('W',  1)
#define GEC6818_WDT_STOP		_IO('W',  2)
#define GEC6818_WDT_ALIVE		_IO('W',  3)


static struct resource *  wdt_res;
static void __iomem *wdt_base_va;
static void __iomem *wtcon_va; //0x00
static void __iomem *wtdat_va; //0x04
static void __iomem *wtcnt_va; //0x08
static void __iomem *wtclrint_va;//0x0c

static struct clk	*wdt_clock;

//1、定义一个文件操作集，并初始化
//filp --->内核中，指向驱动文件的指针
//cmd --->应用程序发下来的命令
//args --->应用程序发下来的参数
static long gec6818_wdt_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	switch(cmd){
	case GEC6818_WDT_START:
		//打开看门狗
		*(unsigned int *)wtcon_va = (255<<8)+(3<<3)+(1<<2)+(1<<0);
		*(unsigned int *)wtcon_va |= (1<<5);
		break;
		
	case GEC6818_WDT_STOP:
		//关闭看门狗	
		*(unsigned int *)wtcon_va = 0;	
		break;
		
	case GEC6818_WDT_ALIVE:
		//看门狗喂狗
		*(unsigned int *)wtcnt_va = 0xEE66;
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
	static unsigned long wdt_freq;
	//申请物理内存区
	wdt_res = request_mem_region(0xC0019000, 0x1000,"WDT_MEM");
	if(wdt_res == NULL){
		printk("request memory error\n");
		ret = -EBUSY;
		goto request_mem_error;
	}
	
	//IO内存动态映射，得到物理地址对应的虚拟地址
	wdt_base_va = ioremap(0xC0019000, 0x1000);
	if(wdt_base_va == NULL){
		printk("ioremap error\n");
		ret = -EBUSY;
		goto ioremap_error;
	}
	wtcon_va = wdt_base_va + 0x00; //0x00
	wtdat_va = wdt_base_va + 0x04; //0x04
	wtcnt_va = wdt_base_va + 0x08; //0x08
	wtclrint_va = wdt_base_va + 0x0C;//0x0c

	wdt_clock = clk_get(NULL, "pclk");
	if (IS_ERR(wdt_clock)) {
		printk("failed to find watchdog clock source\n");
		ret = PTR_ERR(wdt_clock);
		goto clk_get_error;
	}

	clk_enable(wdt_clock);
	wdt_freq = clk_get_rate(wdt_clock);
	printk("wdt_freq = %luHz\n", wdt_freq);
		
	nxp_soc_peri_reset_set(RESET_ID_WDT);
	nxp_soc_peri_reset_set(RESET_ID_WDT_POR);
	
	ret = misc_register(&gec6818_wdt_misc);
	if(ret < 0){
		printk("misc register is error\n");
		goto misc_register_error;		
	}
	
	//看门狗初始化
	*(unsigned int *)wtcon_va = 0;
	*(unsigned int *)wtdat_va = 0xEE66;//61030
	*(unsigned int *)wtcnt_va = 0xEE66;
	*(unsigned int *)wtclrint_va = 0;
	
	printk(KERN_WARNING "gec6818 wdt driver init \n");
	
	return 0;

		
misc_register_error:
	clk_disable(wdt_clock);
	clk_put(wdt_clock);
clk_get_error:
	iounmap(wdt_base_va);
ioremap_error:
	release_mem_region(0xC0019000, 0x1000);
request_mem_error:
	return ret;	
}

//出口函数--->卸载驱动
static void __exit gec6818_wdt_exit(void)
{
	iounmap(wdt_base_va);
	release_mem_region(0xC0019000, 0x1000);
	clk_disable(wdt_clock);
	clk_put(wdt_clock);	
	misc_deregister(&gec6818_wdt_misc);
	
	printk("<4>" "gec6818 wdt driver exit \n");
}

module_init(gec6818_wdt_init);
module_exit(gec6818_wdt_exit);

MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("wdt driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

