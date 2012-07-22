/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
*
* File Name          : l3g4200d.c
* Authors            : MH - C&I BU - Application Team
*		     : Carmine Iascone (carmine.iascone@st.com)
*		     : Matteo Dameno (matteo.dameno@st.com)
* Version            : V 0.2
* Date               : 09/04/2010
* Description        : L3G4200D digital output gyroscope sensor API
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
*
********************************************************************************
* REVISON HISTORY
*
* VERSION | DATE 	| AUTHORS	     | DESCRIPTION
*
* 0.1	  | 29/01/2010	| Carmine Iascone    | First Release
* 
* 0.2	  | 09/04/2010  | Carmine Iascone    | Updated the struct l3g4200d_t
*
*******************************************************************************/

#ifndef __L3G4200D_H__
#define __L3G4200D_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

/** This define controls compilation of the master device interface */
/*#define L3G4200D_MASTER_DEVICE*/

#define L3G4200D_IOCTL_BASE 'g'
/* The following define the IOCTL command values via the ioctl macros */
#define L3G4200D_SET_RANGE		_IOW(L3G4200D_IOCTL_BASE, 1, int)
#define L3G4200D_SET_MODE		_IOW(L3G4200D_IOCTL_BASE, 2, int)
#define L3G4200D_SET_BANDWIDTH		_IOW(L3G4200D_IOCTL_BASE, 3, int)
#define L3G4200D_READ_GYRO_VALUES	_IOW(L3G4200D_IOCTL_BASE, 4, int)

#define ECS_IOCTL_APP_GET_GFLAG  _IOR(L3G4200D_IOCTL_BASE, 5, short)
#define ECS_IOCTL_APP_SET_GDELAY  _IOW(L3G4200D_IOCTL_BASE, 6, int)
#define ECS_IOCTL_APP_SET_GFLAG  _IOW(L3G4200D_IOCTL_BASE, 7, short)
#define ECS_IOCTL_APP_GET_GDELAY  _IOR(L3G4200D_IOCTL_BASE, 8, int)

#define L3G4200D_FS_250DPS	0x00
#define L3G4200D_FS_500DPS	0x10
#define L3G4200D_FS_2000DPS	0x30

#define PM_OFF		0x00
#define PM_NORMAL	0x08
#define ENABLE_ALL_AXES	0x07

#define ODR100_BW12_5	0x00  /* ODR = 100Hz; BW = 12.5Hz */
#define ODR100_BW25	0x10  /* ODR = 100Hz; BW = 25Hz   */
#define ODR200_BW12_5	0x40  /* ODR = 200Hz; BW = 12.5Hz */
#define ODR200_BW25	0x50  /* ODR = 200Hz; BW = 25Hz   */
#define ODR200_BW50	0x60  /* ODR = 200Hz; BW = 50Hz   */
#define ODR400_BW25	0x90  /* ODR = 400Hz; BW = 25Hz   */
#define ODR400_BW50	0xA0  /* ODR = 400Hz; BW = 50Hz   */
#define ODR400_BW110	0xB0  /* ODR = 400Hz; BW = 110Hz  */
#define ODR800_BW50	0xE0  /* ODR = 800Hz; BW = 50Hz   */
#define ODR800_BW100	0xF0  /* ODR = 800Hz; BW = 100Hz  */

/* l3g4200d gyroscope registers */
#define WHO_AM_I    	0x0F

#define CTRL_REG1       0x20    
#define CTRL_REG2       0x21    
#define CTRL_REG3       0x22   
#define CTRL_REG4       0x23   
#define CTRL_REG5       0x24   
#define AXISDATA_REG    0x28
#define STATUS_REG		0x27	
#define INT1_CFG		0x30
#define INT1_SRC		0x31
#define INT1_THS_XH		0x32
#define INT1_THS_XL		0x33
#define INT1_THS_YH		0x34
#define INT1_THS_YL		0x35
#define INT1_THS_ZH		0x36
#define INT1_THS_ZL		0x37
#define INT1_DURATION	0x38

#define L3G4200D_ID		(0xD3)

#ifdef __KERNEL__
struct l3g4200d_platform_data {
	u8 fs_range;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);

};

#endif /* __KERNEL__ */

#endif  /* __L3G4200D_H__ */
