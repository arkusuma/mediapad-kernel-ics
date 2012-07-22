#ifndef _ST303_H_
#define _ST303_H_

/* <BU5D07680 liujinggang 20100413 begin */
#define     GPIO_INT1                         63
#define     GPIO_INT2                         147

#define ST303_TIMRER (1000*1000000)           /*1000000s*/ 

typedef enum
{
	GS_ADIX345 	= 0x01,
	GS_ST35DE	= 0x02,
	GS_ST303DLH = 0X03
}hw_gs_type;

/* BU5D07680 liujinggang 20100413 end> */
#endif

