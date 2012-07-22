/***********************************************************************
  版权信息: 版权所有(C) 1988-2010, 华为技术有限公司.
  文件名: 
  作者: 
  版本: 
  创建日期: 2010-02-10
  完成日期: 
  功能描述: 
      定义GPS模块用到的公共的数据类型； 引用的公共头文件等
  主要函数列表: 
     
  修改历史记录列表: 
    <作  者>    <修改时间>  <版本>  <修改描述>
  备注: 
===========================================================================*/
#ifndef _HW_SAR_H

/*************************头文件引用*******************************/
/*************************宏定义*************************************/
#define _HW_SAR_H

/* 返回值: 成功 */
#define SAR_OK  0
/* 返回值: 失败 */
#define SAR_ERROR -1


/* 打印级别 */
enum 
{
    enSarPrintLevelErr = 0,
    enSarPrintLevelInfo,
    enSarPrintLevelDebug 
};

/*************************结构体定义*******************************/
typedef char INT8;
typedef short INT16;
typedef int INT32;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef char * STRING;
typedef long LONG;
typedef unsigned long ULONG;

/* AD715X 寄存器starts */
#define SAR_AD715X_REG_STATUS              0
#define SAR_AD715X_REG_CH1_DATA_HIGH       1
#define SAR_AD715X_REG_CH1_DATA_LOW        2
#define SAR_AD715X_REG_CH2_DATA_HIGH       3
#define SAR_AD715X_REG_CH2_DATA_LOW        4
#define SAR_AD715X_REG_CH1_AVG_HIGH        5
#define SAR_AD715X_REG_CH1_AVG_LOW         6
#define SAR_AD715X_REG_CH2_AVG_HIGH        7
#define SAR_AD715X_REG_CH2_AVG_LOW         8
#define SAR_AD715X_REG_CH1_SENSITIVITY     9
#define SAR_AD715X_REG_CH1_THR_HOLD_H      9
#define SAR_AD715X_REG_CH1_TIMEOUT         10
#define SAR_AD715X_REG_CH1_THR_HOLD_L      10
#define SAR_AD715X_REG_CH1_SETUP           11
#define SAR_AD715X_REG_CH2_SENSITIVITY     12
#define SAR_AD715X_REG_CH2_THR_HOLD_H      12
#define SAR_AD715X_REG_CH2_TIMEOUT         13
#define SAR_AD715X_REG_CH2_THR_HOLD_L      13
#define SAR_AD715X_REG_CH2_SETUP           14
#define SAR_AD715X_REG_CFG                 15
#define SAR_AD715X_REG_PD_TIMER            16
#define SAR_AD715X_REG_CH1_CAPDAC          17
#define SAR_AD715X_REG_CH2_CAPDAC          18
#define SAR_AD715X_REG_SN3                 19
#define SAR_AD715X_REG_SN2                 20
#define SAR_AD715X_REG_SN1                 21
#define SAR_AD715X_REG_SN0                 22
#define SAR_AD715X_REG_ID                  23
/* AD715X 寄存器ends */
/* ad7156 寄存器数量 */
#define SAR_AD715X_REG_NUM 24
/*************************外部函数声明****************************/
extern INT32 sar_printk(UINT8 ucSarPrintLevel, STRING szFmt, ...);
/*************************外部全局变量****************************/

#endif /* _HW_SAR_H */
/*************************文件结束**********************************/
