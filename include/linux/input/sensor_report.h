#ifndef __SENSOR_REPORT_H_
#define __SENSOR_REPORT_H_

#include <linux/ioctl.h>  /* For IOCTL macros */
#include <linux/slab.h>

typedef struct
{
	signed short ax;  
	signed short ay;  
	signed short az;  
	signed short mx;  
	signed short my;  
	signed short mz;  
	signed short gx;
	signed short gy;
	signed short gz;
	signed long time; 
} sensor_dev_data;

typedef struct
{
	long  mag_x;		//SENSOR_TYPE_MAGNETIC_FIELD
	long  mag_y;  
	long  mag_z;  
	long  ori_x;		//SENSOR_TYPE_ORIENTATION
	long  ori_y;  
	long  ori_z;  
	long  gravity_x;	//SENSOR_TYPE_GRAVITY
	long  gravity_y;
	long  gravity_z;
	long  linear_x;	//SENSOR_TYPE_LINEAR_ACCELERATION
	long  linear_y;
	long  linear_z;
	long  rotation_x;//SENSOR_TYPE_ROTATION_VECTOR
	long  rotation_y;
	long  rotation_z;
    int   status; 
	long  time; 
} sensor_cal_data;

#define SENSOR_IOCTL 's'
#define SENSOR_IOCTL_SEND_DATA _IOW(SENSOR_IOCTL, 0x01, sensor_cal_data)

#endif
