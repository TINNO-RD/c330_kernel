
#ifndef __AS6313_H
#define __AS6313_H

//AS6313设备地址
#define AS6313_SLAVE_ADDR 0x0d

//控制命令字
#define AS_IOCTL_SET_MODE    0x100     	//设置模式
#define AS_IOCTL_GET_MODE    0x101		//获取模式
#define AS_IOCTL_SET_IRQ     0x102		//中断设置

//模式选择
enum {
	AS_MODE_INVALID = -1,
	AS_MODE_USB = 0,           			//USB
	AS_MODE_OMTP,						//OMTP			
	AS_MODE_CTIA,						//CTIA
	AS_MODE_TRS,						//TRS
	AS_MODE_UART,						//UART
	AS_MODE_AUX,						//AUX
	AS_MODE_OFF,						//OFF
	AS_MODE_MAX
};

//中断设置
enum {
	AS_IRQ_ENABLE = 0,          		//使能中断
	AS_IRQ_DISABLE,             		//关闭中断
	AS_IRQ_MAX
};

//调试类型
enum {
	AS_DBG_TYPE_MODE = 0,     			//模式
	AS_DBG_TYPE_IRQ,					//中断
	AS_DBG_TYPE_REG0,					//寄存器0
	AS_DBG_TYPE_REG1,					//寄存器1
	AS_DBG_TYPE_MAX
};

#endif
