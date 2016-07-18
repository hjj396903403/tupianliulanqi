#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

static struct i2c_client *i2cClient = NULL;
static unsigned short const i2c_addr_list[] = 
{ 
	0x38, I2C_CLIENT_END
};//��ַ����

/*
 *��ʲô�豸��Ϣ��������
 *��������������������
 *��������:�ҵ��豸��ַ�Ƕ���
 *����һ�����������ҵ�������
 */

static int __init i2c_dev_init(void)
{
	struct i2c_adapter *i2c_adap;//��ȡ�������ߴ��������ṹ��
	struct i2c_board_info i2c_info;//�豸�����ṹ�壬�����������豸�����ֻ��е�ַ

	
	/*��ȡIIC������*/
	i2c_adap = i2c_get_adapter(1);

	/*��սṹ��*/
	memset(&i2c_info,0,sizeof(struct i2c_board_info));

	/*���Ƶĸ�ֵ*/
	strlcpy(i2c_info.type,"my_iic",I2C_NAME_SIZE);

	/*��ȡ�жϱ��*/
	i2c_info.irq=gpio_to_irq(EXYNOS4_GPX1(6));
	/*����IIC�豸--�ͻ���*/
	i2cClient = i2c_new_probed_device(i2c_adap,&i2c_info,i2c_addr_list,NULL);
	
	/*����ģ����Ϣ*/
	i2c_put_adapter(i2c_adap);

	printk("i2c_dev_init!!\n");
	return 0;
}


static void __exit i2c_dev_exit(void)//ƽ̨�豸�˵ĳ��ں���
{
	printk(" i2c_dev_exit ok!!\n");

	/*�ͷ�*/
	i2c_release_client(i2cClient);
}


module_init(i2c_dev_init);
module_exit(i2c_dev_exit);
MODULE_LICENSE("GPL");

