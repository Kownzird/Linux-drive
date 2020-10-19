#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
//1、定义一个动态定时器
static struct timer_list gec6818_timer;

void  gec6818_timer_fun(unsigned long data) //data=10
{
	printk("jiffies=%lu\n",jiffies);
	printk("data = %lu\n",data);
	mod_timer(&gec6818_timer, jiffies + 100); //周期性超时
}


//入口函数--->安装驱动
static int __init gec6818_led_init(void)
{
	init_timer(&gec6818_timer);
	
	gec6818_timer.function = gec6818_timer_fun;
	gec6818_timer.expires = jiffies + 100; //当前时间开始，100个jiffy后，会产生超时（100ms）
	gec6818_timer.data = 10;
	
	add_timer(&gec6818_timer);
	
	printk(KERN_WARNING "gec6818 led driver init \n");
	
	return 0;
}

//出口函数--->卸载驱动
static void __exit gec6818_led_exit(void)
{
	del_timer(&gec6818_timer);
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

