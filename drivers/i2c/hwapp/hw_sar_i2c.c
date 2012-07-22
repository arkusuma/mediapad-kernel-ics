/***********************************************************************
  版权信息: 版权所有(C) 1988-2010, 华为技术有限公司.
  文件名: 
  作者: 
  版本: 
  创建日期: 2010-09-28
  完成日期: 
  功能描述: SAR主动上报的消息通过netlink发送到应用层。该文件主要实现netlink的接口
    
  主要函数列表: 
     
  修改历史记录列表: 
    <作  者>    <修改时间>  <版本>  <修改描述>
    
  备注: 
===========================================================================*/

/*************************头文件引用*******************************/
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/fs.h>


#include "hw_sar.h"
/*************************宏定义*************************************/
static UINT8 g_ucSarPrintLimit = enSarPrintLevelDebug;
static struct i2c_client *g_client = NULL;
INT32 ad7156_exist = 1;
/************************************************************************ 
  名称     : sar_ad715x_i2c_read
  描述     : AD715X i2c读函数
  参数名          类型            输入/输出            描述

  返回值   :   : 成功
                          : 失败  
************************************************************************/
INT32 sar_ad715x_i2c_read(struct i2c_client *pstClient, UINT8 ucReg, UINT8 *pucData, INT32 iLen)
{
    INT32 iRet = SAR_ERROR;

    iRet = i2c_master_send(pstClient, &ucReg, 1);
    if (iRet < 0) {
    	dev_err(&pstClient->dev, "I2C write error\n");
    	return iRet;
    }

    iRet = i2c_master_recv(pstClient, pucData, iLen);
    if (iRet < 0) {
    	dev_err(&pstClient->dev, "I2C read error\n");
    	return iRet;
    }

    return iRet;
}
/************************************************************************ 
  名称     : sar_ad715x_i2c_write
  描述     : 通过I2C接口将数据写入到AD715X
  参数名          类型            输入/输出            描述

  返回值   :   : 成功
                          : 失败  
************************************************************************/
INT32 sar_ad715x_i2c_write(struct i2c_client *pstClient, UINT8 ucReg, UINT8 ucData)
{
    INT32 iRet = SAR_ERROR;

    UINT8 aucTx[2] = 
    {
    	ucReg,
    	ucData,
    };

    iRet = i2c_master_send(pstClient, aucTx, 2);
    if (iRet < 0)
    	dev_err(&pstClient->dev, "I2C write error\n");

    return iRet;
}

/************************************************************************ 
  名称     : sar_get_gpio_status
  描述     : 获取gpio 管脚的状态
  参数名          类型            输入/输出            描述
  iGpioNum             INT32               IN                               GPIO 的序号
  piGpioStatus         INT32 *            OUT                             GPIO 的状态0:的电平;1:高电平
  返回值   :   : 成功
                          : 失败  
************************************************************************/
INT32 sar_get_gpio_status(INT32 iGpioNum)
{
    INT32 iGpioStatus;
    
    iGpioStatus = gpio_get_value(iGpioNum);
    return iGpioStatus;
}



/************************************************************************
  名称     : sar_printk
  描述     : sar 模块内核层的打印函数
  参数名          类型            输入/输出            描述
  ucGpsPrintLevel   UINT8                     IN                          打印级别
  szFmt                  STRING                  IN                          输入的打印格式化参数
  返回值   :   : 成功
                          : 失败  
************************************************************************/
INT32 sar_printk(UINT8 ucSarPrintLevel, STRING szFmt, ...)
{
    va_list stArgList;

    if (ucSarPrintLevel <= g_ucSarPrintLimit)
    {
        va_start(stArgList, szFmt);
        vprintk(szFmt, stArgList);
        va_end(stArgList);
    }

    return SAR_OK;
}
EXPORT_SYMBOL_GPL(sar_printk);

static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}

static ssize_t state_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	int size;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	char tmp[50];
	memset(buffer,0,SAR_AD715X_REG_NUM);
	memset(tmp,0,50);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("state_attr_show:read error\n");
		val = -1;
	}
	else
	{
		sprintf(tmp, "status: 0x%x\n", buffer[0]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch1 data: %d ,0x%x\n", buffer[1]*256+buffer[2],buffer[1]*256+buffer[2]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch2 data: %d ,0x%x\n", buffer[3]*256+buffer[4],buffer[3]*256+buffer[4]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch1 avg: %d ,0x%x\n", buffer[5]*256+buffer[6],buffer[5]*256+buffer[6]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch2 avg: %d ,0x%x\n", buffer[7]*256+buffer[8],buffer[7]*256+buffer[8]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch1 th: %d ,0x%x\n", buffer[9]*256+buffer[10],buffer[9]*256+buffer[10]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch1 setup: 0x%x\n", buffer[11]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch2 th: %d ,0x%x\n", buffer[12]*256+buffer[13],buffer[12]*256+buffer[13]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch2 setup: 0x%x\n", buffer[14]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "config: 0x%x\n", buffer[15]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "PDT: 0x%x\n", buffer[16]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch1 CAP: 0x%x\n", buffer[17]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "ch2 CAP: 0x%x\n", buffer[18]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "SN3: 0x%x\n", buffer[19]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "SN2: 0x%x\n", buffer[20]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "SN1: 0x%x\n", buffer[21]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "SN0: 0x%x\n", buffer[22]);
		strcat(buf,tmp);
		memset(tmp,0,50);
		sprintf(tmp, "chip ID: 0x%x\n", buffer[23]);
		strcat(buf,tmp);
	}	
		size = strlen(buf);
	  return size;
 }

static struct kobj_attribute state_attribute =
         __ATTR(state, 0664, state_attr_show, NULL);


static ssize_t gpio_nv_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	INT32 iGpioStatus;
	iGpioStatus = sar_get_gpio_status(138);
	
	return sprintf(buf, "%d\n", iGpioStatus);		  
 }

static struct kobj_attribute gpio_nv_attribute =
         __ATTR(gpio_nv, 0664, gpio_nv_attr_show, NULL);

static ssize_t ch1_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);	
		
		if(ret < 0)
		{
			printk("ch1_attr_show:read error\n");
			val = -1;
		}
		else
		{
			val = buffer[SAR_AD715X_REG_CH1_DATA_HIGH]*256 + buffer[SAR_AD715X_REG_CH1_DATA_LOW];
//			printk("ch1_attr_show:read val %d(0x%x)\n",val,val);
		}
		
	  return sprintf(buf, "%d\n", val);		  
 }

static struct kobj_attribute ch1_attribute =
         __ATTR(ch1, 0664, ch1_attr_show, NULL);

static ssize_t ch2_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);	
	if(ret < 0)
	{
		printk("ch2_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH2_DATA_HIGH]*256 + buffer[SAR_AD715X_REG_CH2_DATA_LOW];
//		printk("ch2_attr_show:read val %d(0x%x)\n",val,val);
	}
		
	return sprintf(buf, "%d\n", val);		  
 }

 static struct kobj_attribute ch2_attribute =
         __ATTR(ch2, 0664, ch2_attr_show, NULL);

 
static ssize_t setup1_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("setup1_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH1_SETUP];
//		printk("setup1_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t setup1_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	UINT8 val;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH1_SETUP,val);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH1_SETUP);
		return ret;
	}
	
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute setup1_attribute =
         __ATTR(setup1, 0664, setup1_attr_show, setup1_attr_store);

static ssize_t setup2_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("setup2_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH2_SETUP];
//		printk("setup2_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t setup2_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	UINT8 val;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH2_SETUP,val);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH2_SETUP);
		return ret;
	}
	
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute setup2_attribute =
         __ATTR(setup2, 0664, setup2_attr_show, setup2_attr_store);

static ssize_t config_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("config_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CFG];
//		printk("config_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t config_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	UINT8 val;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);
	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CFG,val);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CFG);
		return ret;
	}
	
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute config_attribute =
         __ATTR(config, 0664, config_attr_show, config_attr_store);

static ssize_t PDT_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("PDT_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_PD_TIMER];
//		printk("PDT_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t PDT_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	UINT8 val;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_PD_TIMER,val);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_PD_TIMER);
		return ret;
	}
	
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute PDT_attribute =
         __ATTR(PDT, 0664, PDT_attr_show, PDT_attr_store);

static ssize_t CAP1_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("CAP1_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH1_CAPDAC];
//		printk("CAP1_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t CAP1_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	UINT8 val;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);
	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH1_CAPDAC,val);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH1_CAPDAC);
		return ret;
	}
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute CAP1_attribute =
         __ATTR(CAP1, 0664, CAP1_attr_show, CAP1_attr_store);

static ssize_t CAP2_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("CAP2_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH2_CAPDAC];
//		printk("CAP2_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t CAP2_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	UINT8 val;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH2_CAPDAC,val);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH2_CAPDAC);
		return ret;
	}
	
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute CAP2_attribute =
         __ATTR(CAP2, 0664, CAP2_attr_show, CAP2_attr_store);

static ssize_t th1_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("th1_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH1_THR_HOLD_H]*256 + buffer[SAR_AD715X_REG_CH1_THR_HOLD_L];
//		printk("th1_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "%d\n", val);
}
static ssize_t th1_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	INT16 val;
	UINT8 val_h = 0;
	UINT8 val_l = 0;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);

	val_l = val & 0x00ff;
	val_h = (val & 0xff00) >> 8;
//	printk("val_h = 0x%x   val_l = 0x%x\n",val_h,val_l);

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH1_THR_HOLD_H,val_h);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH1_THR_HOLD_H);
		return ret;
	}

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH1_THR_HOLD_L,val_l);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH1_THR_HOLD_L);
		return ret;
	}

//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute th1_attribute =
         __ATTR(th1, 0664, th1_attr_show, th1_attr_store);

static ssize_t th2_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	int val = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
		
	ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
	if(ret < 0)
	{
		printk("th2_attr_show:read error\n");
		val = -1;
	}
	else
	{
		val = buffer[SAR_AD715X_REG_CH2_THR_HOLD_H]*256 + buffer[SAR_AD715X_REG_CH2_THR_HOLD_L];
//		printk("th2_attr_show:read val 0x%x\n",val);
	}	
	return sprintf(buf, "%d\n", val);
}
static ssize_t th2_attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	INT32 ret = 0;
	INT16 val;
	UINT8 val_h = 0;
	UINT8 val_l = 0;

    if(NULL == buf || count > 20 || count == 0)
        return -1;

	val = atoi(buf);

	val_l = val & 0x00ff;
	val_h = (val & 0xff00) >> 8;
//	printk("val_h = 0x%x   val_l = 0x%x\n",val_h,val_l);

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH2_THR_HOLD_H,val_h);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH2_THR_HOLD_H);
		return ret;
	}

	ret = sar_ad715x_i2c_write(g_client,SAR_AD715X_REG_CH2_THR_HOLD_L,val_l);
	if(ret < 0)
	{
		printk("write register %d failed\n",SAR_AD715X_REG_CH2_THR_HOLD_L);
		return ret;
	}
	
//	printk("%d\n",count);
	return count;
}
static struct kobj_attribute th2_attribute =
         __ATTR(th2, 0664, th2_attr_show, th2_attr_store);

static ssize_t exist_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
	return sprintf(buf, "%d\n", ad7156_exist);
}

static struct kobj_attribute exist_attribute =
         __ATTR(exist, 0664, exist_attr_show, NULL);
static struct attribute* ad7156_attributes[] =
 {
	 &state_attribute.attr,
	 &gpio_nv_attribute.attr,
	 &ch1_attribute.attr,
	 &ch2_attribute.attr,
	 &setup1_attribute.attr,
	 &setup2_attribute.attr,
	 &config_attribute.attr,
	 &PDT_attribute.attr,
	 &CAP1_attribute.attr,
	 &CAP2_attribute.attr,
	 &th1_attribute.attr,
	 &th2_attribute.attr,
	 &exist_attribute.attr,
     NULL
 };
 static struct attribute_group ad7156_defattr_group =
 {
     .attrs = ad7156_attributes,
 };

static int ad7156_probe(struct i2c_client *client, 
    							const struct i2c_device_id *id)
{
    struct kobject *kobj = NULL;
	UINT8 i = 0;
	UINT8 error_counter = 0;
	INT32 ret = 0;
	UINT8 buffer[SAR_AD715X_REG_NUM];
	memset(buffer,0,SAR_AD715X_REG_NUM);
	printk(KERN_ERR "ad7156 device %s at $%02X...\n", client->name, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		return -1;
	}

    kobj = kobject_create_and_add("ad7156", NULL);
  	if (kobj == NULL) {	
		printk(KERN_ERR "kobject_create_and_add error\n" );
		return -1;
	}
  	if (sysfs_create_group(kobj, &ad7156_defattr_group)) {
		kobject_put(kobj);
		printk(KERN_ERR "sysfs_create_group error\n" );
		return -1;
	}
  
    g_client = client;  

	while(i < 10)
	{
		i++;
		ret = sar_ad715x_i2c_read(g_client,0,buffer,SAR_AD715X_REG_NUM);
		if(ret < 0)
			error_counter++;
	}
	if(error_counter > 9)
		ad7156_exist = 0;
	return 0;
}

static int ad7156_remove(struct i2c_client *client)
{
	return 0;
}
static struct i2c_device_id ad7156_id[]={
	{"ad7156",0},
	{},	
};

static struct i2c_driver ad7156_driver = {
	.probe		= ad7156_probe,
	.remove		= ad7156_remove,
	.id_table	= ad7156_id,
	.driver = {
		.name	= "ad7156",
	},
};



/************************************************************************ 
  名称     : sar_init
  描述     : 
  参数名          类型            输入/输出            描述

  返回值   :   : 成功
                          : 失败  
************************************************************************/

static INT32 __init sar_init(void)
{
	return i2c_add_driver(&ad7156_driver);
}
/************************************************************************ 
  名称     : sar_exit
  描述     : 
  参数名          类型            输入/输出            描述

  返回值   :   : 成功
                          : 失败  
************************************************************************/
static void __exit sar_exit(void)
{
	i2c_del_driver(&ad7156_driver);
}

module_init(sar_init);
module_exit(sar_exit);

/************************* 文件结束**********************************/

