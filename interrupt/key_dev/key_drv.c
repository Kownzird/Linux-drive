#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <cfg_type.h>

irqreturn_t keys_isr(int irq, void *dev)
{
	int key = *(int *)dev;
	printk("key = %d\n",key);
	if(key == 2)
		printk("key2 is pressing\n");
	else if(key == 3)
		printk("key3 is pressing\n");
		
	return 	IRQ_HANDLED;
}

	int key2 = 2;
	int key3 = 3;
//入口函数--->安装驱动
static int __init gec6818_key_init(void)
{
	int ret;

	ret = request_irq(IRQ_GPIO_A_START + 28, keys_isr, IRQF_TRIGGER_FALLING, 
		"key2_gpioa28", (void *)&key2);
	if(ret < 0){
		printk("can not request key2 gpioa28\n");
		return ret;		
	}
	ret = request_irq(IRQ_GPIO_B_START + 30, keys_isr, IRQF_TRIGGER_FALLING, 
		"key3_gpiob30", (void *)&key3);
	if(ret < 0){
		printk("can not request key2 gpioa28\n");
		goto request_irq;
			
	}
	printk(KERN_WARNING "gec6818 key driver init \n");
	
	return 0;
	
request_irq:	
	free_irq(IRQ_GPIO_A_START + 28, (void *)&key2);
	return ret;
}

//出口函数--->卸载驱动
static void __exit gec6818_key_exit(void)
{
	free_irq(IRQ_GPIO_A_START + 28, (void *)&key2);
	free_irq(IRQ_GPIO_B_START + 30, (void *)&key3);
	printk("<4>" "gec6818 key driver exit \n");
}

//驱动程序的入口：#insmod key_drv.ko -->module_init()-->gec6818_key_init()
module_init(gec6818_key_init);
//驱动程序的出口：#rmmod key_drv.ko --->module_exit()-->gec6818_key_exit()
module_exit(gec6818_key_exit);

//module的描述。#modinfo key_drv.ko
MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("key driver for GEC6818");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.0");

