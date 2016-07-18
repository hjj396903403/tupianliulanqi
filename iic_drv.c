#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>     // copy_
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/input.h>

static struct i2c_client *my_client;
static struct work_struct my_work;
static int x,y;
static int zuobiao[3];
static int condition;
//unsigned long zuobiao;
/*1. ��ʼ���ȴ�����ͷ*/

static struct input_dev *myinput = NULL; //������ϵͳ�Ľṹ��

static DECLARE_WAIT_QUEUE_HEAD(my_wait);

static const struct i2c_device_id i2c_id[] =
{
	{"my_iic",0},//�豸�˵�����Ϊ"myiic",0��ʾ����Ҫ˽������
	{}
};

void work_func(struct work_struct *work)
{

	unsigned char buff[15];
	i2c_smbus_read_i2c_block_data(my_client,0x00,15,buff);

	x=(buff[3]&0xF)<<8|buff[4];
	y=(buff[5]&0xF)<<8|buff[6];
//	printk("x=%d  y=%d\n",x,y);
	condition=1;
	zuobiao[0]=x;
	zuobiao[1]=y;
	zuobiao[2]=buff[2];
//	printk("zuobiao[0]=%d  zuobiao[1]=%d  zuobiao[2]=%d\n",zuobiao[0],zuobiao[1],zuobiao[2]);

	/* value:���Ϊ0��ʾ�ɿ� 1��ʾ���� */
	input_report_abs(myinput,ABS_X,x);
	input_report_abs(myinput,ABS_Y,y);
	if(!buff[2])
	{
		/*�ϱ�����ʱѹ��ֵ�Ͱ���ֵ*/
		input_report_abs(myinput,ABS_PRESSURE,0);
		input_report_key(myinput,BTN_TOUCH,0);
	}
	else
	{
		/*�ϱ��ɿ�ʱѹ��ֵ�Ͱ���ֵ*/
		input_report_abs(myinput,ABS_PRESSURE,1);
		input_report_key(myinput,BTN_TOUCH,1);
	}
	/* ͬ���¼� */
	input_sync(myinput);
	wake_up(&my_wait);
	
}
irqreturn_t irq_handler(int irq, void *dev)
{
	schedule_work(&my_work);
//	printk("�����ж�\n");
	return IRQ_HANDLED;
}

static int i2c_probe(struct i2c_client *client, const struct i2c_device_id *device_id)//ƥ��ɹ�ʱ����
{
	my_client=client;

	myinput = input_allocate_device();
	input_set_capability(myinput,EV_ABS,ABS_X);
	input_set_capability(myinput,EV_ABS,ABS_Y);
	input_set_capability(myinput,EV_ABS,ABS_PRESSURE);
	input_set_capability(myinput,EV_KEY,BTN_TOUCH);

	input_set_abs_params(myinput,ABS_X,0,799,0,0);
	input_set_abs_params(myinput,ABS_Y,0,479,0,0);
	input_set_abs_params(myinput,ABS_PRESSURE,0,1,0,0);
	/* 3��ע��input_dev�ṹ�� */
	input_register_device(myinput);
	
	printk("i2c_probe!!!\n");
	printk("������IICƥ��ĵ�ַ=0x%x\n",client->addr);
	if(i2c_smbus_read_byte_data(client,0xA5))
	{
		i2c_smbus_write_byte_data(client,0xA5,0);
		printk("��Դ����!!!\n");
	}
	request_irq(client->irq,irq_handler,IRQ_TYPE_EDGE_FALLING,client->name,NULL);
	printk("irq=%d\n",client->irq);
	INIT_WORK(&my_work,work_func);
	return 0;
}


static int i2c_remove(struct i2c_client *client)
{
	printk("i2c_remove!!!\n");
	free_irq(client->irq,NULL);

	/* ж�� */
	input_unregister_device(myinput);
	input_free_device(myinput); /*�ͷ�������ϵͳ�ṹ��*/
	
	return 0;
}

struct i2c_driver i2c_drv =
{
	.driver = //�������ӻᱨ��ʵ����û����
	{
		.name = "XL",
		.owner = THIS_MODULE,
	},	
	.probe = i2c_probe,//̽�⺯��
	.remove = i2c_remove, //��Դж��
	.id_table = i2c_id,//������һ�����ֵĲ�������ƥ���豸������
};

static long tiny4412_unlocked_ioctl(struct file *my_file, unsigned int cmd, unsigned long argv) // 2.6֮�������
{	
//	printk("ioctl�ɹ�����\n");
	copy_to_user(argv,zuobiao,sizeof(zuobiao));
//	printk("sizeof=%d\n",sizeof(zuobiao));
	return 0;
}
unsigned int tiny4412_poll(struct file *my_file, struct poll_table_struct *my_table)
{
	poll_wait(my_file,&my_wait,my_table);
	if(condition==0)
	{		
		return 0;
	}
	condition=0;
	return POLLIN;
}

static struct file_operations tiny4412_ops={
				
		.unlocked_ioctl=tiny4412_unlocked_ioctl,
		.poll=tiny4412_poll,
};		


struct miscdevice my_touch={
	.minor=255,  //�����豸�Ĵ��豸��  255��ʾ�Զ�����
	.name="lcd_touch",   //�豸ע�������  /dev
	.fops=&tiny4412_ops, //�ļ����� �CӦ�ò��������ʹ��

};
/*iic������*/
static int __init i2c_drv_init(void)
{
	
	
	
	i2c_add_driver(&i2c_drv);//��iic����ע��һ������
	misc_register(&my_touch);
	return 0;
}

static void __exit i2c_drv_exit(void)//ƽ̨�豸�˵ĳ��ں���
{
	
	i2c_del_driver(&i2c_drv);
	misc_deregister(&my_touch);
}

module_init(i2c_drv_init);
module_exit(i2c_drv_exit);
MODULE_LICENSE("GPL");


