
/*
 * (C) Copyright 2010-2017
 * Accusilicon USA Inc., Ltd. <www.accusilicon.com>
 * yzhou <yzhou@accusilicon.com>
 *
 * 6313.c: AS6313 driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "as6313.h"


#define AS6313_CLK_GPIO   100  // should change to board's real GPIO
#define AS6313_DAT_GPIO   101  // should change to board's real GPIO

struct as6313_data {
	unsigned int clk_gpio;
	unsigned int cmd_gpio;
	unsigned int clk_level;
	unsigned int cmd_level;
	unsigned int flags;
	unsigned int hs_det_gpio;//lijian add
	int mode;
	spinlock_t  lock;
};

static struct as6313_data *as6313_priv;
//==================machine/board level codes====================================//

static void set_clkpin_output(int value) { // set clk gpio pin direction = out
    gpio_direction_output(as6313_priv->clk_gpio, value);
}

static void set_datpin_output(int value) { // set dat gpio pin direction = output 
    gpio_direction_output(as6313_priv->cmd_gpio, value);
}

static void set_datpin_input(void) { // set dat gpio pin direction = input
    gpio_direction_input(as6313_priv->cmd_gpio);
}

int get_dat(void) {   //read dat gpio value
    return gpio_get_value(as6313_priv->cmd_gpio);
}
//lijian add
static void set_hsdetpin_output(int value) { // set hsdet gpio pin direction = output 
	int err;
    	err = gpio_direction_output(as6313_priv->hs_det_gpio, value);
	pr_info("ssss--set hsdetpin= %d,err=%d\n", value,err);
}
//============higher level, read/write bit==================//
static int _slave_addr_7bit = AS6313_SLAVE_ADDR;

static void bitbang(int clk, int dat, int flag){
    if (clk) 
		set_clkpin_output(1); 
	else 
		set_clkpin_output(0);
	
	if (flag & GPIOF_DIR_IN) {
		set_datpin_input();
	} else {
		if (dat) 
			set_datpin_output(1); 
		else 
			set_datpin_output(0);
	}
    as6313_priv->cmd_level = dat;
    as6313_priv->clk_level = clk;
    as6313_priv->flags = flag;
}

static bool test_dat(void) {
    if (get_dat() == 0) return false;
    else return true;
}

/////////////////////////Basic IO functions read/write registers///////////////////////////
static void io_start(void) {
    bitbang(1,1,GPIOF_DIR_OUT);  //CLK = 1, DAT = 1;
    bitbang(1,0,GPIOF_DIR_OUT);  //DAT = 0;
    bitbang(0,0,GPIOF_DIR_OUT);  //CLK = 0;
}

static void io_stop(void) {
    bitbang(0,0,GPIOF_DIR_OUT);  // CLK = 0, DAT = 0;
    bitbang(1,0,GPIOF_DIR_OUT);  // CLK = 1;
    bitbang(1,1,GPIOF_DIR_OUT);  // DAT = 1;
}

static void io_write_byte(int data) {
   int i;
   bitbang(0,as6313_priv->cmd_level,GPIOF_DIR_OUT);  //CLK = 0;

   for (i=0;i<8;i++) {
		if ((data & 0x80)) 
			bitbang(0,1,GPIOF_DIR_OUT); 
		else 
			bitbang(0,0,GPIOF_DIR_OUT); // Output the data bit
		data  <<= 1;                                     // Shift the byte by one bit
		bitbang(1,as6313_priv->cmd_level,GPIOF_DIR_OUT);
        bitbang(0,as6313_priv->cmd_level,GPIOF_DIR_OUT);                           // CLK ==>1 ==>0
   }
   bitbang(0,as6313_priv->cmd_level,GPIOF_DIR_IN);  //get ACK
   bitbang(1,as6313_priv->cmd_level,GPIOF_DIR_IN);  //get ACK
}

static void io_write_reg0(int data)
{
   int addr = _slave_addr_7bit << 1;
   unsigned long flags = 0;
   
   spin_lock_irqsave(&as6313_priv->lock, flags);
   io_start();
   io_write_byte(addr);    //slave addr
   io_write_byte(0);       //addr 0, reg 0
   io_write_byte(data);    //write data
   io_stop();
   spin_unlock_irqrestore(&as6313_priv->lock, flags);
}

static void io_write_reg1(int data)
{
   int addr = _slave_addr_7bit << 1;
   unsigned long flags = 0;
   
   spin_lock_irqsave(&as6313_priv->lock, flags);
   io_start();
   io_write_byte(addr);    //slave addr
   io_write_byte(1);       //addr 0, reg 0
   io_write_byte(data);    //write data
   io_stop();
   spin_unlock_irqrestore(&as6313_priv->lock, flags);
}

static void io_disable_irq(void) {
   io_write_reg1(1);
}

static void io_irq_clear(void) {
   io_write_reg1(0);
}

static int io_read_byte(void)
{
   int i, data;
   bitbang(0,as6313_priv->cmd_level,GPIOF_DIR_IN);  //CLK = 0; output disabled
   data=0;
   for (i = 0; i < 8; i++) {
       bitbang(1,as6313_priv->cmd_level,GPIOF_DIR_IN);
       if(test_dat())
           data |=1;
       if(i<7)
           data<<=1;
       bitbang(0,as6313_priv->cmd_level,GPIOF_DIR_IN);
    }

    bitbang(0,1,GPIOF_DIR_OUT);  //send NACK
    bitbang(1,1,GPIOF_DIR_OUT);  //send NACK
    bitbang(0,1,GPIOF_DIR_OUT);  //send NACK

    return data;
}

static int io_read_reg0(void)
{
   int data;
   int addr = _slave_addr_7bit << 1;
   unsigned long flags = 0;
   
   spin_lock_irqsave(&as6313_priv->lock, flags);
   io_start();
   io_write_byte(addr | 1);     //slave addr
   io_write_byte(0);            //addr 0, reg 0
   data = io_read_byte();   //read data
   io_stop();
   spin_unlock_irqrestore(&as6313_priv->lock, flags);
   return data;
}


static int io_read_reg1(void)
{
   int data;
   int addr = _slave_addr_7bit << 1;
   unsigned long flags = 0;
   
   spin_lock_irqsave(&as6313_priv->lock, flags);
   io_start();
   io_write_byte(addr | 1);    //slave addr
   io_write_byte(1);           //addr 1, reg1
   data = io_read_byte();  //read data
   io_stop();
   spin_unlock_irqrestore(&as6313_priv->lock, flags);
   return data;
}

//===============Applications=================//
int as6313_get_reg0(void) { return io_read_reg0(); }

int as6313_get_reg1(void) { return io_read_reg1(); }

void as6313_set_reg0(int data) { io_write_reg0(data & 7); }

void as6313_set_reg1(int data) { io_write_reg1(data & 1); }

void as6313_USB(void)
{
	io_write_reg0(0); 
	set_hsdetpin_output(1);//lijian add
	pr_info("ssss--usb----as6313_get_reg0= %d\n", as6313_get_reg0());
}   //back to USB mode
EXPORT_SYMBOL_GPL(as6313_USB);

void as6313_OMTP(void)   //enter OMTP mode
{
    io_write_reg0(7);
    io_write_reg0(1);
	msleep(100);
    set_hsdetpin_output(0);//lijian add
    pr_info("ssss--omtp---as6313_get_reg0= %d\n", as6313_get_reg0());
}
EXPORT_SYMBOL_GPL(as6313_OMTP);

void as6313_CTIA(void)    //enter CTIA mode
{
    io_write_reg0(7);
    io_write_reg0(2);
	msleep(100);
    set_hsdetpin_output(0);//lijian add
    pr_info("ssss--ctia---as6313_get_reg0= %d\n", as6313_get_reg0());	
}
EXPORT_SYMBOL_GPL(as6313_CTIA);

void as6313_TRS(void)  // enter TRS mode
{
    io_write_reg0(7);
    io_write_reg0(3);
}

void as6313_UART(void)    //enter UART mode
{
    io_write_reg0(7);
    io_write_reg0(5);
}

void as6313_AUX(void)     //enter AUX mode
{
    io_write_reg0(7);
    io_write_reg0(6);
}

void as6313_OFF(void)        //enter OFF mode
{
    io_write_reg0(7);
}

void as6313_disable_irq(void)   //disable IRQ
{
    io_disable_irq(); 
}

void as6313_clear_irq(void)    //clear IRQ
{
   io_irq_clear();
}

static int as6313_set_mode(int mode) 
{
	pr_info("set mode %d\n", mode);
	switch (mode) {
		case AS_MODE_USB:
			as6313_USB();
			break;
		case AS_MODE_OMTP:
			as6313_OMTP();
			break;
		case AS_MODE_CTIA:
			as6313_CTIA(); 
			break;
		case AS_MODE_TRS:
			as6313_TRS(); 
			break;
		case AS_MODE_UART:
			as6313_UART();
			break;
		case AS_MODE_AUX:
			as6313_AUX(); 
			break;
		case AS_MODE_OFF:
			as6313_OFF();
			break;
		default:
			pr_warn("Invalid mode %d\n", mode);
			return -1;
	}
	as6313_priv->mode = mode;
	return 0;
}

static int as6313_handle_irq(int oper)
{
	pr_info("handle irq %d\n", oper);
	switch (oper) {
		case AS_IRQ_ENABLE:
			as6313_clear_irq();
			break;
		case AS_IRQ_DISABLE:
		    as6313_disable_irq();
			break;
		default:
			pr_warn("Invalid oper %d\n", oper);
			return -1;
	}
	return 0;
}

static int as6313_get_mode(void)
{
	int value = as6313_get_reg0();
	if (value > -1 && value <= 7) {
		int mode = value & 0x07;
		switch (mode) {
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
				as6313_priv->mode = mode;
				break;
			case 0x05:
			case 0x06:
			case 0x07:
				as6313_priv->mode = mode - 1;
				break;
			default:
				break;
		}				
	}
	return as6313_priv->mode;
}

static int as6313_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int as6313_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long  as6313_ioctl(struct file *filep, unsigned int cmd,
                unsigned long arg)
{
	long err = 0;
	int mode;
	void __user *argp = (void __user *)arg;
	
	switch(cmd) {
		case AS_IOCTL_SET_MODE:
			err = as6313_set_mode(arg);
			break;
		case AS_IOCTL_GET_MODE:
		    mode = as6313_get_mode();
			if(copy_to_user(argp, &mode, sizeof(int)))
				err = -EFAULT;
			break;
		case AS_IOCTL_SET_IRQ:
			err = as6313_handle_irq(arg);
		    break;
		default:
		    pr_warn("Invalid cmd 0x%03x\n", cmd);
			break;
	}
	return err;
}

static int as6313_gpio_init(struct device *dev)
{
	int err;
	
	pr_info("as6313 gpio init\n");
	
	as6313_priv = kzalloc(sizeof(struct as6313_data),
				GFP_KERNEL);
	if (!as6313_priv) {
		pr_err("%s kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	
	as6313_priv->clk_gpio = of_get_named_gpio(dev->of_node, "clk-gpio", 0);
	if (as6313_priv->clk_gpio < 0) {		
		pr_err("Looking up clk-gpio property in node %s failed %d\n",				
			dev->of_node->full_name, as6313_priv->clk_gpio);	
		goto gpio_init_err;	
	}	
	
	err = gpio_request_one(as6313_priv->clk_gpio, GPIOF_OUT_INIT_HIGH, NULL);
	pr_info("request gpio%d, ret=%d\n", as6313_priv->clk_gpio, err);
	if (err) {
		pr_err("[%s] request gpio%d failed\n", __FUNCTION__, 
			as6313_priv->clk_gpio);
	}
	
	as6313_priv->cmd_gpio = of_get_named_gpio(dev->of_node, "cmd-gpio", 0);
	if (as6313_priv->cmd_gpio < 0) {		
		pr_err("Looking up clk-gpio property in node %s failed %d\n",				
			dev->of_node->full_name, as6313_priv->cmd_gpio);	
		goto gpio_init_err;	
	}
	
	err = gpio_request_one(as6313_priv->cmd_gpio, GPIOF_OUT_INIT_HIGH, NULL);
	pr_info("request gpio%d, ret=%d\n", as6313_priv->cmd_gpio, err);
	if (err) {
		pr_err("[%s] request gpio%d failed\n", __FUNCTION__, 
			as6313_priv->cmd_gpio);
	}

	//lijian add
	as6313_priv->hs_det_gpio = of_get_named_gpio(dev->of_node, "hs-dec-gpio", 0);
	pr_info("ssss--hs_det_gpio=%d\n", as6313_priv->hs_det_gpio);
	if (as6313_priv->hs_det_gpio < 0) {	
		pr_err("Looking up hs-dec-gpio property in node %s failed %d\n",
			dev->of_node->full_name, as6313_priv->hs_det_gpio);
		goto gpio_init_err;
	}
	
	err = gpio_request_one(as6313_priv->hs_det_gpio, GPIOF_OUT_INIT_HIGH, NULL);
	pr_info("ssss---request gpio%d, ret=%d\n", as6313_priv->hs_det_gpio, err);
	if (err) {
		pr_err("[%s] request gpio%d failed\n", __FUNCTION__, 
			as6313_priv->hs_det_gpio);
	}
	//lijian end
	
	as6313_priv->clk_level = 1;
	as6313_priv->cmd_level = 1;
	as6313_priv->flags = GPIOF_DIR_OUT;
	as6313_priv->mode = AS_MODE_INVALID;
	spin_lock_init(&as6313_priv->lock);
gpio_init_err:
	return err;
}

static ssize_t sysfs_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf, u32 type)
{
	int value = 0;
	
	switch (type) {
		case AS_DBG_TYPE_MODE:
			value = as6313_get_mode();
			break;
		case AS_DBG_TYPE_IRQ:
			value = as6313_get_reg1() & 0x01;
		    break;
		case AS_DBG_TYPE_REG0:
			value = as6313_get_reg0();
			break;
		case AS_DBG_TYPE_REG1:
			value = as6313_get_reg1();
			break;
		default:
			pr_warn("%s: invalid type %d\n", __func__, type);
		    break;
	}
	return sprintf(buf, "0x%04x\n", value);
}

static ssize_t sysfs_set(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count, u32 type)
{
	int err;
	unsigned long value;
	
	err = kstrtoul(buf, 10, &value);
	if (0) {
		pr_warn("%s: get data of type %d failed\n", __func__, type);
		return err;
	}
	
	pr_info("%s: set type %d, data %ld\n", __func__, type, value);
	switch (type) {
		case AS_DBG_TYPE_MODE:
			as6313_set_mode(value);
			break;
		case AS_DBG_TYPE_IRQ:
		    as6313_handle_irq(value);
		    break;
		case AS_DBG_TYPE_REG0:
			as6313_set_reg0(value);
			break;
		case AS_DBG_TYPE_REG1:
			as6313_set_reg1(value);
			break;
		default:
			pr_warn("%s: invalid type %d\n", __func__, type);
		    break;
	}
	return count;
}

#define AS6313_DEVICE_SHOW(_name, _type) static ssize_t \
show_##_name(struct device *dev, \
			  struct device_attribute *attr, char *buf) \
{ \
	return sysfs_show(dev, attr, buf, _type); \
}

#define AS6313_DEVICE_SET(_name, _type) static ssize_t \
set_##_name(struct device *dev, \
			 struct device_attribute *attr, \
			 const char *buf, size_t count) \
{ \
	return sysfs_set(dev, attr, buf, count, _type); \
}

#define AS6313_DEVICE_SHOW_SET(name, type) \
AS6313_DEVICE_SHOW(name, type) \
AS6313_DEVICE_SET(name, type) \
static DEVICE_ATTR(name, S_IWUSR | S_IRUGO, show_##name, set_##name);

AS6313_DEVICE_SHOW_SET(as6313_mode, AS_DBG_TYPE_MODE);
AS6313_DEVICE_SHOW_SET(as6313_handle_irq, AS_DBG_TYPE_IRQ);
AS6313_DEVICE_SHOW_SET(as6313_reg0, AS_DBG_TYPE_REG0);
AS6313_DEVICE_SHOW_SET(as6313_reg1, AS_DBG_TYPE_REG1);

static struct attribute *as6313_attrs[] = {
	&dev_attr_as6313_mode.attr,
	&dev_attr_as6313_handle_irq.attr,
	&dev_attr_as6313_reg0.attr,
	&dev_attr_as6313_reg1.attr,
	NULL
};

static const struct attribute_group as6313_group = {
	.attrs = as6313_attrs,
};

static struct file_operations as6313_ops = {
	.owner			= THIS_MODULE,
	.open			= as6313_open,
	.release		= as6313_close, 
	.unlocked_ioctl	= as6313_ioctl,
};

static struct miscdevice as6313_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name =	"as_sbu",
	.fops = &as6313_ops,
};

static int as6313_probe(struct platform_device *pdev) {
	int err;

	pr_info("%s: register as6313 driver init\n", __func__);
	
	as6313_gpio_init(&pdev->dev);
	err = misc_register(&as6313_dev);
	if(err) {
		pr_err("%s: register as6313 driver as misc device error\n", __func__);
		goto exit_init;
	}
	
	err=sysfs_create_group(&as6313_dev.this_device->kobj, &as6313_group);
	if(err) {
                pr_err("%s: create attr error %d\n", __func__, err);
		goto exit_init;
	}
	set_hsdetpin_output(1);//add by lijian init hsdet pin 
exit_init:
	return err;
}

static int as6313_remove(struct platform_device *pdev) {
    pr_info("%s: exit\n", __func__);
	
	sysfs_remove_group(&as6313_dev.this_device->kobj, &as6313_group);
    misc_deregister(&as6313_dev);
	gpio_free(as6313_priv->clk_gpio);
	gpio_free(as6313_priv->cmd_gpio);
	gpio_free(as6313_priv->hs_det_gpio);
	kfree(as6313_priv);
	return 0;
}

static const struct of_device_id of_as6313_match[] = {
	{ .compatible = "accusilicon,as6313", },
	{},
};
static struct platform_driver as6313_driver = {
	.probe		= as6313_probe,
	.remove		= as6313_remove,
	.driver		= {
		.name		= "as6313",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(of_as6313_match),
	},
};

module_platform_driver(as6313_driver);
MODULE_AUTHOR("Yi Zhou");
MODULE_DESCRIPTION("Accusilicon AS6313 driver");
MODULE_LICENSE("GPL");

