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
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <cfg_type.h>


char key_buf[4] = {0,0,0,0};//按键的状态：0--K2，1--K3，2--K4，3--K6
//1、定义等待队列和等待条件
static int key_press_flag = 0;
static wait_queue_head_t gec6818_key_wq;


//定义GPIOC29的IO口号和名字
struct key_gpio_info{
	unsigned gpio_num;
	char gpio_name[20];
	unsigned int int_num;
	char int_name[20];
};

static const struct key_gpio_info key_info[4] = {
	{
		.gpio_num = PAD_GPIO_A + 28,
		.gpio_name = "K2_gpioA28_f0",
		.int_num = IRQ_GPIO_A_START + 28,
		.int_name = "key2"
	},
	{
		.gpio_num = PAD_GPIO_B + 30,
		.gpio_name = "K3_gpioB30_f0",
		.int_num = IRQ_GPIO_B_START + 30,
		.int_name = "key3"
	},
	{
		.gpio_num = PAD_GPIO_B + 31,
		.gpio_name = "K4_gpioB31_f0",
		.int_num = IRQ_GPIO_B_START + 31,
		.int_name = "key4"
	},
	{
		.gpio_num = PAD_GPIO_B + 9,
		.gpio_name = "K6_gpioB9_f0",
		.int_num = IRQ_GPIO_B_START + 9,
		.int_name = "key6"
	},
};

ssize_t gec6818_led_read(struct file *filp, char __user *user_buf, size_t size, loff_t *off)
{
	//将驱动程序的数据发送给应用程序，这个数据代表LED的状态<---gpiocout_va
	int ret;
	if(size != 4)
		return -EINVAL;
	//3.访问等待队列
	wait_event_interruptible(gec6818_key_wq, key_press_flag);
	ret = copy_to_user(user_buf, key_buf,size);
	if(ret != 0)
		return -EFAULT;
	
	//将等待条件设置回0
	key_press_flag = 0;
	for(i=0;i<4;i++)
		key_buf[i]=0;
	
	return size;
}


static struct file_operations gec6818_key_fps = {
	.owner = THIS_MODULE,
	.read = gec6818_led_read,
};


static  struct miscdevice gec6818_key_misc = {
	.minor = MISC_DYNAMIC_MINOR, //动态分配次设备号
	.name = "key_drv",
	.fops = &gec6818_key_fps,
};


irqreturn_t keys_isr(int irq, void *dev)
{
	int i;
	for(i=0;i<4;i++){
		if(irq == key_info[i].int_num)
			key_buf[i]=1; //按键按下

	}
	//将等待条件设置为真
	key_press_flag = 1;
	//4.唤醒等待队列中的进程
	wake_up(&gec6818_key_wq);
	
	return 	IRQ_HANDLED;
}

static int __init gec6818_key_init(void)
{
	int ret,i;
	
	//2.等待队列初始化
	init_waitqueue_head(&gec6818_key_wq);
	
	//申请中断
	for(i=0;i<4;i++){
		ret = request_irq(key_info[i].int_num, keys_isr,IRQF_TRIGGER_FALLING,
						key_info[i].int_name, NULL);
		if(ret < 0){
			printk("can not request irq--:%s\n",key_info[i].int_name);
			goto request_irq_error;			
		}
	
	}

	//注册混杂设备
	ret = misc_register( &gec6818_key_misc);
	if(ret < 0){
		printk("misc register error\n");
		goto misc_register_error;
	}
	
	printk("led_drv init success!");
	return 0;

misc_register_error:
	
request_irq_error:
	while(i--)
		free_irq(key_info[i].int_num,NULL);
	
	return ret;

}

static void __exit gec6818_key_exit(void)
{
	int i = 4;
	while(i--){
		free_irq(key_info[i].int_num,NULL);
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
