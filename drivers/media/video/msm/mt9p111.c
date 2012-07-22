/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "mt9p111.h"

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define Q8    0x00000100

#define REG_MT9P111_MODEL_ID                       0x0000

#define MT9P111_MODEL_ID                       0x2880
//#define MT9P111_OTP_LSC    
#define MT9P111_TARGET_PLL_TIMING_96M 0
#define MT9P111_TARGET_PLL_TIMING_76M 1
#define MT9P111_STATUS_WAIT_TIMES_LONG 40
#define MT9P111_STATUS_WAIT_TIMES_SHORT 3
#define MT9P111_STATUS_WAIT_TIMES_MIDDLE 10
static bool CSI_CONFIG_MT9P111 = 0;
static bool Sensor_CONFIG_MT9P111 = 0;
#ifdef CONFIG_MT9P111_OTP_LSC
static bool Sensor_OTPReaLensshading_MT9P111 = 0;
#endif
//enter or exit software standby mode the time of timeout 
#define STANDBY_TIMEOUT 600
/*==============================================================================
**
** BACK_CAMERA_SENSOR_DETECT
**
** Value: BACK_CAMERA_SENSOR_DETECT must be 0x000 or 0x0001.
** Method:
** During the system register the camera sensor driver,if the module id is right,set it to be 0x0001,
** or else set it to be 0x0000.
** When adding a new sensor driver,you may add the sensor's prob function here.
**
** Alarm:
** When modify the value,you must be careful about the follow function.
** mt9p111_liteon_sensor_probe();
** mt9p111_sensor_probe();
** ... 
** 
** Author:Hushaowu/00183970
** Data:2011/07/16
** Version:1.0
**
** when              who                             what                               why
** ---------      -----------------        -------------------       -----------------------------
** 2011/07/16    hushaowu/00183970       Add the global variable       Enter standby with state retention        
** 
=================================================================================*/
//it's initialization in mt9p111_liteon.c.
//extern unsigned short BACK_CAMERA_SENSOR_DETECT;
unsigned short BACK_CAMERA_SENSOR_DETECT = 0x0000;
EXPORT_SYMBOL(BACK_CAMERA_SENSOR_DETECT);

/*========================================================================
**
** MT9P111_INIT_FLAG
**
** After the system register the camera sensor driver,it will write all the register.if it's failed,
** set the globle variable to 0x0000;
**
** It's value will be used in function mt9p111_sensor_setting();
**
==========================================================================*/
static unsigned short MT9P111_INIT_FLAG = 0x0000;

/*=========================================================================
**
** MT9P111_ENTER_STANDBY_FLAG
**
** After the system initialization the senor,it will enter the standby mode.if it's failed,
** set the globle variable to 0x0000;
**
** It's value will be used in function mt9p111_sensor_setting();
** 
===========================================================================*/
static unsigned short MT9P111_ENTER_STANDBY_FLAG = 0x0000;

/*=========================================================================
**
** MT9P111_EXIT_STANDBY_FLAG
**
** After the system initialization the senor,it will enter the standby mode.if it's failed,
** set the globle variable to 0x0000;
**
** It's value will be used in function mt9p111_liteon_sensor_setting();
** 
===========================================================================*/
//First time,it will not  run exit standby
static unsigned short MT9P111_EXIT_STANDBY_FLAG = 0x0001;
/*============================================================================
							DATA DECLARATIONS
============================================================================*/
/*  720P @ 24MHz MCLK */
#ifndef CONFIG_MT9P111_OTP_LSC
struct reg_addr_val_pair_struct mt9p111_init_settings_array[] = {
// [PLL_settings]
{0x0010, 0x0338 , 2},// PLL_DIVIDERS
{0x0012, 0x0070 , 2},	// PLL_P_DIVIDERS
{0x0014, 0x2025 , 2},	// PLL_CONTROL
{0x001E, 0x0665 , 2},	// PAD_SLEW_PAD_CONFIG
{0x0022, 0x0030	, 2},//1E0 // VDD_DIS_COUNTER
{0x002A, 0x7F7C , 2},	// PLL_P4_P5_P6_DIVIDERS
{0x002C, 0x0000	, 2},//, WORD_LEN, 0},
{0x002E, 0x0000	, 2},//, WORD_LEN, 0},
{0x0018, 0x4008	, 2},//, WORD_LEN, 0}, // STANDBY_CONTROL_AND_STATUS
//POLL_{0x0018,0xE07F,!=0x2008,DELAY=10,TIMEOUT=100	//Wait for the core ready
{0     , 100    , 0},
{0x0010, 0x0338 , 2},	// PLL_DIVIDERS

//timing_settings_
{0x098E, 0x483A , 2},	// LOGICAL_ADDRESS_ACCESS
{0xC83A, 0x000C , 2},	// CAM_CORE_A_Y_ADDR_START
{0xC83C, 0x0018 , 2},	// CAM_CORE_A_X_ADDR_START
{0xC83E, 0x07B1 , 2},	// CAM_CORE_A_Y_ADDR_END
{0xC840, 0x0A45 , 2},	// CAM_CORE_A_X_ADDR_END
{0xC842, 0x0001 , 2},	// CAM_CORE_A_ROW_SPEED
{0xC844, 0x0103 , 2},	// CAM_CORE_A_SKIP_X_CORE
{0xC846, 0x0103 , 2},	// CAM_CORE_A_SKIP_Y_CORE
{0xC848, 0x0103 , 2},	// CAM_CORE_A_SKIP_X_PIPE
{0xC84A, 0x0103 , 2},	// CAM_CORE_A_SKIP_Y_PIPE
{0xC84C, 0x0096 , 2},	// CAM_CORE_A_POWER_MODE [101124 利侩] 0x00F6-> 0x0096
{0xC84E, 0x0001 , 2},	// CAM_CORE_A_BIN_MODE
{0xC850, 0x03 ,    1},// CAM_CORE_A_ORIENTATION
{0xC851, 0x00 ,    1},// CAM_CORE_A_PIXEL_ORDER
{0xC852, 0x019C , 2},	// CAM_CORE_A_FINE_CORRECTION
{0xC854, 0x0732 , 2},	// CAM_CORE_A_FINE_ITMIN
{0xC858, 0x0000 , 2},	// CAM_CORE_A_COARSE_ITMIN
{0xC85A, 0x0001 , 2},	// CAM_CORE_A_COARSE_ITMAX_MARGIN
{0xC85C, 0x0423 , 2},	// CAM_CORE_A_MIN_FRAME_LENGTH_LINES
{0xC85E, 0xFFFF , 2},	// CAM_CORE_A_MAX_FRAME_LENGTH_LINES
{0xC860, 0x0423 , 2},	// CAM_CORE_A_BASE_FRAME_LENGTH_LINES
{0xC862, 0x0F50 , 2},	// CAM_CORE_A_MIN_LINE_LENGTH_PCLK
{0xC864, 0x0F50 , 2},	// CAM_CORE_A_MAX_LINE_LENGTH_PCLK
{0xC866, 0x7F7C , 2},	// CAM_CORE_A_P4_5_6_DIVIDER
{0xC868, 0x0423 , 2},	// CAM_CORE_A_FRAME_LENGTH_LINES
{0xC86A, 0x0F50 , 2},	// CAM_CORE_A_LINE_LENGTH_PCK
{0xC86C, 0x0518 , 2},	// CAM_CORE_A_OUTPUT_SIZE_WIDTH
{0xC86E, 0x03D4 , 2},	// CAM_CORE_A_OUTPUT_SIZE_HEIGHT
{0xC870, 0x0014 , 2},	// CAM_CORE_A_RX_FIFO_TRIGGER_MARK
{0xC858, 0x0002 , 2},	// CAM_CORE_A_COARSE_ITMIN
{0xC8B8, 0x0004 , 2},	// CAM_OUTPUT_0_JPEG_CONTROL
{0xC8AE, 0x0001 , 2},	// CAM_OUTPUT_0_OUTPUT_FORMAT
{0xC8AA, 0x0500	, 2},//500 // CAM_OUTPUT_0_IMAGE_WIDTH
{0xC8AC, 0x03C0	, 2},//3C0 // CAM_OUTPUT_0_IMAGE_HEIGHT
//[Full_Res_Settings_JPEG_Fullspeed]
//;Max Framerate in Full Res
{0x098E, 0x4872 , 2},	// LOGICAL_ADDRESS_ACCESS
{0xC872, 0x0010 , 2},	// CAM_CORE_B_Y_ADDR_START
{0xC874, 0x001C , 2},	// CAM_CORE_B_X_ADDR_START
{0xC876, 0x07AF , 2},	// CAM_CORE_B_Y_ADDR_END
{0xC878, 0x0A43 , 2},	// CAM_CORE_B_X_ADDR_END
{0xC87A, 0x0001 , 2},	// CAM_CORE_B_ROW_SPEED
{0xC87C, 0x0101 , 2},	// CAM_CORE_B_SKIP_X_CORE
{0xC87E, 0x0101 , 2},	// CAM_CORE_B_SKIP_Y_CORE
{0xC880, 0x0101 , 2},	// CAM_CORE_B_SKIP_X_PIPE
{0xC882, 0x0101 , 2},	// CAM_CORE_B_SKIP_Y_PIPE
{0xC884, 0x005C , 2},	// CAM_CORE_B_POWER_MODE [101124 利侩] 0x00F2-> 0x005C
{0xC886, 0x0000 , 2},	// CAM_CORE_B_BIN_MODE
{0xC888, 0x03,     1},// CAM_CORE_B_ORIENTATION
{0xC889, 0x00,     1},// CAM_CORE_B_PIXEL_ORDER
{0xC88A, 0x009C , 2},	// CAM_CORE_B_FINE_CORRECTION
{0xC88C, 0x034A , 2},	// CAM_CORE_B_FINE_ITMIN
{0xC890, 0x0000 , 2},	// CAM_CORE_B_COARSE_ITMIN
{0xC892, 0x0001 , 2},	// CAM_CORE_B_COARSE_ITMAX_MARGIN
{0xC894, 0x07EF , 2},	// CAM_CORE_B_MIN_FRAME_LENGTH_LINES
{0xC896, 0xFFFF , 2},	// CAM_CORE_B_MAX_FRAME_LENGTH_LINES
{0xC898, 0x082F , 2},	// CAM_CORE_B_BASE_FRAME_LENGTH_LINES
{0xC89A, 0x1964 , 2},	// CAM_CORE_B_MIN_LINE_LENGTH_PCLK
{0xC89C, 0xFFFE , 2},	// CAM_CORE_B_MAX_LINE_LENGTH_PCLK
{0xC89E, 0x7F7C , 2},	// CAM_CORE_B_P4_5_6_DIVIDER
{0xC8A0, 0x07EF , 2},	// CAM_CORE_B_FRAME_LENGTH_LINES
{0xC8A2, 0x1F40 , 2},	// CAM_CORE_B_LINE_LENGTH_PCK
{0xC8A4, 0x0A28 , 2},	// CAM_CORE_B_OUTPUT_SIZE_WIDTH
{0xC8A6, 0x07A0 , 2},	// CAM_CORE_B_OUTPUT_SIZE_HEIGHT
{0xC8A8, 0x0124 , 2},	// CAM_CORE_B_RX_FIFO_TRIGGER_MARK
{0xC890, 0x0002 , 2},	// CAM_CORE_B_COARSE_ITMIN
{0xC8C0, 0x0A20 , 2},	// CAM_OUTPUT_1_IMAGE_WIDTH
{0xC89A, 0x1F40 , 2},	// CAM_CORE_B_MIN_LINE_LENGTH_PCLK
{0xC8A2, 0x1F40 , 2},	// CAM_CORE_B_LINE_LENGTH_PCK
{0xC8C4, 0x0001 , 2},	// CAM_OUTPUT_1_OUTPUT_FORMAT
{0xC8C6, 0x0000 , 2},	// CAM_OUTPUT_1_OUTPUT_FORMAT_ORDER
{0xC8CE, 0x0014 , 2},	// CAM_OUTPUT_1_JPEG_CONTROL
{0xD822, 0x4610 , 2},	// JPEG_JPSS_CTRL_VAR
{0x3330, 0x0000 , 2},	// OUTPUT_FORMAT_TEST
// [Flicker new Flicker setting for new PLL - 4/29/11]
{0x098E, 0x2018 , 2},	// LOGICAL_ADDRESS_ACCESS [FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_A]
{0xA018, 0x0107 , 2},	// FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_A
{0xA01A, 0x00B6 , 2},	// FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_B
{0xA01C, 0x00DA , 2},	// FD_EXPECTED60HZ_FLICKER_PERIOD_IN_CONTEXT_A
{0xA01E, 0x0097 , 2},	// FD_EXPECTED60HZ_FLICKER_PERIOD_IN_CONTEXT_B
{0xA010, 0x00F3 , 2},	// FD_MIN_EXPECTED50HZ_FLICKER_PERIOD
{0xA012, 0x0111 , 2},	// FD_MAX_EXPECTED50HZ_FLICKER_PERIOD
{0xA014, 0x00C6 , 2},	// FD_MIN_EXPECTED60HZ_FLICKER_PERIOD
{0xA016, 0x00E4,  2},	// FD_MAX_EXPECTED60HZ_FLICKER_PERIOD

//LOAD = Step3-Recommended		//Patch & Char settings
//; NEW Patch_AF
//  k28a_rev03_patch14_CR31491_Pantech_CAF_OSD_REV2  //jolland_0105_2011
{0x0982, 0x0000 , 2},
{0x098A, 0x0000 , 2},
{0x886C, 0xC0F1 , 2},
{0x886E, 0xC5E1 , 2},
{0x8870, 0x246A , 2},
{0x8872, 0x1280 , 2},
{0x8874, 0xC4E1 , 2},
{0x8876, 0xD20F , 2},
{0x8878, 0x2069 , 2},
{0x887A, 0x0000 , 2},
{0x887C, 0x6A62 , 2},
{0x887E, 0x1303 , 2},
{0x8880, 0x0084 , 2},
{0x8882, 0x1734 , 2},
{0x8884, 0x7005 , 2},
{0x8886, 0xD801 , 2},
{0x8888, 0x8A41 , 2},
{0x888A, 0xD900 , 2},
{0x888C, 0x0D5A , 2},
{0x888E, 0x0664 , 2},
{0x8890, 0x8B61 , 2},
{0x8892, 0xE80B , 2},
{0x8894, 0x000D , 2},
{0x8896, 0x0020 , 2},
{0x8898, 0xD508 , 2},
{0x889A, 0x1504 , 2},
{0x889C, 0x1400 , 2},
{0x889E, 0x7840 , 2},
{0x88A0, 0xD007 , 2},
{0x88A2, 0x0DFB , 2},
{0x88A4, 0x9004 , 2},
{0x88A6, 0xC4C1 , 2},
{0x88A8, 0x2029 , 2},
{0x88AA, 0x0300 , 2},
{0x88AC, 0x0219 , 2},
{0x88AE, 0x06C4 , 2},
{0x88B0, 0xFF80 , 2},
{0x88B2, 0x08CC , 2},
{0x88B4, 0xFF80 , 2},
{0x88B6, 0x086C , 2},
{0x88B8, 0xFF80 , 2},
{0x88BA, 0x08C0 , 2},
{0x88BC, 0xFF80 , 2},
{0x88BE, 0x08CC , 2},
{0x88C0, 0xFF80 , 2},
{0x88C2, 0x0CA8 , 2},
{0x88C4, 0xFF80 , 2},
{0x88C6, 0x0D80 , 2},
{0x88C8, 0xFF80 , 2},
{0x88CA, 0x0DA0 , 2},
{0x88CC, 0x000E , 2},
{0x88CE, 0x0002 , 2},
{0x88D0, 0x0000 , 2},
{0x88D2, 0x0000 , 2},
{0x88D4, 0xD2FC , 2},
{0x88D6, 0xD0FD , 2},
{0x88D8, 0x122A , 2},
{0x88DA, 0x0901 , 2},
{0x88DC, 0x900B , 2},
{0x88DE, 0x792F , 2},
{0x88E0, 0xB808 , 2},
{0x88E2, 0x2004 , 2},
{0x88E4, 0x0F80 , 2},
{0x88E6, 0x0000 , 2},
{0x88E8, 0xFF00 , 2},
{0x88EA, 0x7825 , 2},
{0x88EC, 0x1A2A , 2},
{0x88EE, 0x0024 , 2},
{0x88F0, 0x0729 , 2},
{0x88F2, 0x0504 , 2},
{0x88F4, 0xC0F1 , 2},
{0x88F6, 0x095E , 2},
{0x88F8, 0x06C4 , 2},
{0x88FA, 0xD6F5 , 2},
{0x88FC, 0x8E01 , 2},
{0x88FE, 0xB8A4 , 2},
{0x8900, 0xAE01 , 2},
{0x8902, 0x8E09 , 2},
{0x8904, 0xB8E0 , 2},
{0x8906, 0xF29B , 2},
{0x8908, 0xD5F2 , 2},
{0x890A, 0x153A , 2},
{0x890C, 0x1080 , 2},
{0x890E, 0x153B , 2},
{0x8910, 0x1081 , 2},
{0x8912, 0xB808 , 2},
{0x8914, 0x7825 , 2},
{0x8916, 0x16B8 , 2},
{0x8918, 0x1101 , 2},
{0x891A, 0x092D , 2},
{0x891C, 0x0003 , 2},
{0x891E, 0x16B0 , 2},
{0x8920, 0x1082 , 2},
{0x8922, 0x1E3C , 2},
{0x8924, 0x1082 , 2},
{0x8926, 0x16B1 , 2},
{0x8928, 0x1082 , 2},
{0x892A, 0x1E3D , 2},
{0x892C, 0x1082 , 2},
{0x892E, 0x16B4 , 2},
{0x8930, 0x1082 , 2},
{0x8932, 0x1E3E , 2},
{0x8934, 0x1082 , 2},
{0x8936, 0x16B5 , 2},
{0x8938, 0x1082 , 2},
{0x893A, 0x1E3F , 2},
{0x893C, 0x1082 , 2},
{0x893E, 0x8E40 , 2},
{0x8940, 0xBAA6 , 2},
{0x8942, 0xAE40 , 2},
{0x8944, 0x098F , 2},
{0x8946, 0x0022 , 2},
{0x8948, 0x16BA , 2},
{0x894A, 0x1102 , 2},
{0x894C, 0x0A87 , 2},
{0x894E, 0x0003 , 2},
{0x8950, 0x16B2 , 2},
{0x8952, 0x1084 , 2},
{0x8954, 0x0A92 , 2},
{0x8956, 0x06A4 , 2},
{0x8958, 0x16B0 , 2},
{0x895A, 0x1083 , 2},
{0x895C, 0x1E3C , 2},
{0x895E, 0x1002 , 2},
{0x8960, 0x153A , 2},
{0x8962, 0x1080 , 2},
{0x8964, 0x153B , 2},
{0x8966, 0x1081 , 2},
{0x8968, 0x16B3 , 2},
{0x896A, 0x1084 , 2},
{0x896C, 0xB808 , 2},
{0x896E, 0x7825 , 2},
{0x8970, 0x16B8 , 2},
{0x8972, 0x1101 , 2},
{0x8974, 0x16BA , 2},
{0x8976, 0x1102 , 2},
{0x8978, 0x0A6E , 2},
{0x897A, 0x06A4 , 2},
{0x897C, 0x16B1 , 2},
{0x897E, 0x1083 , 2},
{0x8980, 0x1E3D , 2},
{0x8982, 0x1002 , 2},
{0x8984, 0x153A , 2},
{0x8986, 0x1080 , 2},
{0x8988, 0x153B , 2},
{0x898A, 0x1081 , 2},
{0x898C, 0x16B6 , 2},
{0x898E, 0x1084 , 2},
{0x8990, 0xB808 , 2},
{0x8992, 0x7825 , 2},
{0x8994, 0x16B8 , 2},
{0x8996, 0x1101 , 2},
{0x8998, 0x16BA , 2},
{0x899A, 0x1102 , 2},
{0x899C, 0x0A4A , 2},
{0x899E, 0x06A4 , 2},
{0x89A0, 0x16B4 , 2},
{0x89A2, 0x1083 , 2},
{0x89A4, 0x1E3E , 2},
{0x89A6, 0x1002 , 2},
{0x89A8, 0x153A , 2},
{0x89AA, 0x1080 , 2},
{0x89AC, 0x153B , 2},
{0x89AE, 0x1081 , 2},
{0x89B0, 0x16B7 , 2},
{0x89B2, 0x1084 , 2},
{0x89B4, 0xB808 , 2},
{0x89B6, 0x7825 , 2},
{0x89B8, 0x16B8 , 2},
{0x89BA, 0x1101 , 2},
{0x89BC, 0x16BA , 2},
{0x89BE, 0x1102 , 2},
{0x89C0, 0x0A26 , 2},
{0x89C2, 0x06A4 , 2},
{0x89C4, 0x16B5 , 2},
{0x89C6, 0x1083 , 2},
{0x89C8, 0x1E3F , 2},
{0x89CA, 0x1002 , 2},
{0x89CC, 0x8E00 , 2},
{0x89CE, 0xB8A6 , 2},
{0x89D0, 0xAE00 , 2},
{0x89D2, 0x153A , 2},
{0x89D4, 0x1081 , 2},
{0x89D6, 0x153B , 2},
{0x89D8, 0x1080 , 2},
{0x89DA, 0xB908 , 2},
{0x89DC, 0x7905 , 2},
{0x89DE, 0x16BA , 2},
{0x89E0, 0x1100 , 2},
{0x89E2, 0x085B , 2},
{0x89E4, 0x0042 , 2},
{0x89E6, 0xD0BC , 2},
{0x89E8, 0x9E31 , 2},
{0x89EA, 0x904D , 2},
{0x89EC, 0x0A2B , 2},
{0x89EE, 0x0063 , 2},
{0x89F0, 0x8E00 , 2},
{0x89F2, 0x16B0 , 2},
{0x89F4, 0x1081 , 2},
{0x89F6, 0x1E3C , 2},
{0x89F8, 0x1042 , 2},
{0x89FA, 0x16B1 , 2},
{0x89FC, 0x1081 , 2},
{0x89FE, 0x1E3D , 2},
{0x8A00, 0x1042 , 2},
{0x8A02, 0x16B4 , 2},
{0x8A04, 0x1081 , 2},
{0x8A06, 0x1E3E , 2},
{0x8A08, 0x1042 , 2},
{0x8A0A, 0x16B5 , 2},
{0x8A0C, 0x1081 , 2},
{0x8A0E, 0x1E3F , 2},
{0x8A10, 0x1042 , 2},
{0x8A12, 0xB886 , 2},
{0x8A14, 0xF012 , 2},
{0x8A16, 0x16B2 , 2},
{0x8A18, 0x1081 , 2},
{0x8A1A, 0xB8A6 , 2},
{0x8A1C, 0x1E3C , 2},
{0x8A1E, 0x1042 , 2},
{0x8A20, 0x16B3 , 2},
{0x8A22, 0x1081 , 2},
{0x8A24, 0x1E3D , 2},
{0x8A26, 0x1042 , 2},
{0x8A28, 0x16B6 , 2},
{0x8A2A, 0x1081 , 2},
{0x8A2C, 0x1E3E , 2},
{0x8A2E, 0x1042 , 2},
{0x8A30, 0x16B7 , 2},
{0x8A32, 0x1081 , 2},
{0x8A34, 0x1E3F , 2},
{0x8A36, 0x1042 , 2},
{0x8A38, 0xAE00 , 2},
{0x8A3A, 0x08F6 , 2},
{0x8A3C, 0x01C4 , 2},
{0x8A3E, 0x0081 , 2},
{0x8A40, 0x06C4 , 2},
{0x8A42, 0x78E0 , 2},
{0x8A44, 0xC0F1 , 2},
{0x8A46, 0x080E , 2},
{0x8A48, 0x06E4 , 2},
{0x8A4A, 0xDB03 , 2},
{0x8A4C, 0xD2A3 , 2},
{0x8A4E, 0x8A2E , 2},
{0x8A50, 0x8ACF , 2},
{0x8A52, 0xB908 , 2},
{0x8A54, 0x79C5 , 2},
{0x8A56, 0xDD65 , 2},
{0x8A58, 0x094F , 2},
{0x8A5A, 0x00D1 , 2},
{0x8A5C, 0xD90A , 2},
{0x8A5E, 0x1A24 , 2},
{0x8A60, 0x0042 , 2},
{0x8A62, 0x8A24 , 2},
{0x8A64, 0xE1E5 , 2},
{0x8A66, 0xF6C9 , 2},
{0x8A68, 0xD902 , 2},
{0x8A6A, 0x2941 , 2},
{0x8A6C, 0x0200 , 2},
{0x8A6E, 0xAA0E , 2},
{0x8A70, 0xAA2F , 2},
{0x8A72, 0x70A9 , 2},
{0x8A74, 0xF014 , 2},
{0x8A76, 0xE1C8 , 2},
{0x8A78, 0x0036 , 2},
{0x8A7A, 0x000B , 2},
{0x8A7C, 0xE0C8 , 2},
{0x8A7E, 0x003A , 2},
{0x8A80, 0x000A , 2},
{0x8A82, 0xD901 , 2},
{0x8A84, 0x2941 , 2},
{0x8A86, 0x0200 , 2},
{0x8A88, 0xAA0E , 2},
{0x8A8A, 0xAA2F , 2},
{0x8A8C, 0xD848 , 2},
{0x8A8E, 0xF008 , 2},
{0x8A90, 0xD900 , 2},
{0x8A92, 0x2941 , 2},
{0x8A94, 0x0200 , 2},
{0x8A96, 0xAA0E , 2},
{0x8A98, 0xAA2F , 2},
{0x8A9A, 0xD820 , 2},
{0x8A9C, 0xD290 , 2},
{0x8A9E, 0x8A26 , 2},
{0x8AA0, 0xB961 , 2},
{0x8AA2, 0xAA26 , 2},
{0x8AA4, 0xF00D , 2},
{0x8AA6, 0x091F , 2},
{0x8AA8, 0x0091 , 2},
{0x8AAA, 0x8A24 , 2},
{0x8AAC, 0xF1E5 , 2},
{0x8AAE, 0x0913 , 2},
{0x8AB0, 0x0812 , 2},
{0x8AB2, 0x08E1 , 2},
{0x8AB4, 0x8812 , 2},
{0x8AB6, 0x2B41 , 2},
{0x8AB8, 0x0201 , 2},
{0x8ABA, 0xAA2E , 2},
{0x8ABC, 0xAA6F , 2},
{0x8ABE, 0x0001 , 2},
{0x8AC0, 0x06C4 , 2},
{0x8AC2, 0x09F7 , 2},
{0x8AC4, 0x8051 , 2},
{0x8AC6, 0x8A24 , 2},
{0x8AC8, 0xF1F3 , 2},
{0x8ACA, 0x78E0 , 2},
{0x8ACC, 0xC0F1 , 2},
{0x8ACE, 0x0F7A , 2},
{0x8AD0, 0x0684 , 2},
{0x8AD2, 0xD682 , 2},
{0x8AD4, 0x7508 , 2},
{0x8AD6, 0x8E01 , 2},
{0x8AD8, 0xD181 , 2},
{0x8ADA, 0x2046 , 2},
{0x8ADC, 0x00C0 , 2},
{0x8ADE, 0xAE01 , 2},
{0x8AE0, 0x1145 , 2},
{0x8AE2, 0x0080 , 2},
{0x8AE4, 0x1146 , 2},
{0x8AE6, 0x0082 , 2},
{0x8AE8, 0xB808 , 2},
{0x8AEA, 0x7845 , 2},
{0x8AEC, 0x0817 , 2},
{0x8AEE, 0x001E , 2},
{0x8AF0, 0x8900 , 2},
{0x8AF2, 0x8941 , 2},
{0x8AF4, 0xB808 , 2},
{0x8AF6, 0x7845 , 2},
{0x8AF8, 0x080B , 2},
{0x8AFA, 0x00DE , 2},
{0x8AFC, 0x70A9 , 2},
{0x8AFE, 0xFFD2 , 2},
{0x8B00, 0x7508 , 2},
{0x8B02, 0x1604 , 2},
{0x8B04, 0x1090 , 2},
{0x8B06, 0x0D93 , 2},
{0x8B08, 0x1400 , 2},
{0x8B0A, 0x8EEA , 2},
{0x8B0C, 0x8E0B , 2},
{0x8B0E, 0x214A , 2},
{0x8B10, 0x2040 , 2},
{0x8B12, 0x8E2D , 2},
{0x8B14, 0xBF08 , 2},
{0x8B16, 0x7F05 , 2},
{0x8B18, 0x8E0C , 2},
{0x8B1A, 0xB808 , 2},
{0x8B1C, 0x7825 , 2},
{0x8B1E, 0x7710 , 2},
{0x8B20, 0x21C2 , 2},
{0x8B22, 0x244C , 2},
{0x8B24, 0x081D , 2},
{0x8B26, 0x03E3 , 2},
{0x8B28, 0xD9FF , 2},
{0x8B2A, 0x2702 , 2},
{0x8B2C, 0x1002 , 2},
{0x8B2E, 0x2A05 , 2},
{0x8B30, 0x037E , 2},
{0x8B32, 0x0FF6 , 2},
{0x8B34, 0x06A4 , 2},
{0x8B36, 0x702F , 2},
{0x8B38, 0x7810 , 2},
{0x8B3A, 0x7F02 , 2},
{0x8B3C, 0x7FF0 , 2},
{0x8B3E, 0xF00B , 2},
{0x8B40, 0x78E2 , 2},
{0x8B42, 0x2805 , 2},
{0x8B44, 0x037E , 2},
{0x8B46, 0x0FE2 , 2},
{0x8B48, 0x06A4 , 2},
{0x8B4A, 0x702F , 2},
{0x8B4C, 0x7810 , 2},
{0x8B4E, 0x671F , 2},
{0x8B50, 0x7FF0 , 2},
{0x8B52, 0x7FEF , 2},
{0x8B54, 0x8E08 , 2},
{0x8B56, 0xBF06 , 2},
{0x8B58, 0xD162 , 2},
{0x8B5A, 0xB8C3 , 2},
{0x8B5C, 0x78E5 , 2},
{0x8B5E, 0xB88F , 2},
{0x8B60, 0x1908 , 2},
{0x8B62, 0x0024 , 2},
{0x8B64, 0x2841 , 2},
{0x8B66, 0x0201 , 2},
{0x8B68, 0x1E26 , 2},
{0x8B6A, 0x1042 , 2},
{0x8B6C, 0x0D15 , 2},
{0x8B6E, 0x1423 , 2},
{0x8B70, 0x1E27 , 2},
{0x8B72, 0x1002 , 2},
{0x8B74, 0x214C , 2},
{0x8B76, 0xA000 , 2},
{0x8B78, 0x214A , 2},
{0x8B7A, 0x2040 , 2},
{0x8B7C, 0x21C2 , 2},
{0x8B7E, 0x2442 , 2},
{0x8B80, 0x8E21 , 2},
{0x8B82, 0x214F , 2},
{0x8B84, 0x0040 , 2},
{0x8B86, 0x090F , 2},
{0x8B88, 0x2010 , 2},
{0x8B8A, 0x2145 , 2},
{0x8B8C, 0x0181 , 2},
{0x8B8E, 0xAE21 , 2},
{0x8B90, 0xF003 , 2},
{0x8B92, 0xB8A2 , 2},
{0x8B94, 0xAE01 , 2},
{0x8B96, 0x0B7A , 2},
{0x8B98, 0xFFE3 , 2},
{0x8B9A, 0x70A9 , 2},
{0x8B9C, 0x0709 , 2},
{0x8B9E, 0x0684 , 2},
{0x8BA0, 0xC0F1 , 2},
{0x8BA2, 0xC5E1 , 2},
{0x8BA4, 0xD54E , 2},
{0x8BA6, 0x8D24 , 2},
{0x8BA8, 0x8D45 , 2},
{0x8BAA, 0xB908 , 2},
{0x8BAC, 0x7945 , 2},
{0x8BAE, 0x0941 , 2},
{0x8BB0, 0x011E , 2},
{0x8BB2, 0x8D26 , 2},
{0x8BB4, 0x0939 , 2},
{0x8BB6, 0x0093 , 2},
{0x8BB8, 0xD148 , 2},
{0x8BBA, 0xA907 , 2},
{0x8BBC, 0xD04A , 2},
{0x8BBE, 0x802E , 2},
{0x8BC0, 0x9117 , 2},
{0x8BC2, 0x0F6E , 2},
{0x8BC4, 0x06A4 , 2},
{0x8BC6, 0x912E , 2},
{0x8BC8, 0x790F , 2},
{0x8BCA, 0x0911 , 2},
{0x8BCC, 0x00B2 , 2},
{0x8BCE, 0x1541 , 2},
{0x8BD0, 0x1080 , 2},
{0x8BD2, 0x0F5E , 2},
{0x8BD4, 0x0684 , 2},
{0x8BD6, 0x780F , 2},
{0x8BD8, 0x2840 , 2},
{0x8BDA, 0x0201 , 2},
{0x8BDC, 0x7825 , 2},
{0x8BDE, 0x2841 , 2},
{0x8BE0, 0x0201 , 2},
{0x8BE2, 0x1D42 , 2},
{0x8BE4, 0x1042 , 2},
{0x8BE6, 0x1D43 , 2},
{0x8BE8, 0x1002 , 2},
{0x8BEA, 0xF003 , 2},
{0x8BEC, 0xFFB8 , 2},
{0x8BEE, 0x06D9 , 2},
{0x8BF0, 0x0684 , 2},
{0x8BF2, 0x78E0 , 2},
{0x8BF4, 0xC0F1 , 2},
{0x8BF6, 0x0E5E , 2},
{0x8BF8, 0x0684 , 2},
{0x8BFA, 0xD538 , 2},
{0x8BFC, 0x8D00 , 2},
{0x8BFE, 0x0841 , 2},
{0x8C00, 0x01DE , 2},
{0x8C02, 0xB8A7 , 2},
{0x8C04, 0x790F , 2},
{0x8C06, 0xD639 , 2},
{0x8C08, 0xAD00 , 2},
{0x8C0A, 0x091F , 2},
{0x8C0C, 0x0050 , 2},
{0x8C0E, 0x0921 , 2},
{0x8C10, 0x0110 , 2},
{0x8C12, 0x0911 , 2},
{0x8C14, 0x0210 , 2},
{0x8C16, 0xD036 , 2},
{0x8C18, 0x0A5A , 2},
{0x8C1A, 0xFFE3 , 2},
{0x8C1C, 0xA600 , 2},
{0x8C1E, 0xF00A , 2},
{0x8C20, 0x000F , 2},
{0x8C22, 0x0020 , 2},
{0x8C24, 0xD033 , 2},
{0x8C26, 0x000B , 2},
{0x8C28, 0x0020 , 2},
{0x8C2A, 0xD033 , 2},
{0x8C2C, 0xD033 , 2},
{0x8C2E, 0xA600 , 2},
{0x8C30, 0x8600 , 2},
{0x8C32, 0x8023 , 2},
{0x8C34, 0x7960 , 2},
{0x8C36, 0xD801 , 2},
{0x8C38, 0xD800 , 2},
{0x8C3A, 0xAD05 , 2},
{0x8C3C, 0x1528 , 2},
{0x8C3E, 0x1080 , 2},
{0x8C40, 0x0817 , 2},
{0x8C42, 0x01DE , 2},
{0x8C44, 0xB8A7 , 2},
{0x8C46, 0x1D28 , 2},
{0x8C48, 0x1002 , 2},
{0x8C4A, 0xD028 , 2},
{0x8C4C, 0x8000 , 2},
{0x8C4E, 0x8023 , 2},
{0x8C50, 0x7960 , 2},
{0x8C52, 0x1528 , 2},
{0x8C54, 0x1080 , 2},
{0x8C56, 0x0669 , 2},
{0x8C58, 0x0684 , 2},
{0x8C5A, 0x78E0 , 2},
{0x8C5C, 0xD21F , 2},
{0x8C5E, 0x8A21 , 2},
{0x8C60, 0xB9A1 , 2},
{0x8C62, 0x782F , 2},
{0x8C64, 0x7FE0 , 2},
{0x8C66, 0xAA21 , 2},
{0x8C68, 0xC0F1 , 2},
{0x8C6A, 0xD125 , 2},
{0x8C6C, 0x8906 , 2},
{0x8C6E, 0x8947 , 2},
{0x8C70, 0xB808 , 2},
{0x8C72, 0x7845 , 2},
{0x8C74, 0x262F , 2},
{0x8C76, 0xF007 , 2},
{0x8C78, 0xF406 , 2},
{0x8C7A, 0xD022 , 2},
{0x8C7C, 0x8001 , 2},
{0x8C7E, 0x7840 , 2},
{0x8C80, 0xC0D1 , 2},
{0x8C82, 0x7EE0 , 2},
{0x8C84, 0xB861 , 2},
{0x8C86, 0x7810 , 2},
{0x8C88, 0x2841 , 2},
{0x8C8A, 0x020C , 2},
{0x8C8C, 0xA986 , 2},
{0x8C8E, 0xA907 , 2},
{0x8C90, 0x780F , 2},
{0x8C92, 0x0815 , 2},
{0x8C94, 0x0051 , 2},
{0x8C96, 0xD015 , 2},
{0x8C98, 0x8000 , 2},
{0x8C9A, 0xD210 , 2},
{0x8C9C, 0x8021 , 2},
{0x8C9E, 0x7960 , 2},
{0x8CA0, 0x8A07 , 2},
{0x8CA2, 0xF1F0 , 2},
{0x8CA4, 0xF1EE , 2},
{0x8CA6, 0x78E0 , 2},
{0x8CA8, 0xC0F1 , 2},
{0x8CAA, 0x0DAA , 2},
{0x8CAC, 0x06A4 , 2},
{0x8CAE, 0xDA44 , 2},
{0x8CB0, 0xD115 , 2},
{0x8CB2, 0xD516 , 2},
{0x8CB4, 0x76A9 , 2},
{0x8CB6, 0x0B3E , 2},
{0x8CB8, 0x06A4 , 2},
{0x8CBA, 0x70C9 , 2},
{0x8CBC, 0xD014 , 2},
{0x8CBE, 0xD900 , 2},
{0x8CC0, 0xF028 , 2},
{0x8CC2, 0x78E0 , 2},
{0x8CC4, 0xFF00 , 2},
{0x8CC6, 0x3354 , 2},
{0x8CC8, 0xFF80 , 2},
{0x8CCA, 0x0694 , 2},
{0x8CCC, 0xFF80 , 2},
{0x8CCE, 0x0314 , 2},
{0x8CD0, 0xFF80 , 2},
{0x8CD2, 0x0250 , 2},
{0x8CD4, 0xFF80 , 2},
{0x8CD6, 0x050C , 2},
{0x8CD8, 0xFF80 , 2},
{0x8CDA, 0x0158 , 2},
{0x8CDC, 0xFF80 , 2},
{0x8CDE, 0x0290 , 2},
{0x8CE0, 0xFF00 , 2},
{0x8CE2, 0x0618 , 2},
{0x8CE4, 0xFF80 , 2},
{0x8CE6, 0x06C8 , 2},
{0x8CE8, 0x8000 , 2},
{0x8CEA, 0x0008 , 2},
{0x8CEC, 0x0000 , 2},
{0x8CEE, 0xF1A4 , 2},
{0x8CF0, 0xFF80 , 2},
{0x8CF2, 0x0EC0 , 2},
{0x8CF4, 0x0000 , 2},
{0x8CF6, 0xF1B4 , 2},
{0x8CF8, 0x0000 , 2},
{0x8CFA, 0xF1C4 , 2},
{0x8CFC, 0xFF80 , 2},
{0x8CFE, 0x02CC , 2},
{0x8D00, 0xFF80 , 2},
{0x8D02, 0x0E48 , 2},
{0x8D04, 0x0000 , 2},
{0x8D06, 0xF9AC , 2},
{0x8D08, 0xFF80 , 2},
{0x8D0A, 0x0E50 , 2},
{0x8D0C, 0xFF80 , 2},
{0x8D0E, 0x08D4 , 2},
{0x8D10, 0xA502 , 2},
{0x8D12, 0xD032 , 2},
{0x8D14, 0xA0C0 , 2},
{0x8D16, 0x17B4 , 2},
{0x8D18, 0xF000 , 2},
{0x8D1A, 0xB02B , 2},
{0x8D1C, 0x17B0 , 2},
{0x8D1E, 0xF001 , 2},
{0x8D20, 0x8900 , 2},
{0x8D22, 0xDB08 , 2},
{0x8D24, 0xDAF0 , 2},
{0x8D26, 0x19B0 , 2},
{0x8D28, 0x00C2 , 2},
{0x8D2A, 0xB8A6 , 2},
{0x8D2C, 0xA900 , 2},
{0x8D2E, 0xD851 , 2},
{0x8D30, 0x19B2 , 2},
{0x8D32, 0x0002 , 2},
{0x8D34, 0xD852 , 2},
{0x8D36, 0x19B3 , 2},
{0x8D38, 0x0002 , 2},
{0x8D3A, 0xD855 , 2},
{0x8D3C, 0x19B6 , 2},
{0x8D3E, 0x0002 , 2},
{0x8D40, 0xD856 , 2},
{0x8D42, 0x19B7 , 2},
{0x8D44, 0x0002 , 2},
{0x8D46, 0xD896 , 2},
{0x8D48, 0x19B8 , 2},
{0x8D4A, 0x0004 , 2},
{0x8D4C, 0xD814 , 2},
{0x8D4E, 0x19BA , 2},
{0x8D50, 0x0004 , 2},
{0x8D52, 0xD805 , 2},
{0x8D54, 0xB111 , 2},
{0x8D56, 0x19B1 , 2},
{0x8D58, 0x0082 , 2},
{0x8D5A, 0x19B4 , 2},
{0x8D5C, 0x00C2 , 2},
{0x8D5E, 0x19B5 , 2},
{0x8D60, 0x0082 , 2},
{0x8D62, 0xD11F , 2},
{0x8D64, 0x2555 , 2},
{0x8D66, 0x1440 , 2},
{0x8D68, 0x0A8A , 2},
{0x8D6A, 0x06A4 , 2},
{0x8D6C, 0xDA2C , 2},
{0x8D6E, 0xD01D , 2},
{0x8D70, 0x2555 , 2},
{0x8D72, 0x1441 , 2},
{0x8D74, 0xA514 , 2},
{0x8D76, 0xD01C , 2},
{0x8D78, 0x0545 , 2},
{0x8D7A, 0x06A4 , 2},
{0x8D7C, 0xA020 , 2},
{0x8D7E, 0x78E0 , 2},
{0x8D80, 0xD01A , 2},
{0x8D82, 0x1788 , 2},
{0x8D84, 0xF001 , 2},
{0x8D86, 0xA11C , 2},
{0x8D88, 0xD019 , 2},
{0x8D8A, 0xA11D , 2},
{0x8D8C, 0xD019 , 2},
{0x8D8E, 0xA11E , 2},
{0x8D90, 0xD019 , 2},
{0x8D92, 0xA11F , 2},
{0x8D94, 0x1754 , 2},
{0x8D96, 0xF000 , 2},
{0x8D98, 0xE170 , 2},
{0x8D9A, 0x7FE0 , 2},
{0x8D9C, 0xA020 , 2},
{0x8D9E, 0x78E0 , 2},
{0x8DA0, 0xC0F1 , 2},
{0x8DA2, 0xC5E1 , 2},
{0x8DA4, 0x1764 , 2},
{0x8DA6, 0xF00D , 2},
{0x8DA8, 0xD114 , 2},
{0x8DAA, 0x2556 , 2},
{0x8DAC, 0x1400 , 2},
{0x8DAE, 0x0A46 , 2},
{0x8DB0, 0x06A4 , 2},
{0x8DB2, 0xDA38 , 2},
{0x8DB4, 0x174C , 2},
{0x8DB6, 0xF000 , 2},
{0x8DB8, 0xD111 , 2},
{0x8DBA, 0xA021 , 2},
{0x8DBC, 0xD011 , 2},
{0x8DBE, 0x2556 , 2},
{0x8DC0, 0x1401 , 2},
{0x8DC2, 0x1D94 , 2},
{0x8DC4, 0x1000 , 2},
{0x8DC6, 0xD010 , 2},
{0x8DC8, 0xA020 , 2},
{0x8DCA, 0x171C , 2},
{0x8DCC, 0xF000 , 2},
{0x8DCE, 0x802E , 2},
{0x8DD0, 0x9117 , 2},
{0x8DD2, 0x04F5 , 2},
{0x8DD4, 0x06A4 , 2},
{0x8DD6, 0xB10E , 2},
{0x8DD8, 0x8000 , 2},
{0x8DDA, 0x016C , 2},
{0x8DDC, 0x0000 , 2},
{0x8DDE, 0xF444 , 2},
{0x8DE0, 0xFF80 , 2},
{0x8DE2, 0x08F4 , 2},
{0x8DE4, 0x8000 , 2},
{0x8DE6, 0x009C , 2},
{0x8DE8, 0xFF80 , 2},
{0x8DEA, 0x0BF4 , 2},
{0x8DEC, 0xFF80 , 2},
{0x8DEE, 0x0BA0 , 2},
{0x8DF0, 0xFF80 , 2},
{0x8DF2, 0x0C5C , 2},
{0x8DF4, 0x0000 , 2},
{0x8DF6, 0x0998 , 2},
{0x8DF8, 0x0000 , 2},
{0x8DFA, 0xF3BC , 2},
{0x8DFC, 0x0000 , 2},
{0x8DFE, 0x2F2C , 2},
{0x8E00, 0xFF80 , 2},
{0x8E02, 0x0C68 , 2},
{0x8E04, 0x8000 , 2},
{0x8E06, 0x008C , 2},
{0x8E08, 0xE280 , 2},
{0x8E0A, 0x24CA , 2},
{0x8E0C, 0x7082 , 2},
{0x8E0E, 0x78E0 , 2},
{0x8E10, 0x20E8 , 2},
{0x8E12, 0x01A2 , 2},
{0x8E14, 0x1002 , 2},
{0x8E16, 0x0D02 , 2},
{0x8E18, 0x1902 , 2},
{0x8E1A, 0x0094 , 2},
{0x8E1C, 0x7FE0 , 2},
{0x8E1E, 0x7028 , 2},
{0x8E20, 0x7308 , 2},
{0x8E22, 0x1000 , 2},
{0x8E24, 0x0900 , 2},
{0x8E26, 0x7904 , 2},
{0x8E28, 0x7947 , 2},
{0x8E2A, 0x1B00 , 2},
{0x8E2C, 0x0064 , 2},
{0x8E2E, 0x7EE0 , 2},
{0x8E30, 0xE280 , 2},
{0x8E32, 0x24CA , 2},
{0x8E34, 0x7082 , 2},
{0x8E36, 0x78E0 , 2},
{0x8E38, 0x20E8 , 2},
{0x8E3A, 0x01A2 , 2},
{0x8E3C, 0x1102 , 2},
{0x8E3E, 0x0502 , 2},
{0x8E40, 0x1802 , 2},
{0x8E42, 0x00B4 , 2},
{0x8E44, 0x7FE0 , 2},
{0x8E46, 0x7028 , 2},
{0x8E48, 0x0000 , 2},
{0x8E4A, 0x0000 , 2},
{0x8E4C, 0x0000 , 2},
{0x8E4E, 0x0000 , 2},
{0x098E, 0x0016 , 2}, // LOGICAL_ADDRESS_ACCESS [MON_ADDRESS_LO]
{0x8016, 0x086C , 2}, // MON_ADDRESS_LO
{0x8002, 0x0001 , 2}, // MON_CMD
//POLL_FIELD=MON_ ,PATCH_0,==0,DELAY=10,TIMEOUT=100     // wait for the patch to complete initialization 
{0     , 100     , 0},



//Char settings
//char_settings		//tuning_reg_settings_array
{0x30D4, 0x9080 , 2}, //COLUMN_CORRECTION
{0x316E, 0xC400 , 2}, //DAC_ECL
{0x305E, 0x10A0 , 2}, //GLOBAL_GAIN
{0x3E00, 0x0010 , 2}, //SAMP_CONTROL
{0x3E02, 0xED02 , 2}, //SAMP_ADDR_EN
{0x3E04, 0xC88C , 2}, //SAMP_RD1_SIG
{0x3E06, 0xC88C , 2}, //SAMP_RD1_SIG_BOOST
{0x3E08, 0x700A , 2}, //SAMP_RD1_RST
{0x3E0A, 0x701E , 2}, //SAMP_RD1_RST_BOOST
{0x3E0C, 0x00FF , 2}, //SAMP_RST1_EN
{0x3E0E, 0x00FF , 2}, //SAMP_RST1_BOOST
{0x3E10, 0x00FF , 2}, //SAMP_RST1_CLOOP_SH
{0x3E12, 0x0000 , 2}, //SAMP_RST_BOOST_SEQ
{0x3E14, 0xC78C , 2}, //SAMP_SAMP1_SIG
{0x3E16, 0x6E06 , 2}, //SAMP_SAMP1_RST
{0x3E18, 0xA58C , 2}, //SAMP_TX_EN
{0x3E1A, 0xA58E , 2}, //SAMP_TX_BOOST
{0x3E1C, 0xA58E , 2}, //SAMP_TX_CLOOP_SH
{0x3E1E, 0xC0D0 , 2}, //SAMP_TX_BOOST_SEQ
{0x3E20, 0xEB00 , 2}, //SAMP_VLN_EN
{0x3E22, 0x00FF , 2}, //SAMP_VLN_HOLD
{0x3E24, 0xEB02 , 2}, //SAMP_VCL_EN
{0x3E26, 0xEA02 , 2}, //SAMP_COLCLAMP
{0x3E28, 0xEB0A , 2}, //SAMP_SH_VCL
{0x3E2A, 0xEC01 , 2}, //SAMP_SH_VREF
{0x3E2C, 0xEB01 , 2}, //SAMP_SH_VBST
{0x3E2E, 0x00FF , 2}, //SAMP_SPARE
{0x3E30, 0x00F3 , 2}, //SAMP_READOUT
{0x3E32, 0x3DFA , 2}, //SAMP_RESET_DONE
{0x3E34, 0x00FF , 2}, //SAMP_VLN_CLAMP
{0x3E36, 0x00F3 , 2}, //SAMP_ASC_INT
{0x3E38, 0x0000 , 2}, //SAMP_RS_CLOOP_SH_R
{0x3E3A, 0xF802 , 2}, //SAMP_RS_CLOOP_SH
{0x3E3C, 0x0FFF , 2}, //SAMP_RS_BOOST_SEQ
{0x3E3E, 0xEA10 , 2}, //SAMP_TXLO_GND
{0x3E40, 0xEB05 , 2}, //SAMP_VLN_PER_COL
{0x3E42, 0xE5C8 , 2}, //SAMP_RD2_SIG
{0x3E44, 0xE5C8 , 2}, //SAMP_RD2_SIG_BOOST
{0x3E46, 0x8C70 , 2}, //SAMP_RD2_RST
{0x3E48, 0x8C71 , 2}, //SAMP_RD2_RST_BOOST
{0x3E4A, 0x00FF , 2}, //SAMP_RST2_EN
{0x3E4C, 0x00FF , 2}, //SAMP_RST2_BOOST
{0x3E4E, 0x00FF , 2}, //SAMP_RST2_CLOOP_SH
{0x3E50, 0xE38D , 2}, //SAMP_SAMP2_SIG
{0x3E52, 0x8B0A , 2}, //SAMP_SAMP2_RST
{0x3E58, 0xEB0A , 2}, //SAMP_PIX_CLAMP_EN
{0x3E5C, 0x0A00 , 2}, //SAMP_PIX_PULLUP_EN
{0x3E5E, 0x00FF , 2}, //SAMP_PIX_PULLDOWN_EN_R
{0x3E60, 0x00FF , 2}, //SAMP_PIX_PULLDOWN_EN_S
{0x3E90, 0x3C01 , 2}, //RST_ADDR_EN
{0x3E92, 0x00FF , 2}, //RST_RST_EN
{0x3E94, 0x00FF , 2}, //RST_RST_BOOST
{0x3E96, 0x3C00 , 2}, //RST_TX_EN
{0x3E98, 0x3C00 , 2}, //RST_TX_BOOST
{0x3E9A, 0x3C00 , 2}, //RST_TX_CLOOP_SH
{0x3E9C, 0xC0E0 , 2}, //RST_TX_BOOST_SEQ
{0x3E9E, 0x00FF , 2}, //RST_RST_CLOOP_SH
{0x3EA0, 0x0000 , 2}, //RST_RST_BOOST_SEQ
{0x3EA6, 0x3C00 , 2}, //RST_PIX_PULLUP_EN
{0x3ED8, 0x3057 , 2}, //DAC_LD_12_13
{0x316C, 0xB44F , 2}, //DAC_TXLO
{0x316E, 0xC6FF , 2}, //DAC_ECL
{0x3ED2, 0xEA0A , 2}, //DAC_LD_6_7
{0x3ED4, 0x00A3 , 2}, //DAC_LD_8_9
{0x3EDC, 0x6020 , 2}, //DAC_LD_16_17
{0x3EE6, 0xA541 , 2}, //DAC_LD_26_27
{0x31E0, 0x0000 , 2}, //PIX_DEF_ID
{0x3ED0, 0x2409 , 2}, //DAC_LD_4_5
{0x3EDE, 0x0A49 , 2}, //DAC_LD_18_19
{0x3EE0, 0x4910,  2},//0x4909 , 2}, //DAC_LD_20_21
{0x3EE2, 0x09D2,  2},//0x09CF , 2}, //DAC_LD_22_23
{0x30B6, 0x0008 , 2}, //AUTOLR_CONTROL
{0x337C, 0x0006 , 2}, //YUV_YCBCR_CONTROL
{0x3E1A, 0xA582 , 2}, //SAMP_TX_BOOST
{0x3E2E, 0xEC05 , 2}, //SAMP_SPARE
{0x3EE6, 0xA5C0 , 2}, //DAC_LD_26_27
{0x316C, 0xB43F , 2},//0xF43F , 2},	//B43F, //DAC_TXLO
{0x316E, 0xC6FF , 2}, //DAC_ECL


{0x060E, 0x00FF , 2},  //VGPIO OFF //20110425 //flash no funciton //wangaicui 

//add af VCM_enable full scan begin
{0x098E, 0xC400 , 2}, // LOGICAL_ADDRESS_ACCESS 
{0xC400, 0x88   , 1}, // AFM_ALGO            
{0x8419, 0x02   , 1}, // SEQ_STATE_CFG_1_AF  
{0xC400, 0x08   , 1}, // AFM_ALGO            
{0xB002, 0x0347 , 2}, // AF_MODE                
{0xB004, 0x0042 , 2}, // AF_ALGO                
{0xC40C, 0x00F0 , 2}, // AFM_POS_MAX           
{0xC40A, 0x0010 , 2}, // AFM_POS_MIN            
{0xB018, 0x20   , 1}, // AF_FS_POS_0         
{0xB019, 0x40   , 1}, // AF_FS_POS_1         
{0xB01A, 0x5E   , 1}, // AF_FS_POS_2          
{0xB01B, 0x7C   , 1}, // AF_FS_POS_3          
{0xB01C, 0x98   , 1}, // AF_FS_POS_4  
{0xB01D, 0xB3   , 1}, // AF_FS_POS_4  
{0xB01E, 0xCD   , 1}, // AF_FS_POS_4 
{0xB01F, 0xE5   , 1}, // AF_FS_POS_4  
{0xB020, 0xFB   , 1}, // AF_FS_POS_4  
{0xB012, 0x09   , 1}, // AF_FS_NUM_STEPS     
{0xB013, 0x77   , 1}, // AF_FS_NUM_STEPS2    
{0xB014, 0x05   , 1}, // AF_FS_STEP_SIZE     
{0x098E, 0x8404 , 2}, // LOGICAL_ADDRESS_ACCESS
{0x8404, 0x05   , 1}, // SEQ_CMD             
//add af VCM_enable full scan end



//LOAD = Step4-PGA			//PGA LSC parameters

//lens shading  begin
//[Lens Correction 85% 04/18/11 14:40:36]
{0x3210, 0x49B0 , 2}, // FIELD_WR= COLOR_PIPELINE_CONTROL, 0x49B0 
{0x3640, 0x7C6F , 2}, // FIELD_WR= P_G1_P0Q0, 0x7C6F 	            
{0x3642, 0x5C0B , 2}, // FIELD_WR= P_G1_P0Q1, 0x5C0B 	            
{0x3644, 0x1CD1 , 2}, // FIELD_WR= P_G1_P0Q2, 0x1CD1 	            
{0x3646, 0x170E , 2}, // FIELD_WR= P_G1_P0Q3, 0x170E 	            
{0x3648, 0xB0B0 , 2}, // FIELD_WR= P_G1_P0Q4, 0xB0B0 	            
{0x364A, 0x00D0 , 2}, // FIELD_WR= P_R_P0Q0, 0x00D0 	            
{0x364C, 0xC5AD , 2}, // FIELD_WR= P_R_P0Q1, 0xC5AD 	            
{0x364E, 0x12B0 , 2}, // FIELD_WR= P_R_P0Q2, 0x12B0 	            
{0x3650, 0x09AF , 2}, // FIELD_WR= P_R_P0Q3, 0x09AF 	            
{0x3652, 0xB08F , 2}, // FIELD_WR= P_R_P0Q4, 0xB08F 	            
{0x3654, 0x7F8F , 2}, // FIELD_WR= P_B_P0Q0, 0x7F8F 	            
{0x3656, 0x4F6C , 2}, // FIELD_WR= P_B_P0Q1, 0x4F6C 	            
{0x3658, 0x28F0 , 2}, // FIELD_WR= P_B_P0Q2, 0x28F0 	            
{0x365A, 0xCDCB , 2}, // FIELD_WR= P_B_P0Q3, 0xCDCB 	            
{0x365C, 0xA1EF , 2}, // FIELD_WR= P_B_P0Q4, 0xA1EF 	            
{0x365E, 0x0290 , 2}, // FIELD_WR= P_G2_P0Q0, 0x0290 	            
{0x3660, 0xC34E , 2}, // FIELD_WR= P_G2_P0Q1, 0xC34E 	            
{0x3662, 0x1A91 , 2}, // FIELD_WR= P_G2_P0Q2, 0x1A91 	            
{0x3664, 0x100F , 2}, // FIELD_WR= P_G2_P0Q3, 0x100F 	            
{0x3666, 0xB3F0 , 2}, // FIELD_WR= P_G2_P0Q4, 0xB3F0 	            
{0x3680, 0x8E6C , 2}, // FIELD_WR= P_G1_P1Q0, 0x8E6C 	            
{0x3682, 0xCD4E , 2}, // FIELD_WR= P_G1_P1Q1, 0xCD4E 	            
{0x3684, 0xEF6D , 2}, // FIELD_WR= P_G1_P1Q2, 0xEF6D 	            
{0x3686, 0x33AC , 2}, // FIELD_WR= P_G1_P1Q3, 0x33AC 	            
{0x3688, 0x0D4F , 2}, // FIELD_WR= P_G1_P1Q4, 0x0D4F 	            
{0x368A, 0x24ED , 2}, // FIELD_WR= P_R_P1Q0, 0x24ED 	            
{0x368C, 0x216D , 2}, // FIELD_WR= P_R_P1Q1, 0x216D 	            
{0x368E, 0xAA0E , 2}, // FIELD_WR= P_R_P1Q2, 0xAA0E 	            
{0x3690, 0xC3EF , 2}, // FIELD_WR= P_R_P1Q3, 0xC3EF 	            
{0x3692, 0x180F , 2}, // FIELD_WR= P_R_P1Q4, 0x180F 	            
{0x3694, 0xB30D , 2}, // FIELD_WR= P_B_P1Q0, 0xB30D 	            
{0x3696, 0x360D , 2}, // FIELD_WR= P_B_P1Q1, 0x360D 	            
{0x3698, 0x250F , 2}, // FIELD_WR= P_B_P1Q2, 0x250F 	            
{0x369A, 0x8DCF , 2}, // FIELD_WR= P_B_P1Q3, 0x8DCF 	            
{0x369C, 0xB60F , 2}, // FIELD_WR= P_B_P1Q4, 0xB60F 	            
{0x369E, 0x20A9 , 2}, // FIELD_WR= P_G2_P1Q0, 0x20A9 	            
{0x36A0, 0xD88E , 2}, // FIELD_WR= P_G2_P1Q1, 0xD88E 	            
{0x36A2, 0x43EB , 2}, // FIELD_WR= P_G2_P1Q2, 0x43EB 	            
{0x36A4, 0x216E , 2}, // FIELD_WR= P_G2_P1Q3, 0x216E 	            
{0x36A6, 0x112E , 2}, // FIELD_WR= P_G2_P1Q4, 0x112E 	            
{0x36C0, 0x3E71 , 2}, // FIELD_WR= P_G1_P2Q0, 0x3E71 	            
{0x36C2, 0x414F , 2}, // FIELD_WR= P_G1_P2Q1, 0x414F 	            
{0x36C4, 0x6A6E , 2}, // FIELD_WR= P_G1_P2Q2, 0x6A6E 	            
{0x36C6, 0x8B8F , 2}, // FIELD_WR= P_G1_P2Q3, 0x8B8F 	            
{0x36C8, 0xB4D2 , 2}, // FIELD_WR= P_G1_P2Q4, 0xB4D2 	            
{0x36CA, 0x0FF1 , 2}, // FIELD_WR= P_R_P2Q0, 0x0FF1 	            
{0x36CC, 0xD3E8 , 2}, // FIELD_WR= P_R_P2Q1, 0xD3E8 	            
{0x36CE, 0xF52E , 2}, // FIELD_WR= P_R_P2Q2, 0xF52E 	            
{0x36D0, 0xBC30 , 2}, // FIELD_WR= P_R_P2Q3, 0xBC30 	            
{0x36D2, 0x4EAF , 2}, // FIELD_WR= P_R_P2Q4, 0x4EAF 	            
{0x36D4, 0x16B1 , 2}, // FIELD_WR= P_B_P2Q0, 0x16B1 	            
{0x36D6, 0x7B6F , 2}, // FIELD_WR= P_B_P2Q1, 0x7B6F 	            
{0x36D8, 0x0DD0 , 2}, // FIELD_WR= P_B_P2Q2, 0x0DD0 	            
{0x36DA, 0xC2EF , 2}, // FIELD_WR= P_B_P2Q3, 0xC2EF 	            
{0x36DC, 0x8612 , 2}, // FIELD_WR= P_B_P2Q4, 0x8612 	            
{0x36DE, 0x5291 , 2}, // FIELD_WR= P_G2_P2Q0, 0x5291 	            
{0x36E0, 0xFC0D , 2}, // FIELD_WR= P_G2_P2Q1, 0xFC0D 	            
{0x36E2, 0xB4CD , 2}, // FIELD_WR= P_G2_P2Q2, 0xB4CD 	            
{0x36E4, 0x530E , 2}, // FIELD_WR= P_G2_P2Q3, 0x530E 	            
{0x36E6, 0x8DF2 , 2}, // FIELD_WR= P_G2_P2Q4, 0x8DF2 	            
{0x3700, 0x066F , 2}, // FIELD_WR= P_G1_P3Q0, 0x066F 	            
{0x3702, 0x40AD , 2}, // FIELD_WR= P_G1_P3Q1, 0x40AD 	            
{0x3704, 0x8FEF , 2}, // FIELD_WR= P_G1_P3Q2, 0x8FEF 	            
{0x3706, 0x268E , 2}, // FIELD_WR= P_G1_P3Q3, 0x268E 	            
{0x3708, 0xB310 , 2}, // FIELD_WR= P_G1_P3Q4, 0xB310 	            
{0x370A, 0x5C4E , 2}, // FIELD_WR= P_R_P3Q0, 0x5C4E 	            
{0x370C, 0xABEE , 2}, // FIELD_WR= P_R_P3Q1, 0xABEE 	            
{0x370E, 0x488F , 2}, // FIELD_WR= P_R_P3Q2, 0x488F 	            
{0x3710, 0x7430 , 2}, // FIELD_WR= P_R_P3Q3, 0x7430 	            
{0x3712, 0xEB51 , 2}, // FIELD_WR= P_R_P3Q4, 0xEB51 	            
{0x3714, 0xB24E , 2}, // FIELD_WR= P_B_P3Q0, 0xB24E 	            
{0x3716, 0xD66F , 2}, // FIELD_WR= P_B_P3Q1, 0xD66F 	            
{0x3718, 0x1FB0 , 2}, // FIELD_WR= P_B_P3Q2, 0x1FB0 	            
{0x371A, 0x5B71 , 2}, // FIELD_WR= P_B_P3Q3, 0x5B71 	            
{0x371C, 0xE8F1 , 2}, // FIELD_WR= P_B_P3Q4, 0xE8F1 	            
{0x371E, 0x89ED , 2}, // FIELD_WR= P_G2_P3Q0, 0x89ED 	            
{0x3720, 0xDB0D , 2}, // FIELD_WR= P_G2_P3Q1, 0xDB0D 	            
{0x3722, 0x2BF1 , 2}, // FIELD_WR= P_G2_P3Q2, 0x2BF1 	            
{0x3724, 0x0091 , 2}, // FIELD_WR= P_G2_P3Q3, 0x0091 	            
{0x3726, 0xDB52 , 2}, // FIELD_WR= P_G2_P3Q4, 0xDB52 	            
{0x3740, 0x9150 , 2}, // FIELD_WR= P_G1_P4Q0, 0x9150 	            
{0x3742, 0x3190 , 2}, // FIELD_WR= P_G1_P4Q1, 0x3190 	            
{0x3744, 0xD0D4 , 2}, // FIELD_WR= P_G1_P4Q2, 0xD0D4 	            
{0x3746, 0xD133 , 2}, // FIELD_WR= P_G1_P4Q3, 0xD133 	            
{0x3748, 0x3876 , 2}, // FIELD_WR= P_G1_P4Q4, 0x3876 	            
{0x374A, 0xA9F0 , 2}, // FIELD_WR= P_R_P4Q0, 0xA9F0 	            
{0x374C, 0x10B0 , 2}, // FIELD_WR= P_R_P4Q1, 0x10B0 	            
{0x374E, 0xB633 , 2}, // FIELD_WR= P_R_P4Q2, 0xB633 	            
{0x3750, 0x8293 , 2}, // FIELD_WR= P_R_P4Q3, 0x8293 	            
{0x3752, 0x61B5 , 2}, // FIELD_WR= P_R_P4Q4, 0x61B5 	            
{0x3754, 0xD92E , 2}, // FIELD_WR= P_B_P4Q0, 0xD92E 	            
{0x3756, 0x1BCC , 2}, // FIELD_WR= P_B_P4Q1, 0x1BCC 	            
{0x3758, 0x9ED4 , 2}, // FIELD_WR= P_B_P4Q2, 0x9ED4 	            
{0x375A, 0xBE33 , 2}, // FIELD_WR= P_B_P4Q3, 0xBE33 	            
{0x375C, 0x1BB6 , 2}, // FIELD_WR= P_B_P4Q4, 0x1BB6 	            
{0x375E, 0x9CD0 , 2}, // FIELD_WR= P_G2_P4Q0, 0x9CD0 	            
{0x3760, 0x51D1 , 2}, // FIELD_WR= P_G2_P4Q1, 0x51D1 	            
{0x3762, 0xBD34 , 2}, // FIELD_WR= P_G2_P4Q2, 0xBD34 	            
{0x3764, 0x8C34 , 2}, // FIELD_WR= P_G2_P4Q3, 0x8C34 	            
{0x3766, 0x2E16 , 2}, // FIELD_WR= P_G2_P4Q4, 0x2E16 	            
{0x3782, 0x03F0 , 2}, // FIELD_WR= CENTER_ROW, 0x03F0 	          
{0x3784, 0x0508 , 2}, // FIELD_WR= CENTER_COLUMN, 0x0508 	        
{0x3210, 0x49B8 , 2}, // FIELD_WR= COLOR_PIPELINE_CONTROL, 0x49B8 

//lens shading  end

//LOAD = Step5-AWB_CCM			//AWB & CCM
//AWB_setup
{0x098E, 0xAC01 , 2}, // LOGICAL_ADDRESS_ACCESS [AWB_MODE]
//[tint control and saturation]
{0xAC01, 0xFF,   1}, 	// AWB_MODE
{0xAC02, 0x007F, 2}, 	// AWB_ALGO
{0xAC96, 0x01,   1}, 	// AWB_CCM_TINTING_TH
{0xAC97, 0x68,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_0
{0xAC98, 0x80,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_1
{0xAC99, 0x8F,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_2
{0xAC9A, 0x74,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_0
{0xAC9B, 0x80,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_1
{0xAC9C, 0x82,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_2
{0xAC9D, 0x80,   1}, 	// AWB_PCR_SATURATION
{0xBC56, 0xae,   1},   //0x95 	// LL_START_CCM_SATURATION
{0xBC57, 0x00,   1}, 	// LL_END_CCM_SATURATION
//[Pre AWB ratio]
{0xACB0, 0x32,   1}, 	// AWB_RG_MIN
{0xACB1, 0x4E,   1}, 	// AWB_RG_MAX
{0xACB4, 0x26,   1}, 	// AWB_BG_MIN
{0xACB5, 0x4F,   1}, 	// AWB_BG_MAX
{0xAC3C, 0x32,   1}, 	// AWB_MIN_ACCEPTED_PRE_AWB_R2G_RATIO
{0xAC3D, 0x4E,   1}, 	// AWB_MAX_ACCEPTED_PRE_AWB_R2G_RATIO
{0xAC3E, 0x26,   1}, 	// AWB_MIN_ACCEPTED_PRE_AWB_B2G_RATIO
{0xAC3F, 0x4F,   1}, 	// AWB_MAX_ACCEPTED_PRE_AWB_B2G_RATIO
//[Weight table and checker offset]
{0x3242, 0x0000, 2}, 	// AWB_WEIGHT_R0
{0x3244, 0x0000, 2}, 	// AWB_WEIGHT_R1
{0x3246, 0x0000, 2}, 	// AWB_WEIGHT_R2
{0x3248, 0x3F00, 2}, 	// AWB_WEIGHT_R3
{0x324A, 0xA500, 2}, 	// AWB_WEIGHT_R4
{0x324C, 0x1540, 2}, 	// AWB_WEIGHT_R5
{0x324E, 0x01EC, 2}, 	// AWB_WEIGHT_R6
{0x3250, 0x007E, 2}, 	// AWB_WEIGHT_R7
{0x323C, 0x0008, 2}, 	// AWB_X_SHIFT
{0x323E, 0x001F, 2}, 	// AWB_Y_SHIFT
{0xB842, 0x003A, 2}, 	// STAT_AWB_GRAY_CHECKER_OFFSET_X
{0xB844, 0x0043, 2}, 	// STAT_AWB_GRAY_CHECKER_OFFSET_Y
//[CCM]
{0xAC46, 0x01CB, 2}, 	// AWB_LEFT_CCM_0
{0xAC48, 0xFEF7, 2}, 	// AWB_LEFT_CCM_1
{0xAC4A, 0x003F, 2}, 	// AWB_LEFT_CCM_2
{0xAC4C, 0xFFB2, 2}, 	// AWB_LEFT_CCM_3
{0xAC4E, 0x0118, 2}, 	// AWB_LEFT_CCM_4
{0xAC50, 0x0036, 2}, 	// AWB_LEFT_CCM_5
{0xAC52, 0xFFE5, 2}, 	// AWB_LEFT_CCM_6
{0xAC54, 0xFEF3, 2}, 	// AWB_LEFT_CCM_7
{0xAC56, 0x0227, 2}, 	// AWB_LEFT_CCM_8
{0xAC58, 0x00B0, 2}, 	// AWB_LEFT_CCM_R2BRATIO
{0xAC5C, 0x0254, 2}, 	// AWB_RIGHT_CCM_0
{0xAC5E, 0xFED1, 2}, 	// AWB_RIGHT_CCM_1
{0xAC60, 0xFFDB, 2}, 	// AWB_RIGHT_CCM_2
{0xAC62, 0xFFC6, 2}, 	// AWB_RIGHT_CCM_3
{0xAC64, 0x0147, 2}, 	// AWB_RIGHT_CCM_4
{0xAC66, 0xFFF2, 2}, 	// AWB_RIGHT_CCM_5
{0xAC68, 0x0008, 2}, 	// AWB_RIGHT_CCM_6
{0xAC6A, 0xFF6A, 2}, 	// AWB_RIGHT_CCM_7
{0xAC6C, 0x018E, 2}, 	// AWB_RIGHT_CCM_8
{0xAC6E, 0x0076, 2}, 	// AWB_RIGHT_CCM_R2BRATIO
{0xB83E, 0x00,   1}, 	// STAT_AWB_WINDOW_POS_X
{0xB83F, 0x00,   1}, 	// STAT_AWB_WINDOW_POS_Y
{0xB840, 0xFF,   1}, 	// STAT_AWB_WINDOW_SIZE_X
{0xB841, 0xEF,   1}, 	// STAT_AWB_WINDOW_SIZE_Y
{0x8404, 0x05,   1}, 	// SEQ_CMD
//[color kill]
{0xDC36, 0x43,   1}, 	// SYS_DARK_COLOR_KILL
{0xDC02, 0x303E, 2}, 	// SYS_ALGO
{0x35A2, 0x00A3, 2}, 	// DARK_COLOR_KILL_CONTROLS

//LOAD = Step6-CPIPE_Calibration	//Color Pipe Calibration settings, if any
//jpeg_setup
{0x098E, 0xD80F , 2}, // LOGICAL_ADDRESS_ACCESS [JPEG_QSCALE_0]
{0xD80F, 0x04   , 1}, // JPEG_QSCALE_0
{0xD810, 0x08   , 1}, // JPEG_QSCALE_1
{0xC8D2, 0x04   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_0
{0xC8D3, 0x08   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_1
{0xC8BC, 0x04   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_0
{0xC8BD, 0x08   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_1

//Sys_Settings
{0x301A, 0x10F4 , 2}, // RESET_REGISTER
{0x301E, 0x0000 , 2}, // DATA_PEDESTAL
{0x301A, 0x10FC , 2}, // RESET_REGISTER
{0x098E, 0xDC33 , 2}, // LOGICAL_ADDRESS_ACCESS [SYS_FIRST_BLACK_LEVEL]
{0xDC33, 0x00   , 1}, // SYS_FIRST_BLACK_LEVEL
{0xDC35, 0x04   , 1}, // SYS_UV_COLOR_BOOST
{0x326E, 0x0006 , 2}, // LOW_PASS_YUV_FILTER
{0xDC37, 0x62   , 1}, // SYS_BRIGHT_COLORKILL
{0x35A4, 0x0596 , 2}, // BRIGHT_COLOR_KILL_CONTROLS
{0x35A2, 0x0094 , 2}, // DARK_COLOR_KILL_CONTROLS
{0xDC36, 0x23   , 1}, // SYS_DARK_COLOR_KILL





//Gamma_Curves_REV3
{0x098E, 0xBC18 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_GAMMA_CONTRAST_CURVE_0]
{0xBC18, 0x00   , 1}, // LL_GAMMA_CONTRAST_CURVE_0
{0xBC19, 0x11   , 1}, // LL_GAMMA_CONTRAST_CURVE_1
{0xBC1A, 0x23   , 1}, // LL_GAMMA_CONTRAST_CURVE_2
{0xBC1B, 0x50   , 1}, // LL_GAMMA_CONTRAST_CURVE_3
{0xBC1C, 0x7A   , 1}, // LL_GAMMA_CONTRAST_CURVE_4
{0xBC1D, 0x85   , 1}, // LL_GAMMA_CONTRAST_CURVE_5
{0xBC1E, 0x9B   , 1}, // LL_GAMMA_CONTRAST_CURVE_6
{0xBC1F, 0xAD   , 1}, // LL_GAMMA_CONTRAST_CURVE_7
{0xBC20, 0xBB   , 1}, // LL_GAMMA_CONTRAST_CURVE_8
{0xBC21, 0xC7   , 1}, // LL_GAMMA_CONTRAST_CURVE_9
{0xBC22, 0xD1   , 1}, // LL_GAMMA_CONTRAST_CURVE_10
{0xBC23, 0xDA   , 1}, // LL_GAMMA_CONTRAST_CURVE_11
{0xBC24, 0xE1   , 1}, // LL_GAMMA_CONTRAST_CURVE_12
{0xBC25, 0xE8   , 1}, // LL_GAMMA_CONTRAST_CURVE_13
{0xBC26, 0xEE   , 1}, // LL_GAMMA_CONTRAST_CURVE_14
{0xBC27, 0xF3   , 1}, // LL_GAMMA_CONTRAST_CURVE_15
{0xBC28, 0xF7   , 1}, // LL_GAMMA_CONTRAST_CURVE_16
{0xBC29, 0xFB   , 1}, // LL_GAMMA_CONTRAST_CURVE_17
{0xBC2A, 0xFF   , 1}, // LL_GAMMA_CONTRAST_CURVE_18
{0xBC2B, 0x00   , 1}, // LL_GAMMA_NEUTRAL_CURVE_0
{0xBC2C, 0x11   , 1}, // LL_GAMMA_NEUTRAL_CURVE_1
{0xBC2D, 0x23   , 1}, // LL_GAMMA_NEUTRAL_CURVE_2
{0xBC2E, 0x50   , 1}, // LL_GAMMA_NEUTRAL_CURVE_3
{0xBC2F, 0x7A   , 1}, // LL_GAMMA_NEUTRAL_CURVE_4
{0xBC30, 0x85   , 1}, // LL_GAMMA_NEUTRAL_CURVE_5
{0xBC31, 0x9B   , 1}, // LL_GAMMA_NEUTRAL_CURVE_6
{0xBC32, 0xAD   , 1}, // LL_GAMMA_NEUTRAL_CURVE_7
{0xBC33, 0xBB   , 1}, // LL_GAMMA_NEUTRAL_CURVE_8
{0xBC34, 0xC7   , 1}, // LL_GAMMA_NEUTRAL_CURVE_9
{0xBC35, 0xD1   , 1}, // LL_GAMMA_NEUTRAL_CURVE_10
{0xBC36, 0xDA   , 1}, // LL_GAMMA_NEUTRAL_CURVE_11
{0xBC37, 0xE1   , 1}, // LL_GAMMA_NEUTRAL_CURVE_12
{0xBC38, 0xE8   , 1}, // LL_GAMMA_NEUTRAL_CURVE_13
{0xBC39, 0xEE   , 1}, // LL_GAMMA_NEUTRAL_CURVE_14
{0xBC3A, 0xF3   , 1}, // LL_GAMMA_NEUTRAL_CURVE_15
{0xBC3B, 0xF7   , 1}, // LL_GAMMA_NEUTRAL_CURVE_16
{0xBC3C, 0xFB   , 1}, // LL_GAMMA_NEUTRAL_CURVE_17
{0xBC3D, 0xFF   , 1}, // LL_GAMMA_NEUTRAL_CURVE_18
{0xBC3E, 0x00   , 1}, // LL_GAMMA_NR_CURVE_0
{0xBC3F, 0x18   , 1}, // LL_GAMMA_NR_CURVE_1
{0xBC40, 0x25   , 1}, // LL_GAMMA_NR_CURVE_2
{0xBC41, 0x3A   , 1}, // LL_GAMMA_NR_CURVE_3
{0xBC42, 0x59   , 1}, // LL_GAMMA_NR_CURVE_4
{0xBC43, 0x70   , 1}, // LL_GAMMA_NR_CURVE_5
{0xBC44, 0x81   , 1}, // LL_GAMMA_NR_CURVE_6
{0xBC45, 0x90   , 1}, // LL_GAMMA_NR_CURVE_7
{0xBC46, 0x9E   , 1}, // LL_GAMMA_NR_CURVE_8
{0xBC47, 0xAB   , 1}, // LL_GAMMA_NR_CURVE_9
{0xBC48, 0xB6   , 1}, // LL_GAMMA_NR_CURVE_10
{0xBC49, 0xC1   , 1}, // LL_GAMMA_NR_CURVE_11
{0xBC4A, 0xCB   , 1}, // LL_GAMMA_NR_CURVE_12
{0xBC4B, 0xD5   , 1}, // LL_GAMMA_NR_CURVE_13
{0xBC4C, 0xDE   , 1}, // LL_GAMMA_NR_CURVE_14
{0xBC4D, 0xE7   , 1}, // LL_GAMMA_NR_CURVE_15
{0xBC4E, 0xEF   , 1}, // LL_GAMMA_NR_CURVE_16
{0xBC4F, 0xF7   , 1}, // LL_GAMMA_NR_CURVE_17
{0xBC50, 0xFF   , 1}, // LL_GAMMA_NR_CURVE_18


//BM_Dampening
{0x098E, 0xB801 , 2}, // LOGICAL_ADDRESS_ACCESS [STAT_MODE]
{0xB801, 0xE0   , 1}, // STAT_MODE
{0xB862, 0x04   , 1}, // STAT_BMTRACKING_SPEED


//AE
{0x098E, 0xB829 , 2}, // LOGICAL_ADDRESS_ACCESS [STAT_LL_BRIGHTNESS_METRIC_DIVISOR]
{0xB829, 0x02   , 1}, // STAT_LL_BRIGHTNESS_METRIC_DIVISOR
{0xB863, 0x02   , 1}, // STAT_BM_MUL
{0xB827, 0x0A   , 1}, // STAT_AE_EV_SHIFT //0919
{0xA40D, 0xF5,   1},
{0xA40E, 0x00,   1},
{0xA40F, 0xF4,   1},
{0xA410, 0xE8,   1},
{0xA411, 0xFB,   1},
//{0xA409, 0x37   , 1}, // AE_RULE_BASE_TARGET //wangaicui
{0xA409, 0x44   , 1}, // AE_RULE_BASE_TARGET


//BM_GM_Start_Stop
{0x098E, 0x3C52 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_BRIGHTNESS_METRIC]
{0xBC52, 0x00C8 , 2}, // LL_START_BRIGHTNESS_METRIC
{0xBC54, 0x0A28 , 2}, // LL_END_BRIGHTNESS_METRIC
{0xBC58, 0x00C8 , 2}, // LL_START_GAIN_METRIC
{0xBC5A, 0x12C0 , 2}, // LL_END_GAIN_METRIC
{0xBC5E, 0x00FA , 2}, // LL_START_APERTURE_GAIN_BM
{0xBC60, 0x0258 , 2}, // LL_END_APERTURE_GAIN_BM
{0xBC66, 0x00FA , 2}, // LL_START_APERTURE_GM
{0xBC68, 0x0258 , 2}, // LL_END_APERTURE_GM
{0xBC86, 0x00C8 , 2}, // LL_START_FFNR_GM
{0xBC88, 0x0640 , 2}, // LL_END_FFNR_GM
{0xBCBC, 0x0040 , 2}, // LL_SFFB_START_GAIN
{0xBCBE, 0x01FC , 2}, // LL_SFFB_END_GAIN
{0xBCCC, 0x00C8 , 2}, // LL_SFFB_START_MAX_GM
{0xBCCE, 0x0640 , 2}, // LL_SFFB_END_MAX_GM
{0xBC90, 0x00C8 , 2}, // LL_START_GRB_GM
{0xBC92, 0x0640 , 2}, // LL_END_GRB_GM
{0xBC0E, 0x0001 , 2}, // LL_GAMMA_CURVE_ADJ_START_POS
{0xBC10, 0x0002 , 2}, // LL_GAMMA_CURVE_ADJ_MID_POS
{0xBC12, 0x02BC , 2}, // LL_GAMMA_CURVE_ADJ_END_POS
{0xBCAA, 0x044C , 2}, // LL_CDC_THR_ADJ_START_POS
{0xBCAC, 0x00AF , 2}, // LL_CDC_THR_ADJ_MID_POS
{0xBCAE, 0x0009 , 2}, // LL_CDC_THR_ADJ_END_POS
{0xBCD8, 0x00C8 , 2}, // LL_PCR_START_BM
{0xBCDA, 0x0A28 , 2}, // LL_PCR_END_BM



//noise model tuning...change only if new model has been completed.
//Kernel
{0x3380, 0x0504 , 2}, // KERNEL_CONFIG

//GRB
{0x098E, 0xBC94 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_GB_START_THRESHOLD_0]
{0xBC94, 0x0C   , 1},	// LL_GB_START_THRESHOLD_0
{0xBC95, 0x08   , 1}, // LL_GB_START_THRESHOLD_1
{0xBC9C, 0x3C   , 1}, // LL_GB_END_THRESHOLD_0
{0xBC9D, 0x28   , 1}, // LL_GB_END_THRESHOLD_1

//Demosaic_REV3
{0x33B0, 0x2A16 , 2}, // FFNR_ALPHA_BETA
{0x098E, 0xBC8A , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_FF_MIX_THRESH_Y]
{0xBC8A, 0x02   , 1}, // LL_START_FF_MIX_THRESH_Y
{0xBC8B, 0x0F   , 1}, // LL_END_FF_MIX_THRESH_Y
{0xBC8C, 0xFF   , 1}, // LL_START_FF_MIX_THRESH_YGAIN
{0xBC8D, 0xFF   , 1}, // LL_END_FF_MIX_THRESH_YGAIN
{0xBC8E, 0xFF   , 1}, // LL_START_FF_MIX_THRESH_GAIN
{0xBC8F, 0x00   , 1}, // LL_END_FF_MIX_THRESH_GAIN


//CDC
{0x098E, 0xBCB2 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_CDC_DARK_CLUS_SLOPE]
{0xBCB2, 0x20   , 1}, // LL_CDC_DARK_CLUS_SLOPE
{0xBCB3, 0x3A   , 1}, // LL_CDC_DARK_CLUS_SATUR
{0xBCB4, 0x39   , 1}, // LL_CDC_BRIGHT_CLUS_LO_LIGHT_SLOPE
{0xBCB7, 0x39   , 1}, // LL_CDC_BRIGHT_CLUS_LO_LIGHT_SATUR
{0xBCB5, 0x20   , 1}, // LL_CDC_BRIGHT_CLUS_MID_LIGHT_SLOPE
{0xBCB8, 0x3A   , 1}, // LL_CDC_BRIGHT_CLUS_MID_LIGHT_SATUR
{0xBCB6, 0x80   , 1}, // LL_CDC_BRIGHT_CLUS_HI_LIGHT_SLOPE
{0xBCB9, 0x24   , 1}, // LL_CDC_BRIGHT_CLUS_HI_LIGHT_SATUR
{0xBCAA, 0x03E8 , 2}, // LL_CDC_THR_ADJ_START_POS
{0xBCAC, 0x012C , 2}, // LL_CDC_THR_ADJ_MID_POS
{0xBCAE, 0x0009 , 2}, // LL_CDC_THR_ADJ_END_POS

//Aperture_calib
{0x33BA, 0x0084 , 2}, // APEDGE_CONTROL
{0x33BE, 0x0000 , 2}, // UA_KNEE_L
{0x33C2, 0x8800 , 2}, // UA_WEIGHTS
{0x098E, 0x3C5E , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_APERTURE_GAIN_BM]
{0xBC5E, 0x0154 , 2}, // LL_START_APERTURE_GAIN_BM
{0xBC60, 0x0640 , 2}, // LL_END_APERTURE_GAIN_BM
{0xBC62, 0x0E   , 1}, // LL_START_APERTURE_KPGAIN
{0xBC63, 0x14   , 1}, // LL_END_APERTURE_KPGAIN
{0xBC64, 0x0E   , 1}, // LL_START_APERTURE_KNGAIN
{0xBC65, 0x14   , 1}, // LL_END_APERTURE_KNGAIN
{0xBCE2, 0x0A   , 1}, // LL_START_POS_KNEE
{0xBCE3, 0x2B   , 1}, // LL_END_POS_KNEE
{0xBCE4, 0x0A   , 1}, // LL_START_NEG_KNEE
{0xBCE5, 0x2B   , 1}, // LL_END_NEG_KNEE



//SFFB_REV3_noisemodel
{0x098E, 0xBCC0 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_SFFB_RAMP_START]
{0xBCC0, 0x1F   , 1}, // LL_SFFB_RAMP_START
{0xBCC1, 0x03   , 1}, // LL_SFFB_RAMP_STOP
{0xBCC2, 0x2C   , 1}, // LL_SFFB_SLOPE_START
{0xBCC3, 0x10   , 1}, // LL_SFFB_SLOPE_STOP
{0xBCC4, 0x07   , 1}, // LL_SFFB_THSTART
{0xBCC5, 0x0B   , 1}, // LL_SFFB_THSTOP
{0xBCBA, 0x0009 , 2}, // LL_SFFB_CONFIG



//Step7-CPIPE_Preference	//Color Pipe preference settings, if any
//FTB_OFF
{0x098E, 0x3C14 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_GAMMA_FADE_TO_BLACK_START_POS]
{0xBC14, 0xFFFE , 2}, // LL_GAMMA_FADE_TO_BLACK_START_POS
{0xBC16, 0xFFFF , 2}, // LL_GAMMA_FADE_TO_BLACK_END_POS

//LOAD=Aperture_preference
{0x098E, 0x3C66 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_APERTURE_GM]
{0xBC66, 0x0154 , 2}, // LL_START_APERTURE_GM
{0xBC68, 0x07D0 , 2}, // LL_END_APERTURE_GM
{0xBC6A, 0x04   , 1}, // LL_START_APERTURE_INTEGER_GAIN
{0xBC6B, 0x00   , 1}, // LL_END_APERTURE_INTEGER_GAIN
{0xBC6C, 0x00   , 1}, // LL_START_APERTURE_EXP_GAIN
{0xBC6D, 0x00   , 1}, // LL_END_APERTURE_EXP_GAIN

//Gain_max
{0x098E, 0x281C , 2}, // LOGICAL_ADDRESS_ACCESS [AE_TRACK_MIN_AGAIN]
//{0xA81C, 0x0040 , 2}, // AE_TRACK_MIN_AGAIN
{0xA81C, 0x0060 , 2}, // AE_TRACK_MIN_AGAIN
{0xA820, 0x01FC , 2}, // AE_TRACK_MAX_AGAIN
{0xA822, 0x0080 , 2}, // AE_TRACK_MIN_DGAIN
{0xA824, 0x0100 , 2}, // AE_TRACK_MAX_DGAIN

//Saturation_REV3
{0x098E, 0xBC56 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_CCM_SATURATION]
{0xBC56, 0x64   , 1}, // LL_START_CCM_SATURATION
{0xBC57, 0x1E   , 1}, // LL_END_CCM_SATURATION

//DCCM_REV3
{0x098E, 0xBCDE , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_SYS_THRESHOLD]
{0xBCDE, 0x03   , 1}, // LL_START_SYS_THRESHOLD
{0xBCDF, 0x50   , 1}, // LL_STOP_SYS_THRESHOLD
{0xBCE0, 0x08   , 1}, // LL_START_SYS_GAIN
{0xBCE1, 0x03   , 1}, // LL_STOP_SYS_GAIN

//Sobel_REV3
{0x098E, 0x3CD0 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_SFFB_SOBEL_FLAT_START]
{0xBCD0, 0x000A , 2}, // LL_SFFB_SOBEL_FLAT_START
{0xBCD2, 0x00FE , 2}, // LL_SFFB_SOBEL_FLAT_STOP
{0xBCD4, 0x001E , 2}, // LL_SFFB_SOBEL_SHARP_START
{0xBCD6, 0x00FF , 2}, // LL_SFFB_SOBEL_SHARP_STOP
{0xBCC6, 0x00   , 1}, // LL_SFFB_SHARPENING_START
{0xBCC7, 0x00   , 1}, // LL_SFFB_SHARPENING_STOP
{0xBCC8, 0x20   , 1}, // LL_SFFB_FLATNESS_START
{0xBCC9, 0x40   , 1}, // LL_SFFB_FLATNESS_STOP
{0xBCCA, 0x04   , 1}, // LL_SFFB_TRANSITION_START
{0xBCCB, 0x00   , 1}, // LL_SFFB_TRANSITION_STOP

//SFFB_slope_zero_enable
{0x098E, 0xBCE6 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_SFFB_ZERO_ENABLE]
{0xBCE6, 0x03   , 1}, // LL_SFFB_ZERO_ENABLE
{0xBCE6, 0x03   , 1}, // LL_SFFB_ZERO_ENABLE

//AE_preference
{0x098E, 0xA410 , 2}, // LOGICAL_ADDRESS_ACCESS [AE_RULE_TARGET_AE_6]
{0xA410, 0x04   , 1}, // AE_RULE_TARGET_AE_6
{0xA411, 0x06   , 1}, // AE_RULE_TARGET_AE_7

//fix auto focus //20110425//begin
{0x098E, 0xB045 , 2}, 
{0xB045, 0x0014 , 2}, //20110425//end

//AF OFF//20110425//begin
//{0x098E, 0x8419 , 2}, 
//{0x8419, 0x00   , 1}, 

{0x098E, 0xC428 , 2}, 
{0xC428, 0x82   , 1}, 
//AF OFF//20110425//off

//Step8-Features		  //Ports, special features, etc., if any
{0x098E, 0xC8BC , 2}, // LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_0_JPEG_QSCALE_0]
{0xC8BC, 0x04   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_0
{0xC8BD, 0x0A   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_1
{0xC8D2, 0x04   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_0
{0xC8D3, 0x0A   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_1
{0xDC3A, 0x23   , 1}, // SYS_SEPIA_CR
{0xDC3B, 0xB2   , 1}, // SYS_SEPIA_CB

{0x098E, 0x8404 , 2}, 
{0x8404, 0x06   , 1}, // SEQ_CMD
{0     , 300    , 0},
//delay=300

{0x0018, 0x2008 , 2}, // STANDBY_CONTROL_AND_STATUS
//delay=100
{0     , 100    , 0},
//{0x098E, 0xA818 , 2}, 
//{0xA818, 0x0756 , 2}, //
//{0xA81A, 0x0756 , 2}, // frame rate//wangaicui low frame rate

{0x098E, 0xA816 , 2}, 
{0xA816, 0x0005 , 2}, //0x0680 //up limit
{0xA818, 0x0756 , 2}, //
{0xA81A, 0x0D00 , 2}, // frame rate//wangaicui low frame rate

{0x301A, 0x107C , 2}, // RESET_REGISTER
{0x3400, 0x7A26 , 2}, // MIPI_CONTROL
{0x001A, 0x0018 , 2}, // RESET_AND_MISC_CONTROL
{0x001A, 0x001C , 2}, // RESET_AND_MISC_CONTROL
{0x3CA0, 0x0001 , 2}, // TXSS_PARAMETERS
{0x098E, 0x48D4 , 2}, // LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_1_MIPICHANNEL]
{0xC8D4, 0x0000 , 2}, // CAM_OUTPUT_1_MIPICHANNEL
{0x3CA2, 0x0087 , 2}, // TXC_PARAMETERS
{0x3402, 0x0011 , 2}, // MIPI_STATUS
{0x3400, 0x7A24 , 2}, // MIPI_CONTROL

//[Optimized Low Power]
{0x3ED0, 0x2409 , 2}, // DAC_LD_4_5
{0x3EDE, 0x0A49 , 2}, // DAC_LD_18_19
{0x3EE0, 0x4609 , 2}, // DAC_LD_20_21
{0x3EE2, 0x09CC , 2}, // DAC_LD_22_23
{0x3EDA, 0x6060 , 2},	// DAC_LD_14_15
{0x3170, 0x2096 , 2}, // ANALOG_CONTROL
{0x8404, 0x05   , 1}, // SEQ_CMD
//POLL_{0x8404,0x00,!=0x00,DELAY=10,TIMEOUT=100
{0     , 300     , 0},       
{0x0028,0x0000 , 2},   //for software standby
};
#else
struct reg_addr_val_pair_struct mt9p111_init_settings_A_array[] = {
#if MT9P111_TARGET_PLL_TIMING_96M
// [PLL_settings]
{0x0010, 0x0338 , 2},// PLL_DIVIDERS
{0x0012, 0x0070 , 2},	// PLL_P_DIVIDERS
{0x0014, 0x2025 , 2},	// PLL_CONTROL
{0x001E, 0x0665 , 2},	// PAD_SLEW_PAD_CONFIG
{0x0022, 0x0030	, 2},//1E0 // VDD_DIS_COUNTER
{0x002A, 0x7F7C , 2},	// PLL_P4_P5_P6_DIVIDERS
{0x002C, 0x0000	, 2},//, WORD_LEN, 0},
{0x002E, 0x0000	, 2},//, WORD_LEN, 0},
{0x0018, 0x4008	, 2},//, WORD_LEN, 0}, // STANDBY_CONTROL_AND_STATUS
//POLL_{0x0018,0xE07F,!=0x2008,DELAY=10,TIMEOUT=100	//Wait for the core ready
{0     , 100    , 0},
{0x0010, 0x0338 , 2},	// PLL_DIVIDERS

//timing_settings_
{0x098E, 0x483A , 2},	// LOGICAL_ADDRESS_ACCESS
{0xC83A, 0x000C , 2},	// CAM_CORE_A_Y_ADDR_START
{0xC83C, 0x0018 , 2},	// CAM_CORE_A_X_ADDR_START
{0xC83E, 0x07B1 , 2},	// CAM_CORE_A_Y_ADDR_END
{0xC840, 0x0A45 , 2},	// CAM_CORE_A_X_ADDR_END
{0xC842, 0x0001 , 2},	// CAM_CORE_A_ROW_SPEED
{0xC844, 0x0103 , 2},	// CAM_CORE_A_SKIP_X_CORE
{0xC846, 0x0103 , 2},	// CAM_CORE_A_SKIP_Y_CORE
{0xC848, 0x0103 , 2},	// CAM_CORE_A_SKIP_X_PIPE
{0xC84A, 0x0103 , 2},	// CAM_CORE_A_SKIP_Y_PIPE
{0xC84C, 0x0096 , 2},	// CAM_CORE_A_POWER_MODE [101124 利侩] 0x00F6-> 0x0096
{0xC84E, 0x0001 , 2},	// CAM_CORE_A_BIN_MODE
{0xC850, 0x03 ,    1},// CAM_CORE_A_ORIENTATION
{0xC851, 0x00 ,    1},// CAM_CORE_A_PIXEL_ORDER
{0xC852, 0x019C , 2},	// CAM_CORE_A_FINE_CORRECTION
{0xC854, 0x0732 , 2},	// CAM_CORE_A_FINE_ITMIN
{0xC858, 0x0000 , 2},	// CAM_CORE_A_COARSE_ITMIN
{0xC85A, 0x0001 , 2},	// CAM_CORE_A_COARSE_ITMAX_MARGIN
{0xC85C, 0x0423 , 2},	// CAM_CORE_A_MIN_FRAME_LENGTH_LINES
{0xC85E, 0xFFFF , 2},	// CAM_CORE_A_MAX_FRAME_LENGTH_LINES
{0xC860, 0x0423 , 2},	// CAM_CORE_A_BASE_FRAME_LENGTH_LINES
{0xC862, 0x0F50 , 2},	// CAM_CORE_A_MIN_LINE_LENGTH_PCLK
{0xC864, 0x0F50 , 2},	// CAM_CORE_A_MAX_LINE_LENGTH_PCLK
{0xC866, 0x7F7C , 2},	// CAM_CORE_A_P4_5_6_DIVIDER
{0xC868, 0x0423 , 2},	// CAM_CORE_A_FRAME_LENGTH_LINES
{0xC86A, 0x0F50 , 2},	// CAM_CORE_A_LINE_LENGTH_PCK
{0xC86C, 0x0518 , 2},	// CAM_CORE_A_OUTPUT_SIZE_WIDTH
{0xC86E, 0x03D4 , 2},	// CAM_CORE_A_OUTPUT_SIZE_HEIGHT
{0xC870, 0x0014 , 2},	// CAM_CORE_A_RX_FIFO_TRIGGER_MARK
{0xC858, 0x0002 , 2},	// CAM_CORE_A_COARSE_ITMIN
{0xC8B8, 0x0004 , 2},	// CAM_OUTPUT_0_JPEG_CONTROL
{0xC8AE, 0x0001 , 2},	// CAM_OUTPUT_0_OUTPUT_FORMAT
{0xC8AA, 0x0500	, 2}, //500 // CAM_OUTPUT_0_IMAGE_WIDTH
{0xC8AC, 0x03C0	, 2},//3C0 // CAM_OUTPUT_0_IMAGE_HEIGHT
//[Full_Res_Settings_JPEG_Fullspeed]
//;Max Framerate in Full Res
{0x098E, 0x4872 , 2},	// LOGICAL_ADDRESS_ACCESS
{0xC872, 0x0010 , 2},	// CAM_CORE_B_Y_ADDR_START
{0xC874, 0x001C , 2},	// CAM_CORE_B_X_ADDR_START
{0xC876, 0x07AF , 2},	// CAM_CORE_B_Y_ADDR_END
{0xC878, 0x0A43 , 2},	// CAM_CORE_B_X_ADDR_END
{0xC87A, 0x0001 , 2},	// CAM_CORE_B_ROW_SPEED
{0xC87C, 0x0101 , 2},	// CAM_CORE_B_SKIP_X_CORE
{0xC87E, 0x0101 , 2},	// CAM_CORE_B_SKIP_Y_CORE
{0xC880, 0x0101 , 2},	// CAM_CORE_B_SKIP_X_PIPE
{0xC882, 0x0101 , 2},	// CAM_CORE_B_SKIP_Y_PIPE
{0xC884, 0x005C , 2},	// CAM_CORE_B_POWER_MODE [101124 利侩] 0x00F2-> 0x005C
{0xC886, 0x0000 , 2},	// CAM_CORE_B_BIN_MODE
{0xC888, 0x03,     1},// CAM_CORE_B_ORIENTATION
{0xC889, 0x00,     1},// CAM_CORE_B_PIXEL_ORDER
{0xC88A, 0x009C , 2},	// CAM_CORE_B_FINE_CORRECTION
{0xC88C, 0x034A , 2},	// CAM_CORE_B_FINE_ITMIN
{0xC890, 0x0000 , 2},	// CAM_CORE_B_COARSE_ITMIN
{0xC892, 0x0001 , 2},	// CAM_CORE_B_COARSE_ITMAX_MARGIN
{0xC894, 0x07EF , 2},	// CAM_CORE_B_MIN_FRAME_LENGTH_LINES
{0xC896, 0xFFFF , 2},	// CAM_CORE_B_MAX_FRAME_LENGTH_LINES
{0xC898, 0x082F , 2},	// CAM_CORE_B_BASE_FRAME_LENGTH_LINES
{0xC89A, 0x1964 , 2},	// CAM_CORE_B_MIN_LINE_LENGTH_PCLK
{0xC89C, 0xFFFE , 2},	// CAM_CORE_B_MAX_LINE_LENGTH_PCLK
{0xC89E, 0x7F7C , 2},	// CAM_CORE_B_P4_5_6_DIVIDER
{0xC8A0, 0x07EF , 2},	// CAM_CORE_B_FRAME_LENGTH_LINES
{0xC8A2, 0x1F40 , 2},	// CAM_CORE_B_LINE_LENGTH_PCK
{0xC8A4, 0x0A28 , 2},	// CAM_CORE_B_OUTPUT_SIZE_WIDTH
{0xC8A6, 0x07A0 , 2},	// CAM_CORE_B_OUTPUT_SIZE_HEIGHT
{0xC8A8, 0x0124 , 2},	// CAM_CORE_B_RX_FIFO_TRIGGER_MARK
{0xC890, 0x0002 , 2},	// CAM_CORE_B_COARSE_ITMIN
{0xC8C0, 0x0A20 , 2},	// CAM_OUTPUT_1_IMAGE_WIDTH
{0xC89A, 0x1F40 , 2},	// CAM_CORE_B_MIN_LINE_LENGTH_PCLK
{0xC8A2, 0x1F40 , 2},	// CAM_CORE_B_LINE_LENGTH_PCK
{0xC8C4, 0x0001 , 2},	// CAM_OUTPUT_1_OUTPUT_FORMAT
{0xC8C6, 0x0000 , 2},	// CAM_OUTPUT_1_OUTPUT_FORMAT_ORDER
{0xC8CE, 0x0014 , 2},	// CAM_OUTPUT_1_JPEG_CONTROL
{0xD822, 0x4610 , 2},	// JPEG_JPSS_CTRL_VAR
{0x3330, 0x0000 , 2},	// OUTPUT_FORMAT_TEST
// [Flicker new Flicker setting for new PLL - 4/29/11]
{0x098E, 0x2018 , 2},	// LOGICAL_ADDRESS_ACCESS [FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_A]
{0xA018, 0x0107 , 2},	// FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_A
{0xA01A, 0x00B6 , 2},	// FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_B
{0xA01C, 0x00DA , 2},	// FD_EXPECTED60HZ_FLICKER_PERIOD_IN_CONTEXT_A
{0xA01E, 0x0097 , 2},	// FD_EXPECTED60HZ_FLICKER_PERIOD_IN_CONTEXT_B
{0xA010, 0x00F3 , 2},	// FD_MIN_EXPECTED50HZ_FLICKER_PERIOD
{0xA012, 0x0111 , 2},	// FD_MAX_EXPECTED50HZ_FLICKER_PERIOD
{0xA014, 0x00C6 , 2},	// FD_MIN_EXPECTED60HZ_FLICKER_PERIOD
{0xA016, 0x00E4,  2},	// FD_MAX_EXPECTED60HZ_FLICKER_PERIOD

//[MIPI Timing Registers]
{0x341A, 0x0A0C, 2}, // MIPI_INIT_TIMING
{0x3418, 0x0006, 2}, // MIPI_TIMING_T_LPX
{0x3416, 0x071D, 2}, // MIPI_T_CLK_TRAIL_CLK_ZERO
{0x3414, 0x0D01, 2}, // MIPI_TIMING_T_CLK_POST_CLK_PRE
{0x3412, 0x0B07, 2}, // MIPI_TIMING_T_HS_EXIT_HS_TRAIL
{0x3410, 0x0F00, 2}, // MIPI_TIMING_T_HS_ZERO

{0x301A, 0x107C, 2},  // RESET_REGISTER
{0x3400, 0x7A26, 2},  // MIPI_CONTROL
{0x001A, 0x0018, 2},  // RESET_AND_MISC_CONTROL
{0x001A, 0x001C, 2},  // RESET_AND_MISC_CONTROL
{0x3CA0, 0x0001, 2},  // TXSS_PARAMETERS
{0x098E, 0x48D4, 2},  // LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_1_MIPICHANNEL]
{0xC8D4, 0x0000, 2},  // CAM_OUTPUT_1_MIPICHANNEL
{0x3CA2, 0x0087, 2},  // TXC_PARAMETERS
{0x3402, 0x0011, 2},  // MIPI_STATUS
{0x3400, 0x7A24, 2},  // MIPI_CONTROL
#endif

#if MT9P111_TARGET_PLL_TIMING_76M
//[MT9P111 (SOC5140) Register Wizard Defaults]
{0x0010, 0x0339, 2},	//PLL Dividers = 825
{0x0012, 0x0080, 2},	//PLL P Dividers = 128
{0x0014, 0x2025, 2},	//PLL Control: TEST_BYPASS off = 8229
{0x001E, 0x0665 , 2},	// PAD_SLEW_PAD_CONFIG
{0x0022, 0x0048, 2},	//VDD_DIS counter delay
{0x002A, 0x7F7F, 2},	//PLL P Dividers 4-5-6 = 32638
{0x002C, 0x0000, 2},	//PLL P Dividers 7 = 0
{0x002E, 0x0000, 2},	//Sensor Clock Divider = 0
{0x0018, 0x4008	, 2},//, WORD_LEN, 0}, // STANDBY_CONTROL_AND_STATUS
//POLL_{0x0018,0xE07F,!=0x2008,DELAY=10,TIMEOUT=100	//Wait for the core ready
{0     , 100    , 0},
{0x0010, 0x0339 , 2},	// PLL_DIVIDERS

{0x3CAA, 0x0F0F, 2}, // TXC_TIMING
//[MIPI Timing Registers]
{0x341A, 0x0A0C,  2},  // MIPI_INIT_TIMING
{0x3418, 0x0006,  2},  // MIPI_TIMING_T_LPX
{0x3416, 0x071D,  2},  // MIPI_T_CLK_TRAIL_CLK_ZERO
{0x3414, 0x0D01,  2},   // MIPI_TIMING_T_CLK_POST_CLK_PRE
{0x3412, 0x0B07,  2},   // MIPI_TIMING_T_HS_EXIT_HS_TRAIL
{0x3410, 0x0F00,  2},  // MIPI_TIMING_T_HS_ZERO

{0x301A, 0x107C,  2},   // RESET_REGISTER
{0x3400, 0x7A26,  2},    // MIPI_CONTROL
{0x001A, 0x0018,  2},    // RESET_AND_MISC_CONTROL
{0x001A, 0x001C,  2},    // RESET_AND_MISC_CONTROL
{0x3CA0, 0x0001,  2},   // TXSS_PARAMETERS
{0x098E, 0x48D4, 2},   // LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_1_MIPICHANNEL]
{0xC8D4, 0x0000, 2},    // CAM_OUTPUT_1_MIPICHANNEL
{0x3CA2, 0x0087, 2},    // TXC_PARAMETERS
{0x3402, 0x0011, 2},    // MIPI_STATUS
{0x3400, 0x7A24, 2},    // MIPI_CONTROL

// MIPI settings to try recommended by design  //0825 begin
{0x3400, 0x7A25, 2},     // MIPI_CONTROL
{0x341A, 0x0A0C, 2},     // MIPI_INIT_TIMING
{0x3400, 0x7A24, 2},     // MIPI_CONTROL
{0x3CA0, 0x0001, 2},     // TXSS_PARAMETERS
{0x3CA2, 0x0087, 2},     // TXC_PARAMETERS
{0x3CAA, 0x0F0F, 2},     // TXC_TIMING
{0xC8D4, 0x0000, 2},     // CAM_OUTPUT_1_MIPI_CHANNEL
{0xD822, 0x4710, 2},     // JPEG_JPSS_CTRL_VAR

{0x098E, 0x1000, 2},
{0xC86C, 0x0518, 2},	//Output Width (A) = 1304
{0xC86E, 0x03D4, 2},	//Output Height (A) = 980
{0xC83A, 0x000C, 2},	//Row Start (A) = 12
{0xC83C, 0x0018, 2},	//Column Start (A) = 24
{0xC83E, 0x07B1, 2},	//Row End (A) = 1969
{0xC840, 0x0A45, 2},	//Column End (A) = 2629
{0xC842, 0x0001, 2},	//Row Speed (A) = 1
{0xC844, 0x0103, 2},	//Core Skip X (A) = 259
{0xC846, 0x0103, 2},	//Core Skip Y (A) = 259
{0xC848, 0x0103, 2},	//Pipe Skip X (A) = 259
{0xC84A, 0x0103, 2},	//Pipe Skip Y (A) = 259
{0xC84C, 0x00F6, 2},	//Power Mode (A) = 246
{0xC84E, 0x0001, 2},	//Bin Mode (A) = 1
{0xC850, 0x03,   1},	//Orientation (A) = 0

{0xC851, 0x00,   1},	//Pixel Order (A) = 0
{0xC852, 0x019C, 2},	//Fine Correction (A) = 412
{0xC854, 0x0732, 2},	//Fine IT Min (A) = 1842
{0xC856, 0x048E, 2},	//Fine IT Max Margin (A) = 1166
{0xC858, 0x0002, 2},	//Coarse IT Min (A) = 2
{0xC85A, 0x0001, 2},	//Coarse IT Max Margin (A) = 1
{0xC85C, 0x0423, 2},	//Min Frame Lines (A) = 1059
{0xC85E, 0xFFFF, 2},	//Max Frame Lines (A) = 65535
{0xC860, 0x0423, 2},	//Base Frame Lines (A) = 1059
{0xC862, 0x0DBB, 2},	//Min Line Length (A) = 3515
{0xC864, 0xFFFE, 2},	//Max Line Length (A) = 65534
{0xC866, 0x7F7E, 2},	//P456 Divider (A) = 32638
{0xC868, 0x0423, 2},	//Frame Lines (A) = 1059
{0xC86A, 0x0DBB, 2},	//Line Length (A) = 3515
{0xC870, 0x0014, 2},	//RX FIFO Watermark (A) = 20
{0xC8AA, 0x0500	, 2}, //500 // CAM_OUTPUT_0_IMAGE_WIDTH
{0xC8AC, 0x03C0	, 2},//3C0 // CAM_OUTPUT_0_IMAGE_HEIGHT
{0xC8AE, 0x0001, 2},	//Output_0 Image Format = 1
{0xC8B0, 0x0000, 2},	//Output_0 Format Order = 0
{0xC8B8, 0x0004, 2},	//Output_0 JPEG control = 4
{0xC8A4, 0x0A28, 2},	//Output Width (B) = 2600
{0xC8A6, 0x07A0, 2},	//Output Height (B) = 1952
{0xC872, 0x0010, 2},	//Row Start (B) = 16
{0xC874, 0x001C, 2},	//Column Start (B) = 28
{0xC876, 0x07AF, 2},	//Row End (B) = 1967
{0xC878, 0x0A43, 2},	//Column End (B) = 2627
{0xC87A, 0x0001, 2},	//Row Speed (B) = 1
{0xC87C, 0x0101, 2},	//Core Skip X (B) = 257
{0xC87E, 0x0101, 2},	//Core Skip Y (B) = 257
{0xC880, 0x0101, 2},	//Pipe Skip X (B) = 257
{0xC882, 0x0101, 2},	//Pipe Skip Y (B) = 257
{0xC884, 0x00F2, 2},	//Power Mode (B) = 242
{0xC886, 0x0000, 2},	//Bin Mode (B) = 0
{0xC888, 0x03,   1},	//Orientation (B) = 0


{0xC889, 0x00,   1},	//Pixel Order (B) = 0
{0xC88A, 0x009C, 2},	//Fine Correction (B) = 156
{0xC88C, 0x034A, 2},	//Fine IT Min (B) = 842
{0xC88E, 0x02A6, 2},	//Fine IT Max Margin (B) = 678
{0xC890, 0x0002, 2},	//Coarse IT Min (B) = 2
{0xC892, 0x0001, 2},	//Coarse IT Max Margin (B) = 1
{0xC894, 0x07EF, 2},	//Min Frame Lines (B) = 2031
{0xC896, 0xFFFF, 2},	//Max Frame Lines (B) = 65535
{0xC898, 0x07EF, 2},	//Base Frame Lines (B) = 2031
{0xC89A, 0x1B31, 2},	//Min Line Length (B) = 6961
{0xC89C, 0xFFFE, 2},	//Max Line Length (B) = 65534
{0xC89E, 0x7F7F, 2},	//P456 Divider (B) = 32638		//0x7F7E
{0xC8A0, 0x07EF, 2},	//Frame Lines (B) = 2031
{0xC8A2, 0x1B31, 2},	//Line Length (B) = 6961
{0xC8A8, 0x0014, 2},	//RX FIFO Watermark (B) = 20
{0xC8C0, 0x0A20, 2},	//Output_1 Image Width = 2592
{0xC8C2, 0x0798, 2},	//Output_1 Image Height = 1944
{0xC8C4, 0x0001, 2},	//Output_1 Image Format = 1
{0xC8C6, 0x0000, 2},	//Output_1 Format Order = 0
{0xC8CE, 0x0005, 2},	//Output_1 JPEG control = 5
{0xA010, 0x00F9, 2},	//fd_min_expected50hz_flicker_period = 249
{0xA012, 0x010D, 2},	//fd_max_expected50hz_flicker_period = 269
{0xA014, 0x00CE, 2},	//fd_min_expected60hz_flicker_period = 206
{0xA016, 0x00E2, 2},	//fd_max_expected60hz_flicker_period = 226
{0xA018, 0x0103, 2},	//fd_expected50hz_flicker_period (A) = 259
{0xA01A, 0x0083, 2},	//fd_expected50hz_flicker_period (B) = 131
{0xA01C, 0x00D8, 2},	//fd_expected60hz_flicker_period (A) = 216
{0xA01E, 0x006D, 2},	//fd_expected60hz_flicker_period (B) = 109
{0xDC0A, 0x06,   1},	//Scaler Allow Zoom Ratio = 6
{0xDC1C, 0x2710, 2},	//System Zoom Ratio = 10000   

{0x098E, 0x48C4, 2},
{0xC8C4, 0x0001, 2},
{0xC8C4, 0x0001, 2},
{0xC8C6, 0x0000, 2},
{0xC8CE, 0x0004, 2},
{0xC8CE, 0x0004, 2},
{0xC8CE, 0x0004, 2},
{0xD822, 0x4710, 2},
{0x3330, 0x0000, 2},
{0x8404, 0x06,   1},
{0,      100,    0},

/*Flicker setting Edison 20110721*/
{0x098E, 0x2018, 2}, 	// LOGICAL_ADDRESS_ACCESS [FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_A]
{0xA018, 0x0104, 2}, 	// FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_A
{0xA01A, 0x007B, 2}, 	// FD_EXPECTED50HZ_FLICKER_PERIOD_IN_CONTEXT_B
{0xA01C, 0x00D7, 2}, 	// FD_EXPECTED60HZ_FLICKER_PERIOD_IN_CONTEXT_A
{0xA01E, 0x0066, 2}, 	// FD_EXPECTED60HZ_FLICKER_PERIOD_IN_CONTEXT_B
{0xA010, 0x00DF, 2}, 	// FD_MIN_EXPECTED50HZ_FLICKER_PERIOD
{0xA012, 0x00FD, 2}, 	// FD_MAX_EXPECTED50HZ_FLICKER_PERIOD
{0xA014, 0x00B6, 2}, 	// FD_MIN_EXPECTED60HZ_FLICKER_PERIOD
{0xA016, 0x00D4, 2}, 	// FD_MAX_EXPECTED60HZ_FLICKER_PERIOD

//Move the MIPI Timing registers
#endif

//LOAD = Step3-Recommended		//Patch & Char settings
//; NEW Patch_AF
//  k28a_rev03_patch14_CR31491_Pantech_CAF_OSD_REV2  //jolland_0105_2011
{0x0982, 0x0000 , 2},
{0x098A, 0x0000 , 2},
{0x886C, 0xC0F1 , 2},
{0x886E, 0xC5E1 , 2},
{0x8870, 0x246A , 2},
{0x8872, 0x1280 , 2},
{0x8874, 0xC4E1 , 2},
{0x8876, 0xD20F , 2},
{0x8878, 0x2069 , 2},
{0x887A, 0x0000 , 2},
{0x887C, 0x6A62 , 2},
{0x887E, 0x1303 , 2},
{0x8880, 0x0084 , 2},
{0x8882, 0x1734 , 2},
{0x8884, 0x7005 , 2},
{0x8886, 0xD801 , 2},
{0x8888, 0x8A41 , 2},
{0x888A, 0xD900 , 2},
{0x888C, 0x0D5A , 2},
{0x888E, 0x0664 , 2},
{0x8890, 0x8B61 , 2},
{0x8892, 0xE80B , 2},
{0x8894, 0x000D , 2},
{0x8896, 0x0020 , 2},
{0x8898, 0xD508 , 2},
{0x889A, 0x1504 , 2},
{0x889C, 0x1400 , 2},
{0x889E, 0x7840 , 2},
{0x88A0, 0xD007 , 2},
{0x88A2, 0x0DFB , 2},
{0x88A4, 0x9004 , 2},
{0x88A6, 0xC4C1 , 2},
{0x88A8, 0x2029 , 2},
{0x88AA, 0x0300 , 2},
{0x88AC, 0x0219 , 2},
{0x88AE, 0x06C4 , 2},
{0x88B0, 0xFF80 , 2},
{0x88B2, 0x08CC , 2},
{0x88B4, 0xFF80 , 2},
{0x88B6, 0x086C , 2},
{0x88B8, 0xFF80 , 2},
{0x88BA, 0x08C0 , 2},
{0x88BC, 0xFF80 , 2},
{0x88BE, 0x08CC , 2},
{0x88C0, 0xFF80 , 2},
{0x88C2, 0x0CA8 , 2},
{0x88C4, 0xFF80 , 2},
{0x88C6, 0x0D80 , 2},
{0x88C8, 0xFF80 , 2},
{0x88CA, 0x0DA0 , 2},
{0x88CC, 0x000E , 2},
{0x88CE, 0x0002 , 2},
{0x88D0, 0x0000 , 2},
{0x88D2, 0x0000 , 2},
{0x88D4, 0xD2FC , 2},
{0x88D6, 0xD0FD , 2},
{0x88D8, 0x122A , 2},
{0x88DA, 0x0901 , 2},
{0x88DC, 0x900B , 2},
{0x88DE, 0x792F , 2},
{0x88E0, 0xB808 , 2},
{0x88E2, 0x2004 , 2},
{0x88E4, 0x0F80 , 2},
{0x88E6, 0x0000 , 2},
{0x88E8, 0xFF00 , 2},
{0x88EA, 0x7825 , 2},
{0x88EC, 0x1A2A , 2},
{0x88EE, 0x0024 , 2},
{0x88F0, 0x0729 , 2},
{0x88F2, 0x0504 , 2},
{0x88F4, 0xC0F1 , 2},
{0x88F6, 0x095E , 2},
{0x88F8, 0x06C4 , 2},
{0x88FA, 0xD6F5 , 2},
{0x88FC, 0x8E01 , 2},
{0x88FE, 0xB8A4 , 2},
{0x8900, 0xAE01 , 2},
{0x8902, 0x8E09 , 2},
{0x8904, 0xB8E0 , 2},
{0x8906, 0xF29B , 2},
{0x8908, 0xD5F2 , 2},
{0x890A, 0x153A , 2},
{0x890C, 0x1080 , 2},
{0x890E, 0x153B , 2},
{0x8910, 0x1081 , 2},
{0x8912, 0xB808 , 2},
{0x8914, 0x7825 , 2},
{0x8916, 0x16B8 , 2},
{0x8918, 0x1101 , 2},
{0x891A, 0x092D , 2},
{0x891C, 0x0003 , 2},
{0x891E, 0x16B0 , 2},
{0x8920, 0x1082 , 2},
{0x8922, 0x1E3C , 2},
{0x8924, 0x1082 , 2},
{0x8926, 0x16B1 , 2},
{0x8928, 0x1082 , 2},
{0x892A, 0x1E3D , 2},
{0x892C, 0x1082 , 2},
{0x892E, 0x16B4 , 2},
{0x8930, 0x1082 , 2},
{0x8932, 0x1E3E , 2},
{0x8934, 0x1082 , 2},
{0x8936, 0x16B5 , 2},
{0x8938, 0x1082 , 2},
{0x893A, 0x1E3F , 2},
{0x893C, 0x1082 , 2},
{0x893E, 0x8E40 , 2},
{0x8940, 0xBAA6 , 2},
{0x8942, 0xAE40 , 2},
{0x8944, 0x098F , 2},
{0x8946, 0x0022 , 2},
{0x8948, 0x16BA , 2},
{0x894A, 0x1102 , 2},
{0x894C, 0x0A87 , 2},
{0x894E, 0x0003 , 2},
{0x8950, 0x16B2 , 2},
{0x8952, 0x1084 , 2},
{0x8954, 0x0A92 , 2},
{0x8956, 0x06A4 , 2},
{0x8958, 0x16B0 , 2},
{0x895A, 0x1083 , 2},
{0x895C, 0x1E3C , 2},
{0x895E, 0x1002 , 2},
{0x8960, 0x153A , 2},
{0x8962, 0x1080 , 2},
{0x8964, 0x153B , 2},
{0x8966, 0x1081 , 2},
{0x8968, 0x16B3 , 2},
{0x896A, 0x1084 , 2},
{0x896C, 0xB808 , 2},
{0x896E, 0x7825 , 2},
{0x8970, 0x16B8 , 2},
{0x8972, 0x1101 , 2},
{0x8974, 0x16BA , 2},
{0x8976, 0x1102 , 2},
{0x8978, 0x0A6E , 2},
{0x897A, 0x06A4 , 2},
{0x897C, 0x16B1 , 2},
{0x897E, 0x1083 , 2},
{0x8980, 0x1E3D , 2},
{0x8982, 0x1002 , 2},
{0x8984, 0x153A , 2},
{0x8986, 0x1080 , 2},
{0x8988, 0x153B , 2},
{0x898A, 0x1081 , 2},
{0x898C, 0x16B6 , 2},
{0x898E, 0x1084 , 2},
{0x8990, 0xB808 , 2},
{0x8992, 0x7825 , 2},
{0x8994, 0x16B8 , 2},
{0x8996, 0x1101 , 2},
{0x8998, 0x16BA , 2},
{0x899A, 0x1102 , 2},
{0x899C, 0x0A4A , 2},
{0x899E, 0x06A4 , 2},
{0x89A0, 0x16B4 , 2},
{0x89A2, 0x1083 , 2},
{0x89A4, 0x1E3E , 2},
{0x89A6, 0x1002 , 2},
{0x89A8, 0x153A , 2},
{0x89AA, 0x1080 , 2},
{0x89AC, 0x153B , 2},
{0x89AE, 0x1081 , 2},
{0x89B0, 0x16B7 , 2},
{0x89B2, 0x1084 , 2},
{0x89B4, 0xB808 , 2},
{0x89B6, 0x7825 , 2},
{0x89B8, 0x16B8 , 2},
{0x89BA, 0x1101 , 2},
{0x89BC, 0x16BA , 2},
{0x89BE, 0x1102 , 2},
{0x89C0, 0x0A26 , 2},
{0x89C2, 0x06A4 , 2},
{0x89C4, 0x16B5 , 2},
{0x89C6, 0x1083 , 2},
{0x89C8, 0x1E3F , 2},
{0x89CA, 0x1002 , 2},
{0x89CC, 0x8E00 , 2},
{0x89CE, 0xB8A6 , 2},
{0x89D0, 0xAE00 , 2},
{0x89D2, 0x153A , 2},
{0x89D4, 0x1081 , 2},
{0x89D6, 0x153B , 2},
{0x89D8, 0x1080 , 2},
{0x89DA, 0xB908 , 2},
{0x89DC, 0x7905 , 2},
{0x89DE, 0x16BA , 2},
{0x89E0, 0x1100 , 2},
{0x89E2, 0x085B , 2},
{0x89E4, 0x0042 , 2},
{0x89E6, 0xD0BC , 2},
{0x89E8, 0x9E31 , 2},
{0x89EA, 0x904D , 2},
{0x89EC, 0x0A2B , 2},
{0x89EE, 0x0063 , 2},
{0x89F0, 0x8E00 , 2},
{0x89F2, 0x16B0 , 2},
{0x89F4, 0x1081 , 2},
{0x89F6, 0x1E3C , 2},
{0x89F8, 0x1042 , 2},
{0x89FA, 0x16B1 , 2},
{0x89FC, 0x1081 , 2},
{0x89FE, 0x1E3D , 2},
{0x8A00, 0x1042 , 2},
{0x8A02, 0x16B4 , 2},
{0x8A04, 0x1081 , 2},
{0x8A06, 0x1E3E , 2},
{0x8A08, 0x1042 , 2},
{0x8A0A, 0x16B5 , 2},
{0x8A0C, 0x1081 , 2},
{0x8A0E, 0x1E3F , 2},
{0x8A10, 0x1042 , 2},
{0x8A12, 0xB886 , 2},
{0x8A14, 0xF012 , 2},
{0x8A16, 0x16B2 , 2},
{0x8A18, 0x1081 , 2},
{0x8A1A, 0xB8A6 , 2},
{0x8A1C, 0x1E3C , 2},
{0x8A1E, 0x1042 , 2},
{0x8A20, 0x16B3 , 2},
{0x8A22, 0x1081 , 2},
{0x8A24, 0x1E3D , 2},
{0x8A26, 0x1042 , 2},
{0x8A28, 0x16B6 , 2},
{0x8A2A, 0x1081 , 2},
{0x8A2C, 0x1E3E , 2},
{0x8A2E, 0x1042 , 2},
{0x8A30, 0x16B7 , 2},
{0x8A32, 0x1081 , 2},
{0x8A34, 0x1E3F , 2},
{0x8A36, 0x1042 , 2},
{0x8A38, 0xAE00 , 2},
{0x8A3A, 0x08F6 , 2},
{0x8A3C, 0x01C4 , 2},
{0x8A3E, 0x0081 , 2},
{0x8A40, 0x06C4 , 2},
{0x8A42, 0x78E0 , 2},
{0x8A44, 0xC0F1 , 2},
{0x8A46, 0x080E , 2},
{0x8A48, 0x06E4 , 2},
{0x8A4A, 0xDB03 , 2},
{0x8A4C, 0xD2A3 , 2},
{0x8A4E, 0x8A2E , 2},
{0x8A50, 0x8ACF , 2},
{0x8A52, 0xB908 , 2},
{0x8A54, 0x79C5 , 2},
{0x8A56, 0xDD65 , 2},
{0x8A58, 0x094F , 2},
{0x8A5A, 0x00D1 , 2},
{0x8A5C, 0xD90A , 2},
{0x8A5E, 0x1A24 , 2},
{0x8A60, 0x0042 , 2},
{0x8A62, 0x8A24 , 2},
{0x8A64, 0xE1E5 , 2},
{0x8A66, 0xF6C9 , 2},
{0x8A68, 0xD902 , 2},
{0x8A6A, 0x2941 , 2},
{0x8A6C, 0x0200 , 2},
{0x8A6E, 0xAA0E , 2},
{0x8A70, 0xAA2F , 2},
{0x8A72, 0x70A9 , 2},
{0x8A74, 0xF014 , 2},
{0x8A76, 0xE1C8 , 2},
{0x8A78, 0x0036 , 2},
{0x8A7A, 0x000B , 2},
{0x8A7C, 0xE0C8 , 2},
{0x8A7E, 0x003A , 2},
{0x8A80, 0x000A , 2},
{0x8A82, 0xD901 , 2},
{0x8A84, 0x2941 , 2},
{0x8A86, 0x0200 , 2},
{0x8A88, 0xAA0E , 2},
{0x8A8A, 0xAA2F , 2},
{0x8A8C, 0xD848 , 2},
{0x8A8E, 0xF008 , 2},
{0x8A90, 0xD900 , 2},
{0x8A92, 0x2941 , 2},
{0x8A94, 0x0200 , 2},
{0x8A96, 0xAA0E , 2},
{0x8A98, 0xAA2F , 2},
{0x8A9A, 0xD820 , 2},
{0x8A9C, 0xD290 , 2},
{0x8A9E, 0x8A26 , 2},
{0x8AA0, 0xB961 , 2},
{0x8AA2, 0xAA26 , 2},
{0x8AA4, 0xF00D , 2},
{0x8AA6, 0x091F , 2},
{0x8AA8, 0x0091 , 2},
{0x8AAA, 0x8A24 , 2},
{0x8AAC, 0xF1E5 , 2},
{0x8AAE, 0x0913 , 2},
{0x8AB0, 0x0812 , 2},
{0x8AB2, 0x08E1 , 2},
{0x8AB4, 0x8812 , 2},
{0x8AB6, 0x2B41 , 2},
{0x8AB8, 0x0201 , 2},
{0x8ABA, 0xAA2E , 2},
{0x8ABC, 0xAA6F , 2},
{0x8ABE, 0x0001 , 2},
{0x8AC0, 0x06C4 , 2},
{0x8AC2, 0x09F7 , 2},
{0x8AC4, 0x8051 , 2},
{0x8AC6, 0x8A24 , 2},
{0x8AC8, 0xF1F3 , 2},
{0x8ACA, 0x78E0 , 2},
{0x8ACC, 0xC0F1 , 2},
{0x8ACE, 0x0F7A , 2},
{0x8AD0, 0x0684 , 2},
{0x8AD2, 0xD682 , 2},
{0x8AD4, 0x7508 , 2},
{0x8AD6, 0x8E01 , 2},
{0x8AD8, 0xD181 , 2},
{0x8ADA, 0x2046 , 2},
{0x8ADC, 0x00C0 , 2},
{0x8ADE, 0xAE01 , 2},
{0x8AE0, 0x1145 , 2},
{0x8AE2, 0x0080 , 2},
{0x8AE4, 0x1146 , 2},
{0x8AE6, 0x0082 , 2},
{0x8AE8, 0xB808 , 2},
{0x8AEA, 0x7845 , 2},
{0x8AEC, 0x0817 , 2},
{0x8AEE, 0x001E , 2},
{0x8AF0, 0x8900 , 2},
{0x8AF2, 0x8941 , 2},
{0x8AF4, 0xB808 , 2},
{0x8AF6, 0x7845 , 2},
{0x8AF8, 0x080B , 2},
{0x8AFA, 0x00DE , 2},
{0x8AFC, 0x70A9 , 2},
{0x8AFE, 0xFFD2 , 2},
{0x8B00, 0x7508 , 2},
{0x8B02, 0x1604 , 2},
{0x8B04, 0x1090 , 2},
{0x8B06, 0x0D93 , 2},
{0x8B08, 0x1400 , 2},
{0x8B0A, 0x8EEA , 2},
{0x8B0C, 0x8E0B , 2},
{0x8B0E, 0x214A , 2},
{0x8B10, 0x2040 , 2},
{0x8B12, 0x8E2D , 2},
{0x8B14, 0xBF08 , 2},
{0x8B16, 0x7F05 , 2},
{0x8B18, 0x8E0C , 2},
{0x8B1A, 0xB808 , 2},
{0x8B1C, 0x7825 , 2},
{0x8B1E, 0x7710 , 2},
{0x8B20, 0x21C2 , 2},
{0x8B22, 0x244C , 2},
{0x8B24, 0x081D , 2},
{0x8B26, 0x03E3 , 2},
{0x8B28, 0xD9FF , 2},
{0x8B2A, 0x2702 , 2},
{0x8B2C, 0x1002 , 2},
{0x8B2E, 0x2A05 , 2},
{0x8B30, 0x037E , 2},
{0x8B32, 0x0FF6 , 2},
{0x8B34, 0x06A4 , 2},
{0x8B36, 0x702F , 2},
{0x8B38, 0x7810 , 2},
{0x8B3A, 0x7F02 , 2},
{0x8B3C, 0x7FF0 , 2},
{0x8B3E, 0xF00B , 2},
{0x8B40, 0x78E2 , 2},
{0x8B42, 0x2805 , 2},
{0x8B44, 0x037E , 2},
{0x8B46, 0x0FE2 , 2},
{0x8B48, 0x06A4 , 2},
{0x8B4A, 0x702F , 2},
{0x8B4C, 0x7810 , 2},
{0x8B4E, 0x671F , 2},
{0x8B50, 0x7FF0 , 2},
{0x8B52, 0x7FEF , 2},
{0x8B54, 0x8E08 , 2},
{0x8B56, 0xBF06 , 2},
{0x8B58, 0xD162 , 2},
{0x8B5A, 0xB8C3 , 2},
{0x8B5C, 0x78E5 , 2},
{0x8B5E, 0xB88F , 2},
{0x8B60, 0x1908 , 2},
{0x8B62, 0x0024 , 2},
{0x8B64, 0x2841 , 2},
{0x8B66, 0x0201 , 2},
{0x8B68, 0x1E26 , 2},
{0x8B6A, 0x1042 , 2},
{0x8B6C, 0x0D15 , 2},
{0x8B6E, 0x1423 , 2},
{0x8B70, 0x1E27 , 2},
{0x8B72, 0x1002 , 2},
{0x8B74, 0x214C , 2},
{0x8B76, 0xA000 , 2},
{0x8B78, 0x214A , 2},
{0x8B7A, 0x2040 , 2},
{0x8B7C, 0x21C2 , 2},
{0x8B7E, 0x2442 , 2},
{0x8B80, 0x8E21 , 2},
{0x8B82, 0x214F , 2},
{0x8B84, 0x0040 , 2},
{0x8B86, 0x090F , 2},
{0x8B88, 0x2010 , 2},
{0x8B8A, 0x2145 , 2},
{0x8B8C, 0x0181 , 2},
{0x8B8E, 0xAE21 , 2},
{0x8B90, 0xF003 , 2},
{0x8B92, 0xB8A2 , 2},
{0x8B94, 0xAE01 , 2},
{0x8B96, 0x0B7A , 2},
{0x8B98, 0xFFE3 , 2},
{0x8B9A, 0x70A9 , 2},
{0x8B9C, 0x0709 , 2},
{0x8B9E, 0x0684 , 2},
{0x8BA0, 0xC0F1 , 2},
{0x8BA2, 0xC5E1 , 2},
{0x8BA4, 0xD54E , 2},
{0x8BA6, 0x8D24 , 2},
{0x8BA8, 0x8D45 , 2},
{0x8BAA, 0xB908 , 2},
{0x8BAC, 0x7945 , 2},
{0x8BAE, 0x0941 , 2},
{0x8BB0, 0x011E , 2},
{0x8BB2, 0x8D26 , 2},
{0x8BB4, 0x0939 , 2},
{0x8BB6, 0x0093 , 2},
{0x8BB8, 0xD148 , 2},
{0x8BBA, 0xA907 , 2},
{0x8BBC, 0xD04A , 2},
{0x8BBE, 0x802E , 2},
{0x8BC0, 0x9117 , 2},
{0x8BC2, 0x0F6E , 2},
{0x8BC4, 0x06A4 , 2},
{0x8BC6, 0x912E , 2},
{0x8BC8, 0x790F , 2},
{0x8BCA, 0x0911 , 2},
{0x8BCC, 0x00B2 , 2},
{0x8BCE, 0x1541 , 2},
{0x8BD0, 0x1080 , 2},
{0x8BD2, 0x0F5E , 2},
{0x8BD4, 0x0684 , 2},
{0x8BD6, 0x780F , 2},
{0x8BD8, 0x2840 , 2},
{0x8BDA, 0x0201 , 2},
{0x8BDC, 0x7825 , 2},
{0x8BDE, 0x2841 , 2},
{0x8BE0, 0x0201 , 2},
{0x8BE2, 0x1D42 , 2},
{0x8BE4, 0x1042 , 2},
{0x8BE6, 0x1D43 , 2},
{0x8BE8, 0x1002 , 2},
{0x8BEA, 0xF003 , 2},
{0x8BEC, 0xFFB8 , 2},
{0x8BEE, 0x06D9 , 2},
{0x8BF0, 0x0684 , 2},
{0x8BF2, 0x78E0 , 2},
{0x8BF4, 0xC0F1 , 2},
{0x8BF6, 0x0E5E , 2},
{0x8BF8, 0x0684 , 2},
{0x8BFA, 0xD538 , 2},
{0x8BFC, 0x8D00 , 2},
{0x8BFE, 0x0841 , 2},
{0x8C00, 0x01DE , 2},
{0x8C02, 0xB8A7 , 2},
{0x8C04, 0x790F , 2},
{0x8C06, 0xD639 , 2},
{0x8C08, 0xAD00 , 2},
{0x8C0A, 0x091F , 2},
{0x8C0C, 0x0050 , 2},
{0x8C0E, 0x0921 , 2},
{0x8C10, 0x0110 , 2},
{0x8C12, 0x0911 , 2},
{0x8C14, 0x0210 , 2},
{0x8C16, 0xD036 , 2},
{0x8C18, 0x0A5A , 2},
{0x8C1A, 0xFFE3 , 2},
{0x8C1C, 0xA600 , 2},
{0x8C1E, 0xF00A , 2},
{0x8C20, 0x000F , 2},
{0x8C22, 0x0020 , 2},
{0x8C24, 0xD033 , 2},
{0x8C26, 0x000B , 2},
{0x8C28, 0x0020 , 2},
{0x8C2A, 0xD033 , 2},
{0x8C2C, 0xD033 , 2},
{0x8C2E, 0xA600 , 2},
{0x8C30, 0x8600 , 2},
{0x8C32, 0x8023 , 2},
{0x8C34, 0x7960 , 2},
{0x8C36, 0xD801 , 2},
{0x8C38, 0xD800 , 2},
{0x8C3A, 0xAD05 , 2},
{0x8C3C, 0x1528 , 2},
{0x8C3E, 0x1080 , 2},
{0x8C40, 0x0817 , 2},
{0x8C42, 0x01DE , 2},
{0x8C44, 0xB8A7 , 2},
{0x8C46, 0x1D28 , 2},
{0x8C48, 0x1002 , 2},
{0x8C4A, 0xD028 , 2},
{0x8C4C, 0x8000 , 2},
{0x8C4E, 0x8023 , 2},
{0x8C50, 0x7960 , 2},
{0x8C52, 0x1528 , 2},
{0x8C54, 0x1080 , 2},
{0x8C56, 0x0669 , 2},
{0x8C58, 0x0684 , 2},
{0x8C5A, 0x78E0 , 2},
{0x8C5C, 0xD21F , 2},
{0x8C5E, 0x8A21 , 2},
{0x8C60, 0xB9A1 , 2},
{0x8C62, 0x782F , 2},
{0x8C64, 0x7FE0 , 2},
{0x8C66, 0xAA21 , 2},
{0x8C68, 0xC0F1 , 2},
{0x8C6A, 0xD125 , 2},
{0x8C6C, 0x8906 , 2},
{0x8C6E, 0x8947 , 2},
{0x8C70, 0xB808 , 2},
{0x8C72, 0x7845 , 2},
{0x8C74, 0x262F , 2},
{0x8C76, 0xF007 , 2},
{0x8C78, 0xF406 , 2},
{0x8C7A, 0xD022 , 2},
{0x8C7C, 0x8001 , 2},
{0x8C7E, 0x7840 , 2},
{0x8C80, 0xC0D1 , 2},
{0x8C82, 0x7EE0 , 2},
{0x8C84, 0xB861 , 2},
{0x8C86, 0x7810 , 2},
{0x8C88, 0x2841 , 2},
{0x8C8A, 0x020C , 2},
{0x8C8C, 0xA986 , 2},
{0x8C8E, 0xA907 , 2},
{0x8C90, 0x780F , 2},
{0x8C92, 0x0815 , 2},
{0x8C94, 0x0051 , 2},
{0x8C96, 0xD015 , 2},
{0x8C98, 0x8000 , 2},
{0x8C9A, 0xD210 , 2},
{0x8C9C, 0x8021 , 2},
{0x8C9E, 0x7960 , 2},
{0x8CA0, 0x8A07 , 2},
{0x8CA2, 0xF1F0 , 2},
{0x8CA4, 0xF1EE , 2},
{0x8CA6, 0x78E0 , 2},
{0x8CA8, 0xC0F1 , 2},
{0x8CAA, 0x0DAA , 2},
{0x8CAC, 0x06A4 , 2},
{0x8CAE, 0xDA44 , 2},
{0x8CB0, 0xD115 , 2},
{0x8CB2, 0xD516 , 2},
{0x8CB4, 0x76A9 , 2},
{0x8CB6, 0x0B3E , 2},
{0x8CB8, 0x06A4 , 2},
{0x8CBA, 0x70C9 , 2},
{0x8CBC, 0xD014 , 2},
{0x8CBE, 0xD900 , 2},
{0x8CC0, 0xF028 , 2},
{0x8CC2, 0x78E0 , 2},
{0x8CC4, 0xFF00 , 2},
{0x8CC6, 0x3354 , 2},
{0x8CC8, 0xFF80 , 2},
{0x8CCA, 0x0694 , 2},
{0x8CCC, 0xFF80 , 2},
{0x8CCE, 0x0314 , 2},
{0x8CD0, 0xFF80 , 2},
{0x8CD2, 0x0250 , 2},
{0x8CD4, 0xFF80 , 2},
{0x8CD6, 0x050C , 2},
{0x8CD8, 0xFF80 , 2},
{0x8CDA, 0x0158 , 2},
{0x8CDC, 0xFF80 , 2},
{0x8CDE, 0x0290 , 2},
{0x8CE0, 0xFF00 , 2},
{0x8CE2, 0x0618 , 2},
{0x8CE4, 0xFF80 , 2},
{0x8CE6, 0x06C8 , 2},
{0x8CE8, 0x8000 , 2},
{0x8CEA, 0x0008 , 2},
{0x8CEC, 0x0000 , 2},
{0x8CEE, 0xF1A4 , 2},
{0x8CF0, 0xFF80 , 2},
{0x8CF2, 0x0EC0 , 2},
{0x8CF4, 0x0000 , 2},
{0x8CF6, 0xF1B4 , 2},
{0x8CF8, 0x0000 , 2},
{0x8CFA, 0xF1C4 , 2},
{0x8CFC, 0xFF80 , 2},
{0x8CFE, 0x02CC , 2},
{0x8D00, 0xFF80 , 2},
{0x8D02, 0x0E48 , 2},
{0x8D04, 0x0000 , 2},
{0x8D06, 0xF9AC , 2},
{0x8D08, 0xFF80 , 2},
{0x8D0A, 0x0E50 , 2},
{0x8D0C, 0xFF80 , 2},
{0x8D0E, 0x08D4 , 2},
{0x8D10, 0xA502 , 2},
{0x8D12, 0xD032 , 2},
{0x8D14, 0xA0C0 , 2},
{0x8D16, 0x17B4 , 2},
{0x8D18, 0xF000 , 2},
{0x8D1A, 0xB02B , 2},
{0x8D1C, 0x17B0 , 2},
{0x8D1E, 0xF001 , 2},
{0x8D20, 0x8900 , 2},
{0x8D22, 0xDB08 , 2},
{0x8D24, 0xDAF0 , 2},
{0x8D26, 0x19B0 , 2},
{0x8D28, 0x00C2 , 2},
{0x8D2A, 0xB8A6 , 2},
{0x8D2C, 0xA900 , 2},
{0x8D2E, 0xD851 , 2},
{0x8D30, 0x19B2 , 2},
{0x8D32, 0x0002 , 2},
{0x8D34, 0xD852 , 2},
{0x8D36, 0x19B3 , 2},
{0x8D38, 0x0002 , 2},
{0x8D3A, 0xD855 , 2},
{0x8D3C, 0x19B6 , 2},
{0x8D3E, 0x0002 , 2},
{0x8D40, 0xD856 , 2},
{0x8D42, 0x19B7 , 2},
{0x8D44, 0x0002 , 2},
{0x8D46, 0xD896 , 2},
{0x8D48, 0x19B8 , 2},
{0x8D4A, 0x0004 , 2},
{0x8D4C, 0xD814 , 2},
{0x8D4E, 0x19BA , 2},
{0x8D50, 0x0004 , 2},
{0x8D52, 0xD805 , 2},
{0x8D54, 0xB111 , 2},
{0x8D56, 0x19B1 , 2},
{0x8D58, 0x0082 , 2},
{0x8D5A, 0x19B4 , 2},
{0x8D5C, 0x00C2 , 2},
{0x8D5E, 0x19B5 , 2},
{0x8D60, 0x0082 , 2},
{0x8D62, 0xD11F , 2},
{0x8D64, 0x2555 , 2},
{0x8D66, 0x1440 , 2},
{0x8D68, 0x0A8A , 2},
{0x8D6A, 0x06A4 , 2},
{0x8D6C, 0xDA2C , 2},
{0x8D6E, 0xD01D , 2},
{0x8D70, 0x2555 , 2},
{0x8D72, 0x1441 , 2},
{0x8D74, 0xA514 , 2},
{0x8D76, 0xD01C , 2},
{0x8D78, 0x0545 , 2},
{0x8D7A, 0x06A4 , 2},
{0x8D7C, 0xA020 , 2},
{0x8D7E, 0x78E0 , 2},
{0x8D80, 0xD01A , 2},
{0x8D82, 0x1788 , 2},
{0x8D84, 0xF001 , 2},
{0x8D86, 0xA11C , 2},
{0x8D88, 0xD019 , 2},
{0x8D8A, 0xA11D , 2},
{0x8D8C, 0xD019 , 2},
{0x8D8E, 0xA11E , 2},
{0x8D90, 0xD019 , 2},
{0x8D92, 0xA11F , 2},
{0x8D94, 0x1754 , 2},
{0x8D96, 0xF000 , 2},
{0x8D98, 0xE170 , 2},
{0x8D9A, 0x7FE0 , 2},
{0x8D9C, 0xA020 , 2},
{0x8D9E, 0x78E0 , 2},
{0x8DA0, 0xC0F1 , 2},
{0x8DA2, 0xC5E1 , 2},
{0x8DA4, 0x1764 , 2},
{0x8DA6, 0xF00D , 2},
{0x8DA8, 0xD114 , 2},
{0x8DAA, 0x2556 , 2},
{0x8DAC, 0x1400 , 2},
{0x8DAE, 0x0A46 , 2},
{0x8DB0, 0x06A4 , 2},
{0x8DB2, 0xDA38 , 2},
{0x8DB4, 0x174C , 2},
{0x8DB6, 0xF000 , 2},
{0x8DB8, 0xD111 , 2},
{0x8DBA, 0xA021 , 2},
{0x8DBC, 0xD011 , 2},
{0x8DBE, 0x2556 , 2},
{0x8DC0, 0x1401 , 2},
{0x8DC2, 0x1D94 , 2},
{0x8DC4, 0x1000 , 2},
{0x8DC6, 0xD010 , 2},
{0x8DC8, 0xA020 , 2},
{0x8DCA, 0x171C , 2},
{0x8DCC, 0xF000 , 2},
{0x8DCE, 0x802E , 2},
{0x8DD0, 0x9117 , 2},
{0x8DD2, 0x04F5 , 2},
{0x8DD4, 0x06A4 , 2},
{0x8DD6, 0xB10E , 2},
{0x8DD8, 0x8000 , 2},
{0x8DDA, 0x016C , 2},
{0x8DDC, 0x0000 , 2},
{0x8DDE, 0xF444 , 2},
{0x8DE0, 0xFF80 , 2},
{0x8DE2, 0x08F4 , 2},
{0x8DE4, 0x8000 , 2},
{0x8DE6, 0x009C , 2},
{0x8DE8, 0xFF80 , 2},
{0x8DEA, 0x0BF4 , 2},
{0x8DEC, 0xFF80 , 2},
{0x8DEE, 0x0BA0 , 2},
{0x8DF0, 0xFF80 , 2},
{0x8DF2, 0x0C5C , 2},
{0x8DF4, 0x0000 , 2},
{0x8DF6, 0x0998 , 2},
{0x8DF8, 0x0000 , 2},
{0x8DFA, 0xF3BC , 2},
{0x8DFC, 0x0000 , 2},
{0x8DFE, 0x2F2C , 2},
{0x8E00, 0xFF80 , 2},
{0x8E02, 0x0C68 , 2},
{0x8E04, 0x8000 , 2},
{0x8E06, 0x008C , 2},
{0x8E08, 0xE280 , 2},
{0x8E0A, 0x24CA , 2},
{0x8E0C, 0x7082 , 2},
{0x8E0E, 0x78E0 , 2},
{0x8E10, 0x20E8 , 2},
{0x8E12, 0x01A2 , 2},
{0x8E14, 0x1002 , 2},
{0x8E16, 0x0D02 , 2},
{0x8E18, 0x1902 , 2},
{0x8E1A, 0x0094 , 2},
{0x8E1C, 0x7FE0 , 2},
{0x8E1E, 0x7028 , 2},
{0x8E20, 0x7308 , 2},
{0x8E22, 0x1000 , 2},
{0x8E24, 0x0900 , 2},
{0x8E26, 0x7904 , 2},
{0x8E28, 0x7947 , 2},
{0x8E2A, 0x1B00 , 2},
{0x8E2C, 0x0064 , 2},
{0x8E2E, 0x7EE0 , 2},
{0x8E30, 0xE280 , 2},
{0x8E32, 0x24CA , 2},
{0x8E34, 0x7082 , 2},
{0x8E36, 0x78E0 , 2},
{0x8E38, 0x20E8 , 2},
{0x8E3A, 0x01A2 , 2},
{0x8E3C, 0x1102 , 2},
{0x8E3E, 0x0502 , 2},
{0x8E40, 0x1802 , 2},
{0x8E42, 0x00B4 , 2},
{0x8E44, 0x7FE0 , 2},
{0x8E46, 0x7028 , 2},
{0x8E48, 0x0000 , 2},
{0x8E4A, 0x0000 , 2},
{0x8E4C, 0x0000 , 2},
{0x8E4E, 0x0000 , 2},
{0x098E, 0x0016 , 2}, // LOGICAL_ADDRESS_ACCESS [MON_ADDRESS_LO]
{0x8016, 0x086C , 2}, // MON_ADDRESS_LO
{0x8002, 0x0001 , 2}, // MON_CMD
//POLL_FIELD=MON_ ,PATCH_0,==0,DELAY=10,TIMEOUT=100     // wait for the patch to complete initialization 
{0     , 100     , 0},



//Char settings
//char_settings		//tuning_reg_settings_array
//[Char_settings]
{0x3E1A, 0xA58E, 2},     // SAMP_TX_BOOST (Pixel Setting)
{0x3E2E, 0xF319, 2},     // SAMP_SPARE (Pixel Setting)
{0x3EE6, 0xA7C1, 2},     // DAC_LD_26_27 (Pixel Setting)
{0x316C, 0xB430, 2},     // DAC_TXLO (Eclipse control)
{0x316E, 0xC400, 2},     // DAC_ECL (Eclipse control)
{0x31E0, 0x0003, 2},     // PIX_DEF_ID (Eclipse control)
//[tx_latch]
{0xC8ED, 0x03,   1},       // CAM_TX_ENABLE_MODE //(Turn on tx_latch for both contexts bit 0 for preview, bit 1 for capture).

{0x060E, 0x00FF , 2},  //VGPIO OFF //20110425 //flash no funciton //wangaicui 


/*move some lines for AF to step 7*/

};

struct reg_addr_val_pair_struct mt9p111_init_settings_B_array[] = {
//LOAD = Step5-AWB_CCM			//AWB & CCM
//AWB_setup
{0x098E, 0xAC01 , 2}, // LOGICAL_ADDRESS_ACCESS [AWB_MODE]
//[tint control and saturation]
{0xAC01, 0xFF,   1}, 	// AWB_MODE
{0xAC02, 0x007F, 2}, 	// AWB_ALGO
{0xAC96, 0x01,   1}, 	// AWB_CCM_TINTING_TH
{0xAC97, 0x68,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_0
{0xAC98, 0x80,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_1
{0xAC99, 0x8F,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_2
{0xAC9A, 0x74,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_0
{0xAC9B, 0x80,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_1
{0xAC9C, 0x82,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_2
{0xAC9D, 0x80,   1}, 	// AWB_PCR_SATURATION
{0xBC56, 0xae,   1},   //0x95 	// LL_START_CCM_SATURATION
{0xBC57, 0x00,   1}, 	// LL_END_CCM_SATURATION
//[Pre AWB ratio]
{0xACB0, 0x32,   1}, 	// AWB_RG_MIN
{0xACB1, 0x4E,   1}, 	// AWB_RG_MAX
{0xACB4, 0x26,   1}, 	// AWB_BG_MIN
{0xACB5, 0x4F,   1}, 	// AWB_BG_MAX
{0xAC3C, 0x32,   1}, 	// AWB_MIN_ACCEPTED_PRE_AWB_R2G_RATIO
{0xAC3D, 0x4E,   1}, 	// AWB_MAX_ACCEPTED_PRE_AWB_R2G_RATIO
{0xAC3E, 0x26,   1}, 	// AWB_MIN_ACCEPTED_PRE_AWB_B2G_RATIO
{0xAC3F, 0x4F,   1}, 	// AWB_MAX_ACCEPTED_PRE_AWB_B2G_RATIO
//[Weight table and checker offset]
{0x3242, 0x0000, 2}, 	// AWB_WEIGHT_R0
{0x3244, 0x0000, 2}, 	// AWB_WEIGHT_R1
{0x3246, 0x0000, 2}, 	// AWB_WEIGHT_R2
{0x3248, 0x3F00, 2}, 	// AWB_WEIGHT_R3
{0x324A, 0xA500, 2}, 	// AWB_WEIGHT_R4
{0x324C, 0x1540, 2}, 	// AWB_WEIGHT_R5
{0x324E, 0x01EC, 2}, 	// AWB_WEIGHT_R6
{0x3250, 0x007E, 2}, 	// AWB_WEIGHT_R7
{0x323C, 0x0008, 2}, 	// AWB_X_SHIFT
{0x323E, 0x001F, 2}, 	// AWB_Y_SHIFT
{0xB842, 0x003A, 2}, 	// STAT_AWB_GRAY_CHECKER_OFFSET_X
{0xB844, 0x0043, 2}, 	// STAT_AWB_GRAY_CHECKER_OFFSET_Y
//[CCM]
{0xAC46, 0x01CB, 2}, 	// AWB_LEFT_CCM_0
{0xAC48, 0xFEF7, 2}, 	// AWB_LEFT_CCM_1
{0xAC4A, 0x003F, 2}, 	// AWB_LEFT_CCM_2
{0xAC4C, 0xFFB2, 2}, 	// AWB_LEFT_CCM_3
{0xAC4E, 0x0118, 2}, 	// AWB_LEFT_CCM_4
{0xAC50, 0x0036, 2}, 	// AWB_LEFT_CCM_5
{0xAC52, 0xFFE5, 2}, 	// AWB_LEFT_CCM_6
{0xAC54, 0xFEF3, 2}, 	// AWB_LEFT_CCM_7
{0xAC56, 0x0227, 2}, 	// AWB_LEFT_CCM_8
{0xAC58, 0x00B0, 2}, 	// AWB_LEFT_CCM_R2BRATIO
{0xAC5C, 0x0254, 2}, 	// AWB_RIGHT_CCM_0
{0xAC5E, 0xFED1, 2}, 	// AWB_RIGHT_CCM_1
{0xAC60, 0xFFDB, 2}, 	// AWB_RIGHT_CCM_2
{0xAC62, 0xFFC6, 2}, 	// AWB_RIGHT_CCM_3
{0xAC64, 0x0147, 2}, 	// AWB_RIGHT_CCM_4
{0xAC66, 0xFFF2, 2}, 	// AWB_RIGHT_CCM_5
{0xAC68, 0x0008, 2}, 	// AWB_RIGHT_CCM_6
{0xAC6A, 0xFF6A, 2}, 	// AWB_RIGHT_CCM_7
{0xAC6C, 0x018E, 2}, 	// AWB_RIGHT_CCM_8
{0xAC6E, 0x0076, 2}, 	// AWB_RIGHT_CCM_R2BRATIO
{0xB83E, 0x00,   1}, 	// STAT_AWB_WINDOW_POS_X
{0xB83F, 0x00,   1}, 	// STAT_AWB_WINDOW_POS_Y
{0xB840, 0xFF,   1}, 	// STAT_AWB_WINDOW_SIZE_X
{0xB841, 0xEF,   1}, 	// STAT_AWB_WINDOW_SIZE_Y
{0x8404, 0x05,   1}, 	// SEQ_CMD
//[color kill]
{0xDC36, 0x43,   1}, 	// SYS_DARK_COLOR_KILL
{0xDC02, 0x303E, 2}, 	// SYS_ALGO
{0x35A2, 0x00A3, 2}, 	// DARK_COLOR_KILL_CONTROLS

//LOAD = Step6-CPIPE_Calibration	//Color Pipe Calibration settings, if any
//jpeg_setup
{0x098E, 0xD80F , 2}, // LOGICAL_ADDRESS_ACCESS [JPEG_QSCALE_0]
{0xD80F, 0x04   , 1}, // JPEG_QSCALE_0
{0xD810, 0x08   , 1}, // JPEG_QSCALE_1
{0xC8D2, 0x04   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_0
{0xC8D3, 0x08   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_1
{0xC8BC, 0x04   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_0
{0xC8BD, 0x08   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_1

//Sys_Settings
{0x301A, 0x10F4 , 2}, // RESET_REGISTER
{0x301E, 0x0000 , 2}, // DATA_PEDESTAL
{0x301A, 0x10FC , 2}, // RESET_REGISTER
{0x098E, 0xDC33 , 2}, // LOGICAL_ADDRESS_ACCESS [SYS_FIRST_BLACK_LEVEL]
{0xDC33, 0x00   , 1}, // SYS_FIRST_BLACK_LEVEL
{0xDC35, 0x04   , 1}, // SYS_UV_COLOR_BOOST
{0x326E, 0x0006 , 2}, // LOW_PASS_YUV_FILTER
{0xDC37, 0x62   , 1}, // SYS_BRIGHT_COLORKILL
{0x35A4, 0x0596 , 2}, // BRIGHT_COLOR_KILL_CONTROLS
{0x35A2, 0x0094 , 2}, // DARK_COLOR_KILL_CONTROLS
{0xDC36, 0x23   , 1}, // SYS_DARK_COLOR_KILL

//Gamma_Curves_REV3
{0x098E, 0xBC18 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_GAMMA_CONTRAST_CURVE_0]
{0xBC18, 0x00   , 1}, // LL_GAMMA_CONTRAST_CURVE_0
{0xBC19, 0x11   , 1}, // LL_GAMMA_CONTRAST_CURVE_1
{0xBC1A, 0x23   , 1}, // LL_GAMMA_CONTRAST_CURVE_2
{0xBC1B, 0x50   , 1}, // LL_GAMMA_CONTRAST_CURVE_3
{0xBC1C, 0x7A   , 1}, // LL_GAMMA_CONTRAST_CURVE_4
{0xBC1D, 0x85   , 1}, // LL_GAMMA_CONTRAST_CURVE_5
{0xBC1E, 0x9B   , 1}, // LL_GAMMA_CONTRAST_CURVE_6
{0xBC1F, 0xAD   , 1}, // LL_GAMMA_CONTRAST_CURVE_7
{0xBC20, 0xBB   , 1}, // LL_GAMMA_CONTRAST_CURVE_8
{0xBC21, 0xC7   , 1}, // LL_GAMMA_CONTRAST_CURVE_9
{0xBC22, 0xD1   , 1}, // LL_GAMMA_CONTRAST_CURVE_10
{0xBC23, 0xDA   , 1}, // LL_GAMMA_CONTRAST_CURVE_11
{0xBC24, 0xE1   , 1}, // LL_GAMMA_CONTRAST_CURVE_12
{0xBC25, 0xE8   , 1}, // LL_GAMMA_CONTRAST_CURVE_13
{0xBC26, 0xEE   , 1}, // LL_GAMMA_CONTRAST_CURVE_14
{0xBC27, 0xF3   , 1}, // LL_GAMMA_CONTRAST_CURVE_15
{0xBC28, 0xF7   , 1}, // LL_GAMMA_CONTRAST_CURVE_16
{0xBC29, 0xFB   , 1}, // LL_GAMMA_CONTRAST_CURVE_17
{0xBC2A, 0xFF   , 1}, // LL_GAMMA_CONTRAST_CURVE_18
{0xBC2B, 0x00   , 1}, // LL_GAMMA_NEUTRAL_CURVE_0
{0xBC2C, 0x11   , 1}, // LL_GAMMA_NEUTRAL_CURVE_1
{0xBC2D, 0x23   , 1}, // LL_GAMMA_NEUTRAL_CURVE_2
{0xBC2E, 0x50   , 1}, // LL_GAMMA_NEUTRAL_CURVE_3
{0xBC2F, 0x7A   , 1}, // LL_GAMMA_NEUTRAL_CURVE_4
{0xBC30, 0x85   , 1}, // LL_GAMMA_NEUTRAL_CURVE_5
{0xBC31, 0x9B   , 1}, // LL_GAMMA_NEUTRAL_CURVE_6
{0xBC32, 0xAD   , 1}, // LL_GAMMA_NEUTRAL_CURVE_7
{0xBC33, 0xBB   , 1}, // LL_GAMMA_NEUTRAL_CURVE_8
{0xBC34, 0xC7   , 1}, // LL_GAMMA_NEUTRAL_CURVE_9
{0xBC35, 0xD1   , 1}, // LL_GAMMA_NEUTRAL_CURVE_10
{0xBC36, 0xDA   , 1}, // LL_GAMMA_NEUTRAL_CURVE_11
{0xBC37, 0xE1   , 1}, // LL_GAMMA_NEUTRAL_CURVE_12
{0xBC38, 0xE8   , 1}, // LL_GAMMA_NEUTRAL_CURVE_13
{0xBC39, 0xEE   , 1}, // LL_GAMMA_NEUTRAL_CURVE_14
{0xBC3A, 0xF3   , 1}, // LL_GAMMA_NEUTRAL_CURVE_15
{0xBC3B, 0xF7   , 1}, // LL_GAMMA_NEUTRAL_CURVE_16
{0xBC3C, 0xFB   , 1}, // LL_GAMMA_NEUTRAL_CURVE_17
{0xBC3D, 0xFF   , 1}, // LL_GAMMA_NEUTRAL_CURVE_18
{0xBC3E, 0x00   , 1}, // LL_GAMMA_NR_CURVE_0
{0xBC3F, 0x18   , 1}, // LL_GAMMA_NR_CURVE_1
{0xBC40, 0x25   , 1}, // LL_GAMMA_NR_CURVE_2
{0xBC41, 0x3A   , 1}, // LL_GAMMA_NR_CURVE_3
{0xBC42, 0x59   , 1}, // LL_GAMMA_NR_CURVE_4
{0xBC43, 0x70   , 1}, // LL_GAMMA_NR_CURVE_5
{0xBC44, 0x81   , 1}, // LL_GAMMA_NR_CURVE_6
{0xBC45, 0x90   , 1}, // LL_GAMMA_NR_CURVE_7
{0xBC46, 0x9E   , 1}, // LL_GAMMA_NR_CURVE_8
{0xBC47, 0xAB   , 1}, // LL_GAMMA_NR_CURVE_9
{0xBC48, 0xB6   , 1}, // LL_GAMMA_NR_CURVE_10
{0xBC49, 0xC1   , 1}, // LL_GAMMA_NR_CURVE_11
{0xBC4A, 0xCB   , 1}, // LL_GAMMA_NR_CURVE_12
{0xBC4B, 0xD5   , 1}, // LL_GAMMA_NR_CURVE_13
{0xBC4C, 0xDE   , 1}, // LL_GAMMA_NR_CURVE_14
{0xBC4D, 0xE7   , 1}, // LL_GAMMA_NR_CURVE_15
{0xBC4E, 0xEF   , 1}, // LL_GAMMA_NR_CURVE_16
{0xBC4F, 0xF7   , 1}, // LL_GAMMA_NR_CURVE_17
{0xBC50, 0xFF   , 1}, // LL_GAMMA_NR_CURVE_18


//BM_Dampening
{0x098E, 0xB801 , 2}, // LOGICAL_ADDRESS_ACCESS [STAT_MODE]
{0xB801, 0xE0   , 1}, // STAT_MODE
{0xB862, 0x04   , 1}, // STAT_BMTRACKING_SPEED


//AE
{0x098E, 0xB829 , 2}, // LOGICAL_ADDRESS_ACCESS [STAT_LL_BRIGHTNESS_METRIC_DIVISOR]
{0xB829, 0x02   , 1}, // STAT_LL_BRIGHTNESS_METRIC_DIVISOR
{0xB863, 0x02   , 1}, // STAT_BM_MUL
{0xB827, 0x0A   , 1}, // STAT_AE_EV_SHIFT //0919
{0xA40D, 0xF5,   1},
{0xA40E, 0x00,   1},
{0xA40F, 0xF4,   1},
{0xA410, 0xE8,   1},
{0xA411, 0xFB,   1},
//{0xA409, 0x37   , 1}, // AE_RULE_BASE_TARGET //wangaicui
{0xA409, 0x44   , 1}, // AE_RULE_BASE_TARGET


//BM_GM_Start_Stop
{0x098E, 0x3C52 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_BRIGHTNESS_METRIC]
{0xBC52, 0x00C8 , 2}, // LL_START_BRIGHTNESS_METRIC
{0xBC54, 0x0A28 , 2}, // LL_END_BRIGHTNESS_METRIC
{0xBC58, 0x00C8 , 2}, // LL_START_GAIN_METRIC
{0xBC5A, 0x12C0 , 2}, // LL_END_GAIN_METRIC
{0xBC5E, 0x00FA , 2}, // LL_START_APERTURE_GAIN_BM
{0xBC60, 0x0258 , 2}, // LL_END_APERTURE_GAIN_BM
{0xBC66, 0x00FA , 2}, // LL_START_APERTURE_GM
{0xBC68, 0x0258 , 2}, // LL_END_APERTURE_GM
{0xBC86, 0x00C8 , 2}, // LL_START_FFNR_GM
{0xBC88, 0x0640 , 2}, // LL_END_FFNR_GM
{0xBCBC, 0x0040 , 2}, // LL_SFFB_START_GAIN
{0xBCBE, 0x01FC , 2}, // LL_SFFB_END_GAIN
{0xBCCC, 0x00C8 , 2}, // LL_SFFB_START_MAX_GM
{0xBCCE, 0x0640 , 2}, // LL_SFFB_END_MAX_GM
{0xBC90, 0x00C8 , 2}, // LL_START_GRB_GM
{0xBC92, 0x0640 , 2}, // LL_END_GRB_GM
{0xBC0E, 0x0001 , 2}, // LL_GAMMA_CURVE_ADJ_START_POS
{0xBC10, 0x0002 , 2}, // LL_GAMMA_CURVE_ADJ_MID_POS
{0xBC12, 0x02BC , 2}, // LL_GAMMA_CURVE_ADJ_END_POS
{0xBCAA, 0x044C , 2}, // LL_CDC_THR_ADJ_START_POS
{0xBCAC, 0x00AF , 2}, // LL_CDC_THR_ADJ_MID_POS
{0xBCAE, 0x0009 , 2}, // LL_CDC_THR_ADJ_END_POS
{0xBCD8, 0x00C8 , 2}, // LL_PCR_START_BM
{0xBCDA, 0x0A28 , 2}, // LL_PCR_END_BM



//noise model tuning...change only if new model has been completed.
//Kernel
{0x3380, 0x0504 , 2}, // KERNEL_CONFIG

//GRB
{0x098E, 0xBC94 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_GB_START_THRESHOLD_0]
{0xBC94, 0x0C   , 1},	// LL_GB_START_THRESHOLD_0
{0xBC95, 0x08   , 1}, // LL_GB_START_THRESHOLD_1
{0xBC9C, 0x3C   , 1}, // LL_GB_END_THRESHOLD_0
{0xBC9D, 0x28   , 1}, // LL_GB_END_THRESHOLD_1

//Demosaic_REV3
{0x33B0, 0x2A16 , 2}, // FFNR_ALPHA_BETA
{0x098E, 0xBC8A , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_FF_MIX_THRESH_Y]
{0xBC8A, 0x02   , 1}, // LL_START_FF_MIX_THRESH_Y
{0xBC8B, 0x0F   , 1}, // LL_END_FF_MIX_THRESH_Y
{0xBC8C, 0xFF   , 1}, // LL_START_FF_MIX_THRESH_YGAIN
{0xBC8D, 0xFF   , 1}, // LL_END_FF_MIX_THRESH_YGAIN
{0xBC8E, 0xFF   , 1}, // LL_START_FF_MIX_THRESH_GAIN
{0xBC8F, 0x00   , 1}, // LL_END_FF_MIX_THRESH_GAIN


//CDC
{0x098E, 0xBCB2 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_CDC_DARK_CLUS_SLOPE]
{0xBCB2, 0x20   , 1}, // LL_CDC_DARK_CLUS_SLOPE
{0xBCB3, 0x3A   , 1}, // LL_CDC_DARK_CLUS_SATUR
{0xBCB4, 0x39   , 1}, // LL_CDC_BRIGHT_CLUS_LO_LIGHT_SLOPE
{0xBCB7, 0x39   , 1}, // LL_CDC_BRIGHT_CLUS_LO_LIGHT_SATUR
{0xBCB5, 0x20   , 1}, // LL_CDC_BRIGHT_CLUS_MID_LIGHT_SLOPE
{0xBCB8, 0x3A   , 1}, // LL_CDC_BRIGHT_CLUS_MID_LIGHT_SATUR
{0xBCB6, 0x80   , 1}, // LL_CDC_BRIGHT_CLUS_HI_LIGHT_SLOPE
{0xBCB9, 0x24   , 1}, // LL_CDC_BRIGHT_CLUS_HI_LIGHT_SATUR
{0xBCAA, 0x03E8 , 2}, // LL_CDC_THR_ADJ_START_POS
{0xBCAC, 0x012C , 2}, // LL_CDC_THR_ADJ_MID_POS
{0xBCAE, 0x0009 , 2}, // LL_CDC_THR_ADJ_END_POS

//Aperture_calib
{0x33BA, 0x0084 , 2}, // APEDGE_CONTROL
{0x33BE, 0x0000 , 2}, // UA_KNEE_L
{0x33C2, 0x8800 , 2}, // UA_WEIGHTS
{0x098E, 0x3C5E , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_APERTURE_GAIN_BM]
{0xBC5E, 0x0154 , 2}, // LL_START_APERTURE_GAIN_BM
{0xBC60, 0x0640 , 2}, // LL_END_APERTURE_GAIN_BM
{0xBC62, 0x0E   , 1}, // LL_START_APERTURE_KPGAIN
{0xBC63, 0x14   , 1}, // LL_END_APERTURE_KPGAIN
{0xBC64, 0x0E   , 1}, // LL_START_APERTURE_KNGAIN
{0xBC65, 0x14   , 1}, // LL_END_APERTURE_KNGAIN
{0xBCE2, 0x0A   , 1}, // LL_START_POS_KNEE
{0xBCE3, 0x2B   , 1}, // LL_END_POS_KNEE
{0xBCE4, 0x0A   , 1}, // LL_START_NEG_KNEE
{0xBCE5, 0x2B   , 1}, // LL_END_NEG_KNEE
//{0x3210, 0x49B0 , 2},  // COLOR_PIPELINE_CONTROL

//SFFB_REV3_noisemodel
{0x098E, 0xBCC0 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_SFFB_RAMP_START]
{0xBCC0, 0x1F   , 1}, // LL_SFFB_RAMP_START
{0xBCC1, 0x03   , 1}, // LL_SFFB_RAMP_STOP
{0xBCC2, 0x2C   , 1}, // LL_SFFB_SLOPE_START
{0xBCC3, 0x10   , 1}, // LL_SFFB_SLOPE_STOP
{0xBCC4, 0x07   , 1}, // LL_SFFB_THSTART
{0xBCC5, 0x0B   , 1}, // LL_SFFB_THSTOP
{0xBCBA, 0x0009 , 2}, // LL_SFFB_CONFIG



//Step7-CPIPE_Preference	//Color Pipe preference settings, if any
//FTB_OFF
{0x098E, 0x3C14 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_GAMMA_FADE_TO_BLACK_START_POS]
{0xBC14, 0xFFFF , 2}, // LL_GAMMA_FADE_TO_BLACK_START_POS
{0xBC16, 0xFFFF , 2}, // LL_GAMMA_FADE_TO_BLACK_END_POS

//LOAD=Aperture_preference
{0x098E, 0x3C66 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_APERTURE_GM]
{0xBC66, 0x0154 , 2}, // LL_START_APERTURE_GM
{0xBC68, 0x07D0 , 2}, // LL_END_APERTURE_GM
{0xBC6A, 0x04   , 1}, // LL_START_APERTURE_INTEGER_GAIN
{0xBC6B, 0x00   , 1}, // LL_END_APERTURE_INTEGER_GAIN
{0xBC6C, 0x00   , 1}, // LL_START_APERTURE_EXP_GAIN
{0xBC6D, 0x00   , 1}, // LL_END_APERTURE_EXP_GAIN

//Gain_max
{0x098E, 0x281C , 2}, // LOGICAL_ADDRESS_ACCESS [AE_TRACK_MIN_AGAIN]
//{0xA81C, 0x0040 , 2}, // AE_TRACK_MIN_AGAIN
{0xA81C, 0x0060 , 2}, // AE_TRACK_MIN_AGAIN
{0xA820, 0x01FC , 2}, // AE_TRACK_MAX_AGAIN
{0xA822, 0x0080 , 2}, // AE_TRACK_MIN_DGAIN
{0xA824, 0x0100 , 2}, // AE_TRACK_MAX_DGAIN

/*delete some lines for saturation*/

//DCCM_REV3
{0x098E, 0xBCDE , 2}, // LOGICAL_ADDRESS_ACCESS [LL_START_SYS_THRESHOLD]
{0xBCDE, 0x03   , 1}, // LL_START_SYS_THRESHOLD
{0xBCDF, 0x50   , 1}, // LL_STOP_SYS_THRESHOLD
{0xBCE0, 0x08   , 1}, // LL_START_SYS_GAIN
{0xBCE1, 0x03   , 1}, // LL_STOP_SYS_GAIN

//Sobel_REV3
{0x098E, 0x3CD0 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_SFFB_SOBEL_FLAT_START]
{0xBCD0, 0x000A , 2}, // LL_SFFB_SOBEL_FLAT_START
{0xBCD2, 0x00FE , 2}, // LL_SFFB_SOBEL_FLAT_STOP
{0xBCD4, 0x001E , 2}, // LL_SFFB_SOBEL_SHARP_START
{0xBCD6, 0x00FF , 2}, // LL_SFFB_SOBEL_SHARP_STOP
{0xBCC6, 0x00   , 1}, // LL_SFFB_SHARPENING_START
{0xBCC7, 0x00   , 1}, // LL_SFFB_SHARPENING_STOP
{0xBCC8, 0x20   , 1}, // LL_SFFB_FLATNESS_START
{0xBCC9, 0x40   , 1}, // LL_SFFB_FLATNESS_STOP
{0xBCCA, 0x04   , 1}, // LL_SFFB_TRANSITION_START
{0xBCCB, 0x00   , 1}, // LL_SFFB_TRANSITION_STOP

//SFFB_slope_zero_enable
{0x098E, 0xBCE6 , 2}, // LOGICAL_ADDRESS_ACCESS [LL_SFFB_ZERO_ENABLE]
{0xBCE6, 0x03   , 1}, // LL_SFFB_ZERO_ENABLE
{0xBCE6, 0x03   , 1}, // LL_SFFB_ZERO_ENABLE

//AE_preference
{0x098E, 0xA410 , 2}, // LOGICAL_ADDRESS_ACCESS [AE_RULE_TARGET_AE_6]
{0xA410, 0x04   , 1}, // AE_RULE_TARGET_AE_6
{0xA411, 0x06   , 1}, // AE_RULE_TARGET_AE_7

//fix auto focus //20110425//begin
{0x098E, 0xB045 , 2}, 
{0xB045, 0x0014 , 2}, //20110425//end

//[AFM INIT]    
//Initialize iVCM type
{0x098E, 0xC400, 2},     // LOGICAL_ADDRESS_ACCESS [AFM_ALGO]
{0xC400, 0x88,   1},          // AFM_ALGO
{0x8419, 0x05,   1},          // SEQ_STATE_CFG_1_AF
{0x8404, 0x05,   1},          // SEQ_CMD
{0,      100,    0},

//DAC limits 
{0xC40A, 0x0028, 2},     // AFM_POS_MIN
{0xC40C, 0x0098, 2},     // AFM_POS_MAX

//Skip Frame enabled
{0xB002, 0x0302, 2},     // AF_MODE 

//[Fast Focus]
{0xB018, 0x00,   1},          // AF_FS_POS_0
{0xB019, 0x40,   1},          // AF_FS_POS_1
{0xB01A, 0x58,   1},         // AF_FS_POS_2
{0xB01B, 0x68,   1},          // AF_FS_POS_3
{0xB01C, 0x78,   1},          // AF_FS_POS_4
{0xB01D, 0x90,   1},         // AF_FS_POS_5
{0xB01E, 0xA8,   1},         // AF_FS_POS_6
{0xB01F, 0xC0,   1},          // AF_FS_POS_7
{0xB020, 0xFF,   1},          // AF_FS_POS_8
{0xB04A, 0xF8,   1},         // AF_FF_METRIC_TH_0_1
{0xB04B, 0x01,   1},          // AF_FF_METRIC_TH
{0xB04C, 0x0C,   1},         // AF_FF_METRIC_NOISE

{0xB854, 0x20,   1},          // STAT_SM_WINDOW_POS_X
{0xB855, 0x20,   1},          // STAT_SM_WINDOW_POS_Y
{0xB856, 0xBF,   1},          // STAT_SM_WINDOW_SIZE_X
{0xB857, 0xBF,   1},         // STAT_SM_WINDOW_SIZE_Y
{0x8404, 0x06,   1},          // SEQ_CMD

{0,   300,   0},

{0x098E, 0xC428 , 2}, 
{0xC428, 0x82   , 1}, 
//AF OFF//20110425//off

//Step8-Features		  //Ports, special features, etc., if any
{0x098E, 0xC8BC , 2}, // LOGICAL_ADDRESS_ACCESS [CAM_OUTPUT_0_JPEG_QSCALE_0]
{0xC8BC, 0x04   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_0
{0xC8BD, 0x0A   , 1}, // CAM_OUTPUT_0_JPEG_QSCALE_1
{0xC8D2, 0x04   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_0
{0xC8D3, 0x0A   , 1}, // CAM_OUTPUT_1_JPEG_QSCALE_1
{0xDC3A, 0x23   , 1}, // SYS_SEPIA_CR
{0xDC3B, 0xB2   , 1}, // SYS_SEPIA_CB

{0x098E, 0x8404 , 2}, 
{0x8404, 0x06   , 1}, // SEQ_CMD
{0     , 300    , 0},
//delay=300

{0x0018, 0x2008 , 2}, // STANDBY_CONTROL_AND_STATUS
//delay=100
{0     , 100    , 0},
//{0x098E, 0xA818 , 2}, 
//{0xA818, 0x0756 , 2}, //
//{0xA81A, 0x0756 , 2}, // frame rate//wangaicui low frame rate

{0x098E, 0xA816 , 2}, 
{0xA816, 0x0005 , 2}, //0x0680 //up limit
{0xA818, 0x0655 , 2}, //16fps value
{0xA81A, 0x0A23,  2}, //10fps

/*move MIPI setting to step 2*/

//noise
//{0x3380, 0x0505, 2},
//DAC default
//delete DAC default setting because some are same as char setting.
//DAC default
/*Delete default DAC setting*/

//[tint control and saturation]
{0xAC01, 0xFF,   1}, 	// AWB_MODE
{0xAC02, 0x007F, 2}, 	// AWB_ALGO

{0xAC96, 0x01,   1}, 	// AWB_CCM_TINTING_TH
{0xAC97, 0x61,   1},   //0x74 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_0
{0xAC98, 0x80,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_1
{0xAC99, 0x80,   1}, 	// AWB_LEFT_TINT_COEF_FOR_CCM_ROW_2
{0xAC9A, 0x85,   1}, 	// 0x7F AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_0 //0819
{0xAC9B, 0x80,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_1
{0xAC9C, 0x76,   1}, 	// AWB_RIGHT_TINT_COEF_FOR_CCM_ROW_2
{0xAC9D, 0x80,   1}, 	// AWB_PCR_SATURATION
{0xBC56, 0xCA,   1},    //0xa5,   1},//0xca,  //0817  1}, //0x80 	// LL_START_CCM_SATURATION
{0xBC57, 0x00,   1}, 	// LL_END_CCM_SATURATION

//[Pre AWB ratio]
{0xACB0, 0x32,   1}, 	// AWB_RG_MIN
{0xACB1, 0x4E,   1}, 	// AWB_RG_MAX
{0xACB4, 0x26,   1}, 	// AWB_BG_MIN
{0xACB5, 0x4F,   1}, 	// AWB_BG_MAX
{0xAC3C, 0x32,   1}, 	// AWB_MIN_ACCEPTED_PRE_AWB_R2G_RATIO
{0xAC3D, 0x4E,   1}, 	// AWB_MAX_ACCEPTED_PRE_AWB_R2G_RATIO
{0xAC3E, 0x26,   1}, 	// AWB_MIN_ACCEPTED_PRE_AWB_B2G_RATIO
{0xAC3F, 0x4F,   1}, 	// AWB_MAX_ACCEPTED_PRE_AWB_B2G_RATIO

//[Weight table and checker offset]
{0x3242, 0x0000, 2}, 	// AWB_WEIGHT_R0
{0x3244, 0x0000, 2}, 	// AWB_WEIGHT_R1
{0x3246, 0x0000, 2}, 	// AWB_WEIGHT_R2
{0x3248, 0x3F00, 2}, 	// AWB_WEIGHT_R3
{0x324A, 0xA500, 2}, 	// AWB_WEIGHT_R4
{0x324C, 0x1540, 2}, 	// AWB_WEIGHT_R5
{0x324E, 0x01EC, 2}, 	// AWB_WEIGHT_R6
{0x3250, 0x03E0, 2},   // AWB_WEIGHT_R7
{0x323C, 0x0008, 2}, 	// AWB_X_SHIFT
{0x323E, 0x001F, 2}, 	// AWB_Y_SHIFT

{0xB842, 0x003A, 2}, 	// STAT_AWB_GRAY_CHECKER_OFFSET_X
{0xB844, 0x0043, 2}, 	// STAT_AWB_GRAY_CHECKER_OFFSET_Y
//[color kill]
{0xDC36, 0x43,   1}, 	// SYS_DARK_COLOR_KILL
{0xDC02, 0x303E, 2}, 	// SYS_ALGO
{0x35A2, 0x00A3, 2}, 	// DARK_COLOR_KILL_CONTROLS

//[AWB and CCMs 08/23/11 16:56:24]-1
{0x098E, 0x2C46, 2}, 	// LOGICAL_ADDRESS_ACCESS [AWB_LEFT_CCM_0]
{0xAC46, 0x01F6, 2},  //0x01CB, 2}, 	// AWB_LEFT_CCM_0
{0xAC48, 0xFF2A, 2},  //0xFEF7, 2}, 	// AWB_LEFT_CCM_1
{0xAC4A, 0xFFE1, 2},  //0x003F, 2}, 	// AWB_LEFT_CCM_2
{0xAC4C, 0xFFB9, 2},  //0xFFC8, 2}, 	// AWB_LEFT_CCM_3
{0xAC4E, 0x0112, 2},  //0x00F8, 2}, 	// AWB_LEFT_CCM_4
{0xAC50, 0x0035, 2},  //0x0040, 2}, 	// AWB_LEFT_CCM_5
{0xAC52, 0xFFEA, 2},  //0xFFEB, 2}, 	// AWB_LEFT_CCM_6
{0xAC54, 0xFF07, 2},  //0xFE56, 2}, 	// AWB_LEFT_CCM_7
{0xAC56, 0x020E, 2},  //0x02BE, 2}, 	// AWB_LEFT_CCM_8

{0xAC58, 0x00A6, 2}, 	// AWB_LEFT_CCM_R2BRATIO
{0xAC5C, 0x0254, 2}, 	// AWB_RIGHT_CCM_0
{0xAC5E, 0xFED1, 2}, 	// AWB_RIGHT_CCM_1
{0xAC60, 0xFFDB, 2}, 	// AWB_RIGHT_CCM_2
{0xAC62, 0xFFC6, 2}, 	// AWB_RIGHT_CCM_3
{0xAC64, 0x0147, 2}, 	// AWB_RIGHT_CCM_4
{0xAC66, 0xFFF2, 2}, 	// AWB_RIGHT_CCM_5
{0xAC68, 0x0008, 2}, 	// AWB_RIGHT_CCM_6
{0xAC6A, 0xFF6A, 2}, 	// AWB_RIGHT_CCM_7
{0xAC6C, 0x018E, 2}, 	// AWB_RIGHT_CCM_8
{0xAC6E, 0x0076, 2}, 	// AWB_RIGHT_CCM_R2BRATIO
{0xB83E, 0x00,   1}, 	// STAT_AWB_WINDOW_POS_X
{0xB83F, 0x00,   1}, 	// STAT_AWB_WINDOW_POS_Y
{0xB840, 0xFF,   1}, 	// STAT_AWB_WINDOW_SIZE_X
//{0xB841, 0xEF,   1}, 	// STAT_AWB_WINDOW_SIZE_Y//20110823 AWB window
{0xB841, 0xFF,   1}, 	// STAT_AWB_WINDOW_SIZE_Y//20110823 AWB window
//{0xC8ED, 0x02,   1},//bit 0 for preview, bit 1 capture
//[Rev3 Noise Model]
{0x098E, 0xBCC0, 2}, // LOGICAL_ADDRESS_ACCESS
{0xBCC0, 0x1F,   1}, // LL_SFFB_RAMP_START
{0xBCC1, 0x03,   1}, // LL_SFFB_RAMP_STOP
{0xBCC2, 0x3C,   1}, // LL_SFFB_SLOPE_START
{0xBCC3, 0x10,   1}, // LL_SFFB_SLOPE_STOP
{0xBCC4, 0x05,   1}, // LL_SFFB_THSTART
{0xBCC5, 0x0C,   1}, // LL_SFFB_THSTOP
{0xBC90, 0x00C8, 2}, // LL_START_GRB_GM
{0xBC92, 0x0640, 2}, // LL_END_GRB_GM
{0xBC94, 0x13,   1}, // LL_GB_START_THRESHOLD_0
{0xBC95, 0x0C,   1}, // LL_GB_START_THRESHOLD_1
{0xBC9C, 0x2E,   1}, // LL_GB_END_THRESHOLD_0
{0xBC9D, 0x1F,   1}, // LL_GB_END_THRESHOLD_1
{0xBC86, 0x00C8, 2}, // LL_START_FFNR_GM
{0xBC88, 0x0640, 2}, // LL_END_FFNR_GM
{0xBC8A, 0x01,   1}, // LL_START_FF_MIX_THRESH_Y
{0xBC8B, 0x0A,   1}, // LL_END_FF_MIX_THRESH_Y
{0xBC8C, 0xFF,   1}, // LL_START_FF_MIX_THRESH_YGAIN
{0xBC8D, 0xFF,   1}, // LL_END_FF_MIX_THRESH_YGAIN
{0xBC8E, 0xFF,   1}, // LL_START_FF_MIX_THRESH_GAIN
{0xBC8F, 0x00,   1}, // LL_END_FF_MIX_THRESH_GAIN
{0xBC5E, 0x00C8, 2}, // LL_START_APERTURE_GAIN_BM
{0xBC60, 0x0640, 2}, // LL_END_APERTURE_GAIN_BM
{0xBC62, 0x0A,   1}, // LL_START_APERTURE_KPGAIN
{0xBC63, 0x19,   1}, // LL_END_APERTURE_KPGAIN
{0xBC64, 0x0A,   1}, // LL_START_APERTURE_KNGAIN
{0xBC65, 0x19,   1}, // LL_END_APERTURE_KNGAIN
{0xBCE2, 0x0E,   1}, // LL_START_POS_KNEE
{0xBCE3, 0x2C,   1}, // LL_END_POS_KNEE
{0xBCE4, 0x0E,   1}, // LL_START_NEG_KNEE
{0xBCE5, 0x2C,   1}, // LL_END_NEG_KNEE
{0x33BA, 0x006B, 2}, // APEDGE_CONTROL
{0x33C2, 0x2200, 2}, // UA_WEIGHTS
//[COLOR DOT WORKAROUND 5]
{0x31E0, 0x0003, 2}, 
{0x3380, 0x0585, 2}, 

//[CDC_aggressive]
{0x098E, 0xBCB4, 2}, // LOGICAL_ADDRESS_ACCESS [LL_CDC_BRIGHT_CLUS_LO_LIGHT_SLOPE]
{0xBCB4, 0x0A,   1}, // LL_CDC_BRIGHT_CLUS_LO_LIGHT_SLOPE
{0xBCB5, 0x16,   1}, // LL_CDC_BRIGHT_CLUS_MID_LIGHT_SLOPE
{0xBCB6, 0x2B,   1}, // LL_CDC_BRIGHT_CLUS_HI_LIGHT_SLOPE
{0xBCB7, 0x0C,   1}, // LL_CDC_BRIGHT_CLUS_LO_LIGHT_SATUR
{0xBCB8, 0x14,   1}, // LL_CDC_BRIGHT_CLUS_MID_LIGHT_SATUR
{0xBCB9, 0x48,   1}, // LL_CDC_BRIGHT_CLUS_HI_LIGHT_SATUR
#if 0
//[some improve]
{0x324C, 0xECA3, 2}, 	// AWB_WEIGHT_R5
#endif
{0x8404, 0x06,   1}, 	// SEQ_CMD
{0     , 300     , 0},

{0x8404, 0x05   , 1}, // SEQ_CMD
{0     , 200     , 0},       
{0x0028,0x0000 , 2},   //for software standby

};
struct reg_addr_val_pair_struct mt9p111_lensshading_default_array[] = {
//LOAD = Step4-PGA			//PGA LSC parameters

//lens shading  begin
//[Lens Correction 85% 04/18/11 14:40:36]
{0x3210, 0x49B0 , 2}, // FIELD_WR= COLOR_PIPELINE_CONTROL, 0x49B0 
{0x3640, 0x7C6F , 2}, // FIELD_WR= P_G1_P0Q0, 0x7C6F 	            
{0x3642, 0x5C0B , 2}, // FIELD_WR= P_G1_P0Q1, 0x5C0B 	            
{0x3644, 0x1CD1 , 2}, // FIELD_WR= P_G1_P0Q2, 0x1CD1 	            
{0x3646, 0x170E , 2}, // FIELD_WR= P_G1_P0Q3, 0x170E 	            
{0x3648, 0xB0B0 , 2}, // FIELD_WR= P_G1_P0Q4, 0xB0B0 	            
{0x364A, 0x00D0 , 2}, // FIELD_WR= P_R_P0Q0, 0x00D0 	            
{0x364C, 0xC5AD , 2}, // FIELD_WR= P_R_P0Q1, 0xC5AD 	            
{0x364E, 0x12B0 , 2}, // FIELD_WR= P_R_P0Q2, 0x12B0 	            
{0x3650, 0x09AF , 2}, // FIELD_WR= P_R_P0Q3, 0x09AF 	            
{0x3652, 0xB08F , 2}, // FIELD_WR= P_R_P0Q4, 0xB08F 	            
{0x3654, 0x7F8F , 2}, // FIELD_WR= P_B_P0Q0, 0x7F8F 	            
{0x3656, 0x4F6C , 2}, // FIELD_WR= P_B_P0Q1, 0x4F6C 	            
{0x3658, 0x28F0 , 2}, // FIELD_WR= P_B_P0Q2, 0x28F0 	            
{0x365A, 0xCDCB , 2}, // FIELD_WR= P_B_P0Q3, 0xCDCB 	            
{0x365C, 0xA1EF , 2}, // FIELD_WR= P_B_P0Q4, 0xA1EF 	            
{0x365E, 0x0290 , 2}, // FIELD_WR= P_G2_P0Q0, 0x0290 	            
{0x3660, 0xC34E , 2}, // FIELD_WR= P_G2_P0Q1, 0xC34E 	            
{0x3662, 0x1A91 , 2}, // FIELD_WR= P_G2_P0Q2, 0x1A91 	            
{0x3664, 0x100F , 2}, // FIELD_WR= P_G2_P0Q3, 0x100F 	            
{0x3666, 0xB3F0 , 2}, // FIELD_WR= P_G2_P0Q4, 0xB3F0 	            
{0x3680, 0x8E6C , 2}, // FIELD_WR= P_G1_P1Q0, 0x8E6C 	            
{0x3682, 0xCD4E , 2}, // FIELD_WR= P_G1_P1Q1, 0xCD4E 	            
{0x3684, 0xEF6D , 2}, // FIELD_WR= P_G1_P1Q2, 0xEF6D 	            
{0x3686, 0x33AC , 2}, // FIELD_WR= P_G1_P1Q3, 0x33AC 	            
{0x3688, 0x0D4F , 2}, // FIELD_WR= P_G1_P1Q4, 0x0D4F 	            
{0x368A, 0x24ED , 2}, // FIELD_WR= P_R_P1Q0, 0x24ED 	            
{0x368C, 0x216D , 2}, // FIELD_WR= P_R_P1Q1, 0x216D 	            
{0x368E, 0xAA0E , 2}, // FIELD_WR= P_R_P1Q2, 0xAA0E 	            
{0x3690, 0xC3EF , 2}, // FIELD_WR= P_R_P1Q3, 0xC3EF 	            
{0x3692, 0x180F , 2}, // FIELD_WR= P_R_P1Q4, 0x180F 	            
{0x3694, 0xB30D , 2}, // FIELD_WR= P_B_P1Q0, 0xB30D 	            
{0x3696, 0x360D , 2}, // FIELD_WR= P_B_P1Q1, 0x360D 	            
{0x3698, 0x250F , 2}, // FIELD_WR= P_B_P1Q2, 0x250F 	            
{0x369A, 0x8DCF , 2}, // FIELD_WR= P_B_P1Q3, 0x8DCF 	            
{0x369C, 0xB60F , 2}, // FIELD_WR= P_B_P1Q4, 0xB60F 	            
{0x369E, 0x20A9 , 2}, // FIELD_WR= P_G2_P1Q0, 0x20A9 	            
{0x36A0, 0xD88E , 2}, // FIELD_WR= P_G2_P1Q1, 0xD88E 	            
{0x36A2, 0x43EB , 2}, // FIELD_WR= P_G2_P1Q2, 0x43EB 	            
{0x36A4, 0x216E , 2}, // FIELD_WR= P_G2_P1Q3, 0x216E 	            
{0x36A6, 0x112E , 2}, // FIELD_WR= P_G2_P1Q4, 0x112E 	            
{0x36C0, 0x3E71 , 2}, // FIELD_WR= P_G1_P2Q0, 0x3E71 	            
{0x36C2, 0x414F , 2}, // FIELD_WR= P_G1_P2Q1, 0x414F 	            
{0x36C4, 0x6A6E , 2}, // FIELD_WR= P_G1_P2Q2, 0x6A6E 	            
{0x36C6, 0x8B8F , 2}, // FIELD_WR= P_G1_P2Q3, 0x8B8F 	            
{0x36C8, 0xB4D2 , 2}, // FIELD_WR= P_G1_P2Q4, 0xB4D2 	            
{0x36CA, 0x0FF1 , 2}, // FIELD_WR= P_R_P2Q0, 0x0FF1 	            
{0x36CC, 0xD3E8 , 2}, // FIELD_WR= P_R_P2Q1, 0xD3E8 	            
{0x36CE, 0xF52E , 2}, // FIELD_WR= P_R_P2Q2, 0xF52E 	            
{0x36D0, 0xBC30 , 2}, // FIELD_WR= P_R_P2Q3, 0xBC30 	            
{0x36D2, 0x4EAF , 2}, // FIELD_WR= P_R_P2Q4, 0x4EAF 	            
{0x36D4, 0x16B1 , 2}, // FIELD_WR= P_B_P2Q0, 0x16B1 	            
{0x36D6, 0x7B6F , 2}, // FIELD_WR= P_B_P2Q1, 0x7B6F 	            
{0x36D8, 0x0DD0 , 2}, // FIELD_WR= P_B_P2Q2, 0x0DD0 	            
{0x36DA, 0xC2EF , 2}, // FIELD_WR= P_B_P2Q3, 0xC2EF 	            
{0x36DC, 0x8612 , 2}, // FIELD_WR= P_B_P2Q4, 0x8612 	            
{0x36DE, 0x5291 , 2}, // FIELD_WR= P_G2_P2Q0, 0x5291 	            
{0x36E0, 0xFC0D , 2}, // FIELD_WR= P_G2_P2Q1, 0xFC0D 	            
{0x36E2, 0xB4CD , 2}, // FIELD_WR= P_G2_P2Q2, 0xB4CD 	            
{0x36E4, 0x530E , 2}, // FIELD_WR= P_G2_P2Q3, 0x530E 	            
{0x36E6, 0x8DF2 , 2}, // FIELD_WR= P_G2_P2Q4, 0x8DF2 	            
{0x3700, 0x066F , 2}, // FIELD_WR= P_G1_P3Q0, 0x066F 	            
{0x3702, 0x40AD , 2}, // FIELD_WR= P_G1_P3Q1, 0x40AD 	            
{0x3704, 0x8FEF , 2}, // FIELD_WR= P_G1_P3Q2, 0x8FEF 	            
{0x3706, 0x268E , 2}, // FIELD_WR= P_G1_P3Q3, 0x268E 	            
{0x3708, 0xB310 , 2}, // FIELD_WR= P_G1_P3Q4, 0xB310 	            
{0x370A, 0x5C4E , 2}, // FIELD_WR= P_R_P3Q0, 0x5C4E 	            
{0x370C, 0xABEE , 2}, // FIELD_WR= P_R_P3Q1, 0xABEE 	            
{0x370E, 0x488F , 2}, // FIELD_WR= P_R_P3Q2, 0x488F 	            
{0x3710, 0x7430 , 2}, // FIELD_WR= P_R_P3Q3, 0x7430 	            
{0x3712, 0xEB51 , 2}, // FIELD_WR= P_R_P3Q4, 0xEB51 	            
{0x3714, 0xB24E , 2}, // FIELD_WR= P_B_P3Q0, 0xB24E 	            
{0x3716, 0xD66F , 2}, // FIELD_WR= P_B_P3Q1, 0xD66F 	            
{0x3718, 0x1FB0 , 2}, // FIELD_WR= P_B_P3Q2, 0x1FB0 	            
{0x371A, 0x5B71 , 2}, // FIELD_WR= P_B_P3Q3, 0x5B71 	            
{0x371C, 0xE8F1 , 2}, // FIELD_WR= P_B_P3Q4, 0xE8F1 	            
{0x371E, 0x89ED , 2}, // FIELD_WR= P_G2_P3Q0, 0x89ED 	            
{0x3720, 0xDB0D , 2}, // FIELD_WR= P_G2_P3Q1, 0xDB0D 	            
{0x3722, 0x2BF1 , 2}, // FIELD_WR= P_G2_P3Q2, 0x2BF1 	            
{0x3724, 0x0091 , 2}, // FIELD_WR= P_G2_P3Q3, 0x0091 	            
{0x3726, 0xDB52 , 2}, // FIELD_WR= P_G2_P3Q4, 0xDB52 	            
{0x3740, 0x9150 , 2}, // FIELD_WR= P_G1_P4Q0, 0x9150 	            
{0x3742, 0x3190 , 2}, // FIELD_WR= P_G1_P4Q1, 0x3190 	            
{0x3744, 0xD0D4 , 2}, // FIELD_WR= P_G1_P4Q2, 0xD0D4 	            
{0x3746, 0xD133 , 2}, // FIELD_WR= P_G1_P4Q3, 0xD133 	            
{0x3748, 0x3876 , 2}, // FIELD_WR= P_G1_P4Q4, 0x3876 	            
{0x374A, 0xA9F0 , 2}, // FIELD_WR= P_R_P4Q0, 0xA9F0 	            
{0x374C, 0x10B0 , 2}, // FIELD_WR= P_R_P4Q1, 0x10B0 	            
{0x374E, 0xB633 , 2}, // FIELD_WR= P_R_P4Q2, 0xB633 	            
{0x3750, 0x8293 , 2}, // FIELD_WR= P_R_P4Q3, 0x8293 	            
{0x3752, 0x61B5 , 2}, // FIELD_WR= P_R_P4Q4, 0x61B5 	            
{0x3754, 0xD92E , 2}, // FIELD_WR= P_B_P4Q0, 0xD92E 	            
{0x3756, 0x1BCC , 2}, // FIELD_WR= P_B_P4Q1, 0x1BCC 	            
{0x3758, 0x9ED4 , 2}, // FIELD_WR= P_B_P4Q2, 0x9ED4 	            
{0x375A, 0xBE33 , 2}, // FIELD_WR= P_B_P4Q3, 0xBE33 	            
{0x375C, 0x1BB6 , 2}, // FIELD_WR= P_B_P4Q4, 0x1BB6 	            
{0x375E, 0x9CD0 , 2}, // FIELD_WR= P_G2_P4Q0, 0x9CD0 	            
{0x3760, 0x51D1 , 2}, // FIELD_WR= P_G2_P4Q1, 0x51D1 	            
{0x3762, 0xBD34 , 2}, // FIELD_WR= P_G2_P4Q2, 0xBD34 	            
{0x3764, 0x8C34 , 2}, // FIELD_WR= P_G2_P4Q3, 0x8C34 	            
{0x3766, 0x2E16 , 2}, // FIELD_WR= P_G2_P4Q4, 0x2E16 	            
{0x3782, 0x03F0 , 2}, // FIELD_WR= CENTER_ROW, 0x03F0 	          
{0x3784, 0x0508 , 2}, // FIELD_WR= CENTER_COLUMN, 0x0508 	        
{0x3210, 0x49B8 , 2}, // FIELD_WR= COLOR_PIPELINE_CONTROL, 0x49B8 

//lens shading  end
};
#endif

struct mt9p111_work_t {
	struct work_struct work;
};
static struct  mt9p111_work_t *mt9p111_sensorw;
static struct  i2c_client *mt9p111_client;
struct mt9p111_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;
	uint32_t sensormode;
	uint32_t fps_divider;		/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;	/* init to 1 * 0x00000400 */
	uint32_t fps;
	int32_t  curr_lens_pos;
	uint32_t curr_step_pos;
	uint32_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint32_t total_lines_per_frame;
	enum mt9p111_resolution_t prev_res;
	enum mt9p111_resolution_t pict_res;
	enum mt9p111_resolution_t curr_res;
	enum mt9p111_test_mode_t  set_test;
	unsigned short imgaddr;
};
static struct mt9p111_ctrl_t *mt9p111_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(mt9p111_wait_queue);
DEFINE_MUTEX(mt9p111_mut);

/*=============================================================*/

static int mt9p111_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 2,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(mt9p111_client->adapter, msgs, 2) < 0) {
		CDBG("mt9p111_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}
static int32_t mt9p111_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	if (i2c_transfer(mt9p111_client->adapter, msg, 1) < 0) {
		printk("mt9p111_i2c_txdata faild 0x%x\n", mt9p111_client->addr);
		return -EIO;
	}

	return 0;
}

static int32_t mt9p111_i2c_read(unsigned short raddr,
	unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);
	rc = mt9p111_i2c_rxdata(mt9p111_client->addr, buf, 2);
	if (rc < 0) {
		printk("mt9p111_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = buf[0] << 8 | buf[1];
	return rc;
}
static int32_t mt9p111_i2c_write_b_sensor(unsigned short waddr, unsigned short bdata,  int width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (bdata & 0xFF00)>>8;
		buf[3] = (bdata & 0x00FF);
		//rc = mt9p111_i2c_txdata(mt9p111_client->addr >> 1, buf, 4);
		rc = mt9p111_i2c_txdata(mt9p111_client->addr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (bdata & 0x00FF);

		rc = mt9p111_i2c_txdata(mt9p111_client->addr, buf, 3);
	}
		break;
		
	case TIMEZERO_LEN: {
		
		mdelay(bdata);
		rc = 0; 
		
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		printk(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, bdata);

	return rc;
}
static int32_t mt9p111_enter_standby(void)
{
    int32_t rc = 0;
    uint16_t model_id=0xFFFF;
    uint16_t timeout = 0;
    printk("%s:----begin !\n",__FUNCTION__);

    rc = mt9p111_i2c_write_b_sensor(0x3400,0x7A26,2);
    if(rc < 0)
        goto enter_standby_fail;
    
     //Is 0x3402 bit0  1?
    for(timeout= 0;timeout < STANDBY_TIMEOUT +10; timeout += 10)
    {
        mt9p111_i2c_read(0x3402, &model_id);
        model_id = model_id  & 0x0001;
        if(0x0001 == model_id )
        {
            printk("%s :MIPI Standby Successful! reg 0x3402  = 0x%04x\n",__FUNCTION__,model_id);
            break;
        }
        else
        {
            msleep(10);
            printk("%s:MIPI Enter Standby Doing .timeout = 0x%d\n",__FUNCTION__,timeout+10);
        }
        if(timeout >= STANDBY_TIMEOUT)
        {
            MT9P111_ENTER_STANDBY_FLAG = 0x0000;
            printk("**************************************************\n");
            printk("**************************************************\n");
            printk("**************************************************\n");
            printk("ShangBuQi %s:Failed !0x0018 = 0x%04x\n",__FUNCTION__,model_id);
            goto enter_standby_fail;
        }
    }
    
    rc = mt9p111_i2c_write_b_sensor(0x0018,0x2009,2);    
    if(rc < 0)
        goto enter_standby_fail;
    
    //Is 0x0018 bit14  1?
    for(timeout= 0;timeout < STANDBY_TIMEOUT + 10; timeout += 10)
    {
        mt9p111_i2c_read(0x0018, &model_id);
        model_id = (model_id >>14)&0x0001;
        if(0x0001 == model_id)
        {
            MT9P111_ENTER_STANDBY_FLAG = 0x0001;
            printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
            printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
            printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
            printk("HaHa %s:Successful !0x0018 = 0x%04x\n",__FUNCTION__,model_id);
            break;
        }
        else
        {
            msleep(10);
            printk(" %s:Sensor Enter Standby Doing .timeout = 0x%d\n",__FUNCTION__,timeout+10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            MT9P111_ENTER_STANDBY_FLAG = 0x0000;
            printk("**************************************************\n");
            printk("**************************************************\n");
            printk("**************************************************\n");
            printk("ShangBuQi %s:Failed !0x0018 = 0x%04x\n",__FUNCTION__,model_id);
            printk("ShangBuQi %s:Failed !timeout = %d\n",__FUNCTION__,timeout+10);
            goto enter_standby_fail;
        }
    }
    //Here,the sensor enter standby successful!
    printk("%s:----end !\n",__FUNCTION__);
    return rc;
    
enter_standby_fail:
    printk("%s:Failed !\n",__FUNCTION__);
    return rc;
}

static int32_t mt9p111_exit_standby(void)
{
     int32_t rc = 0;
     uint16_t model_id=0xFFFF;
     uint16_t timeout = 0;
     printk("%s:----begin !\n",__FUNCTION__);

     rc = mt9p111_i2c_write_b_sensor(0x0018,0x2008,2);
     if(rc < 0)
        goto exit_standby_fail;
     //Is 0x0018 bit14  0?
     for(timeout= 0;timeout < STANDBY_TIMEOUT + 10; timeout += 10)
     {
        mt9p111_i2c_read(0x0018, &model_id) ;     
        printk("%s:reg 0x0018 = 0x%04x\n",__FUNCTION__,model_id);
        model_id = (model_id >>14)&0x0001;
        
         if(0x0000 == model_id)
         {
            MT9P111_EXIT_STANDBY_FLAG = 0x0001;
            printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
            printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
            printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
            printk("HaHa %s:Successful !0x0018 = 0x%04x\n",__FUNCTION__,model_id);
            printk("HaHa %s:Successful !timeout = 0x%04x\n",__FUNCTION__,timeout+10);
            break;
        
         }
        else
        {
            printk("Wait %s:Doing !0x0018 = 0x%04x\n",__FUNCTION__,model_id);
            printk("Wait %s:Doing !timeout = %d\n",__FUNCTION__,timeout+10);
            msleep(10);        
        }
        if(timeout >= STANDBY_TIMEOUT)
        {
            MT9P111_ENTER_STANDBY_FLAG = 0x0000;
            printk("**************************************************\n");
            printk("**************************************************\n");
            printk("**************************************************\n");
            printk("ShangBuQi %s:Failed !0x0018 = 0x%04x\n",__FUNCTION__,model_id);
            printk("ShangBuQi %s:Failed !timeout = %d\n",__FUNCTION__,timeout+10);
            goto exit_standby_fail;
        }
     }
     printk("%s:----end !\n",__FUNCTION__);
     return rc;
     
exit_standby_fail:
    printk("%s:Failed !\n",__FUNCTION__);
    return rc;
}

#ifdef CONFIG_MT9P111_OTP_LSC
#define OTP_FLAG_DONE        0x4f54
#define OTP_FLAG_DONE1       0x5001
#define OTP_FLAG_DONE2       0x5002
#define OTP_FLAG_DONE3       0x5003

struct reg_addr_val_pair_struct   mt9p111_OTP_init_settings_array[] = {
{0x0010, 0x032D, 2},                                   
{0x0012, 0x00B0, 2},                                      
{0x0014, 0x2025, 2},                                        
{0x001E, 0x0770, 2},        // PAD_SLEW_PAD_CONFIG           

{0x0022, 0x0030, 2},         // VDD_DIS_COUNTER       
{0x002A, 0x7F8A, 2},                                       
{0x002C, 0x0000, 2},                                       
{0x002E, 0x0000, 2},                                       
{0x0018, 0x4008, 2},        // STANDBY_CONTROL_AND_STATUS    
};	
unsigned short  SOC5140_RegAddr[]=
{
    0x3640,
    0x3642,
    0x3644,
    0x3646,
    0x3648,
    0x364A,
    0x364C,
    0x364E,
    0x3650,
    0x3652,
    0x3654,
    0x3656,
    0x3658,
    0x365A,
    0x365C,
    0x365E,
    0x3660,
    0x3662,
    0x3664,
    0x3666,
    0x3680,
    0x3682,
    0x3684,
    0x3686,
    0x3688,
    0x368A,
    0x368C,
    0x368E,
    0x3690,
    0x3692,
    0x3694,
    0x3696,
    0x3698,
    0x369A,
    0x369C,
    0x369E,
    0x36A0,
    0x36A2,
    0x36A4,
    0x36A6,
    0x36C0,
    0x36C2,
    0x36C4,
    0x36C6,
    0x36C8,
    0x36CA,
    0x36CC,
    0x36CE,
    0x36D0,
    0x36D2,
    0x36D4,
    0x36D6,
    0x36D8,
    0x36DA,
    0x36DC,
    0x36DE,
    0x36E0,
    0x36E2,
    0x36E4,
    0x36E6,
    0x3700,
    0x3702,
    0x3704,
    0x3706,
    0x3708,
    0x370A,
    0x370C,
    0x370E,
    0x3710,
    0x3712,
    0x3714,
    0x3716,
    0x3718,
    0x371A,
    0x371C,
    0x371E,
    0x3720,
    0x3722,
    0x3724,
    0x3726,
    0x3740,
    0x3742,
    0x3744,
    0x3746,
    0x3748,
    0x374A,
    0x374C,
    0x374E,
    0x3750,
    0x3752,
    0x3754,
    0x3756,
    0x3758,
    0x375A,
    0x375C,
    0x375E,
    0x3760,
    0x3762,
    0x3764,
    0x3766,
    0x3782,
    0x3784,
};

unsigned short  SOC5140_OTP_Start_Row_lists[] =
{
   0x0009,
   0x0045,
   0x0089,
};
unsigned short SOC5140_RegValue[102];
static int32_t mt9p111_ReadOTPLensShading(void)
{
 
	unsigned short  OTPFlagValueLower = 0;
	unsigned short  OTPFlagValueHigher = 0;      
	bool bOTPWritten[3]={0,0,0};
	unsigned short  OTPStartAddr=0;
	int i = 0;
	unsigned short readValue;
	int32_t rc = 0;
	

    OTPFlagValueLower = 0;
    OTPFlagValueHigher = 0;      
    mt9p111_i2c_write_b_sensor(0x3802, 0x0000, 2);
    mt9p111_i2c_write_b_sensor(0x3804, 0x0001, 2);
    for(i = 0; i<8; i++)
    {	
        mt9p111_i2c_write_b_sensor( 0x3802, 0x0009, 2);
        msleep(10);
        mt9p111_i2c_read(0x380C, &OTPFlagValueLower);
        mt9p111_i2c_read(0x380E, &OTPFlagValueHigher);

        printk("OTPFlagValueLower = 0x%x, OTPFlagValueHigher = 0x%x\n", OTPFlagValueLower, OTPFlagValueHigher);
        
        if (OTP_FLAG_DONE == OTPFlagValueLower)
        {          
            if (OTP_FLAG_DONE1 == OTPFlagValueHigher)
            {
                bOTPWritten[0] = 1;
                bOTPWritten[1] = 0;
                bOTPWritten[2] = 0;               
            }
            else if (OTP_FLAG_DONE2 == OTPFlagValueHigher)
            {
                bOTPWritten[1] = 1;
                bOTPWritten[0] = 0;
                bOTPWritten[2] = 0;
            }
            else if (OTP_FLAG_DONE3 == OTPFlagValueHigher)
            {
                bOTPWritten[2] = 1;
                bOTPWritten[0] = 0;
                bOTPWritten[1] = 0;
            }
        }           
    }
    mt9p111_i2c_write_b_sensor(0x3802, 0x0000, 2);
    if (bOTPWritten[2])
    {
        OTPStartAddr = SOC5140_OTP_Start_Row_lists[2]+2;
    }
    else if(bOTPWritten[1])
    {
        OTPStartAddr = SOC5140_OTP_Start_Row_lists[1]+2;
    }
    else if(bOTPWritten[0])
    {
        OTPStartAddr = SOC5140_OTP_Start_Row_lists[0]+2;
    }
    else
    {    
        printk("Read OTP lens shading start address failed\n");
		return -1;
    }

    printk("Start read otp value...\n");   

    mt9p111_i2c_write_b_sensor(0x3802, 0x0000, 2);
    mt9p111_i2c_write_b_sensor(0x3804, OTPStartAddr, 2);   
    for(i=0; i<102; i++)
    {	
        rc = mt9p111_i2c_write_b_sensor(0x3802,0x0009,2);
	 if(rc<0) 
	 	return rc;
        msleep(10);
        rc = mt9p111_i2c_read(0x380C, &readValue);
	 if(rc<0) 
	 	return rc;
       SOC5140_RegValue[i] = readValue;
        i++;
        rc =  mt9p111_i2c_read(0x380E, &readValue);
	 if(rc<0) 
	 	return rc;
        SOC5140_RegValue[i] = readValue;
    }
    mt9p111_i2c_write_b_sensor(0x3802,0x0000,2);
    printk("read otp value finish\n");

	return 0;
  
}
#endif

static int32_t mt9p111_sensor_setting(int update_type, int rt)
{
	int32_t i = 0, array_length = 0, time_out = 0;
	int32_t rc = 0;
	struct msm_camera_csi_params mt9p111_csi_params;
	uint16_t model_id = 0;
	switch (update_type) {
	case REG_INIT:
		CSI_CONFIG_MT9P111 = 0;
		Sensor_CONFIG_MT9P111 = 0;
		break;
	case UPDATE_PERIODIC:
	if (!CSI_CONFIG_MT9P111) {
		mt9p111_csi_params.lane_cnt = 1;
		mt9p111_csi_params.data_format = CSI_8BIT;
		mt9p111_csi_params.lane_assign = 0xe4;
		mt9p111_csi_params.dpcm_scheme = 0;
		mt9p111_csi_params.settle_cnt = 0x18;
		
		rc = msm_camio_csi_config(&mt9p111_csi_params);
		msleep(10);
		CSI_CONFIG_MT9P111 = 1;
	}
    if (rt == RES_PREVIEW ) {
		printk("mt9p111_sensor_setting--setting preview---begin\n");
		if (!Sensor_CONFIG_MT9P111) {	
			
#ifdef CONFIG_MT9P111_OTP_LSC	
             printk("mt9p111_sensor_setting--setting preview---begin\n");
			//If first time enter the preview mode,running here.
			//If capture to preview,goto else.Config the sensor to be capture to preview.
			Sensor_CONFIG_MT9P111 = 1;
				
			//if MT9P111 enter software standby failed or initialization the register failed,
			//the system must initialization the sensor again,and write all the regiaters.
			if( 0x0000 == MT9P111_ENTER_STANDBY_FLAG 
			|| 0x0000 == MT9P111_INIT_FLAG
			|| 0x0000 == MT9P111_EXIT_STANDBY_FLAG)
			{
				printk("MT9P111_ENTER_STANDBY_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9P111_ENTER_STANDBY_FLAG);
				printk("MT9P111_INIT_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9P111_INIT_FLAG);
				printk("MT9P111_EXIT_STANDBY_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9P111_EXIT_STANDBY_FLAG);
				/********* write init setting  init setting( A )  ***********/ 
				printk("WAC: write init setting  init setting( A )--\n");
				array_length = sizeof(mt9p111_init_settings_A_array) /
					sizeof(mt9p111_init_settings_A_array[0]);
				for (i = 0; i < array_length; i++) {
					rc = mt9p111_i2c_write_b_sensor(
						mt9p111_init_settings_A_array[i].reg_addr,
						mt9p111_init_settings_A_array[i].reg_val, 
						mt9p111_init_settings_A_array[i].lens);
					if (rc < 0)
						return rc;				
				}

             		/********* write lens shading from OTP  ***********/                           
				if(Sensor_OTPReaLensshading_MT9P111)
				{
	           			printk("WAC:write lens shading from OTP--\n");
					mt9p111_i2c_write_b_sensor(0x3210, 0x49B0, 2);
					for(i=0;i<102;i++)
					{
				 	 	mt9p111_i2c_write_b_sensor(SOC5140_RegAddr[i], SOC5140_RegValue[i], 2);
						//printk("i =%d, reg = 0x%x,   val = 0x%x\n", i, SOC5140_RegAddr[i], SOC5140_RegValue[i]);
					}
					mt9p111_i2c_write_b_sensor(0x3210, 0x49B8, 2);
           			}else
       			{
           				printk("WAC:write lens shading from default setting--\n");
					array_length = sizeof(mt9p111_lensshading_default_array) /
						sizeof(mt9p111_lensshading_default_array[0]);
					for (i = 0; i < array_length; i++) {
						rc = mt9p111_i2c_write_b_sensor(
							mt9p111_lensshading_default_array[i].reg_addr,
							mt9p111_lensshading_default_array[i].reg_val, 
							mt9p111_lensshading_default_array[i].lens);
						if (rc < 0)
							return rc;				
					}
				}
						
				/********* write init setting  init setting( B )  ***********/ 
				printk("WAC:write init setting  init setting( B )--\n");
				array_length = sizeof(mt9p111_init_settings_B_array) /
					sizeof(mt9p111_init_settings_B_array[0]);
				for (i = 0; i < array_length; i++) {
					rc = mt9p111_i2c_write_b_sensor(
						mt9p111_init_settings_B_array[i].reg_addr,
						mt9p111_init_settings_B_array[i].reg_val, 
						mt9p111_init_settings_B_array[i].lens);
					if (rc < 0)
						return rc;				
				}
				//if initialization the register again,set MT9P111_INIT_FLAG to be 0x0001.
                MT9P111_INIT_FLAG = 0x0001;
					
				mdelay(30);
				mt9p111_i2c_write_b_sensor(0x098E, 0x8404, 2);
				mt9p111_i2c_read(0x8404, &model_id); 
				model_id = (model_id & 0xFF00 ) >> 8;	
				printk("Preview:mt9p111 reg=0x8404, value = 0x%04x\n", model_id);

				mt9p111_i2c_write_b_sensor(0x098E, 0x8404, 2);  
				mt9p111_i2c_read(0x8404, &model_id) ;
				model_id = (model_id & 0x00FF );
				printk("Preview:mt9p111 reg=0x8405, value = 0x%04x\n", model_id);
			
				mt9p111_i2c_write_b_sensor(0x098E, 0x8405, 2);  
				mt9p111_i2c_read(0x8405, &model_id) ;
				model_id = (model_id & 0xFF00 ) >> 8;
				printk("Preview:mt9p111 reg=0x8405, value = 0x%04x\n", model_id);

				mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2);  
				mt9p111_i2c_read(0xDC38, &model_id) ;
				model_id = (model_id & 0xFF00 ) >> 8;
				printk("Preview:mt9p111 reg=0xDC38, value = 0x%04x\n", model_id);
			}
			else
			{
				printk("MT9P111_ENTER_STANDBY_FLAG = %d  Congratulations!It's running so fast!--OTP\n",MT9P111_ENTER_STANDBY_FLAG);
				printk("MT9P111_INIT_FLAG = %d  Congratulations!It's running so fast!--OTP \n",MT9P111_INIT_FLAG);
				printk("MT9P111_EXIT_STANDBY_FLAG = %d  Congratulations!It's running so fast!--OTP \n",MT9P111_EXIT_STANDBY_FLAG);
				mt9p111_exit_standby();
				printk("mt9p111_sensor_setting---mt9p111_exit_standby--OTP\n");
			}
#else
			/********* write init setting all  init setting  ***********/ 
			printk("mt9p111_sensor_setting--setting preview---begin\n");
			//If first time enter the preview mode,running here.
			//If capture to preview,goto else.Config the sensor to be capture to preview.
			Sensor_CONFIG_MT9P111 = 1;
				
			//if MT9P111 enter software standby failed or initialization the register failed,
			//the system must initialization the sensor again,and write all the regiaters.
			if( 0x0000 == MT9P111_ENTER_STANDBY_FLAG 
                   || 0x0000 == MT9P111_INIT_FLAG 
                   || 0x0000 == MT9P111_EXIT_STANDBY_FLAG )
			{
				printk("MT9P111_ENTER_STANDBY_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9P111_ENTER_STANDBY_FLAG);
				printk("MT9P111_INIT_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9P111_INIT_FLAG);
				printk("MT9P111_EXIT_STANDBY_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9P111_EXIT_STANDBY_FLAG);

				array_length = sizeof(mt9p111_init_settings_array) /
					sizeof(mt9p111_init_settings_array[0]);
				for (i = 0; i < array_length; i++) {
					rc = mt9p111_i2c_write_b_sensor(
						mt9p111_init_settings_array[i].reg_addr,
						mt9p111_init_settings_array[i].reg_val, 
						mt9p111_init_settings_array[i].lens);
					if (rc < 0)
						return rc;				
				}
				//if initialization the register again,set MT9P111_INIT_FLAG to be 0x0001.
                MT9P111_INIT_FLAG = 0x0001;
					
				mdelay(30);
				mt9p111_i2c_write_b_sensor(0x098E, 0x8404, 2);
				mt9p111_i2c_read(0x8404, &model_id); 
				model_id = (model_id & 0xFF00 ) >> 8;	
				printk("Preview:mt9p111 reg=0x8404, value = 0x%04x\n", model_id);

				mt9p111_i2c_write_b_sensor(0x098E, 0x8404, 2);  
				mt9p111_i2c_read(0x8404, &model_id) ;
				model_id = (model_id & 0x00FF );
				printk("Preview:mt9p111 reg=0x8405, value = 0x%04x\n", model_id);
			
				mt9p111_i2c_write_b_sensor(0x098E, 0x8405, 2);  
				mt9p111_i2c_read(0x8405, &model_id) ;
				model_id = (model_id & 0xFF00 ) >> 8;
				printk("Preview:mt9p111 reg=0x8405, value = 0x%04x\n", model_id);

				mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2);  
				mt9p111_i2c_read(0xDC38, &model_id) ;
				model_id = (model_id & 0xFF00 ) >> 8;
				printk("Preview:mt9p111 reg=0xDC38, value = 0x%04x\n", model_id);

			}
			else
			{
				 printk("MT9P111_ENTER_STANDBY_FLAG = %d  Congratulations!It's running so fast!\n",MT9P111_ENTER_STANDBY_FLAG);
				 printk("MT9P111_INIT_FLAG = %d  Congratulations!It's running so fast! \n",MT9P111_INIT_FLAG);
				 printk("MT9P111_EXIT_STANDBY_FLAG = %d  Congratulations!It's running so fast! \n",MT9P111_EXIT_STANDBY_FLAG);
				 mt9p111_exit_standby();
				 printk("mt9p111_sensor_setting---mt9p111_exit_standby\n");
			}
#endif

		}else
		{			
			mt9p111_i2c_write_b_sensor(0x098E, 0x843C, 2);
			
			mt9p111_i2c_write_b_sensor(0x843C, 0x01,   1);
			
			mt9p111_i2c_write_b_sensor(0x8404, 0x01,   1);
			
			mt9p111_i2c_write_b_sensor(0x0016, 0x0447, 2);

			msleep(100);

	        for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_LONG; time_out++) //polling 0x8404 status until 0x00
        	{
				mt9p111_i2c_read(0x8404, &model_id); 
				model_id = (model_id & 0xFF00 ) >> 8;	
				CDBG(" %d, mt9p111 reg=0x8404, value = 0x%x\n", time_out, model_id);
				if (0 == model_id)
					break;
				msleep(10);
        	}
			for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_MIDDLE; time_out++)//polling 0x8405 status until 0x03;
			{
				mt9p111_i2c_read(0x8405, &model_id) ;
				model_id = (model_id & 0xFF00 ) >> 8;				
				CDBG(" %d  mt9p111 reg=0x8405, value = 0x%x\n", time_out, model_id);
				if (0x03 == model_id){
					CDBG("WAC:  succeed to entry preview mode now\n");
					break;
				}
				msleep(20);
        	}
 
     		CDBG("Close AF first after capture !! \n");
     		mt9p111_i2c_write_b_sensor(0x098E, 0x8419, 2); // LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_1_AF]
 
     		mt9p111_i2c_write_b_sensor(0x8419, 0x00,   1); // SEQ_STATE_CFG_1_AF
 
     		mt9p111_i2c_write_b_sensor(0xC428, 0x82,   1); // AFM_DRIVER_CONTROL
 
     		mt9p111_i2c_write_b_sensor(0x8404, 0x05,   1); // SEQ_CMD	  
 
            msleep(50);
 
		}
			msleep(20);
			
			CDBG("mt9p111_sensor_setting--setting preview---end\n");
			return rc;
		}else if(rt == RES_CAPTURE)
		{	
			CDBG("mt9p111_sensor_setting--setting Capture---begin\n");			

			mt9p111_i2c_write_b_sensor(0x098E, 0x843C, 2);  
                
			mt9p111_i2c_write_b_sensor(0x843C, 0xFF,   1);    
			                
			mt9p111_i2c_write_b_sensor(0x8404, 0x02,   1);    
			
			msleep(100);

	        for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_LONG; time_out++)//polling 0x8404 status
        	{
				mt9p111_i2c_read(0x8404, &model_id); 
				model_id = (model_id & 0xFF00 ) >> 8;	
				CDBG(" %d, mt9p111 reg=0x8404, value = 0x%x\n", time_out, model_id);
				if (0 == model_id)
					break;
				msleep(10);
        	}

			for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_SHORT; time_out++)//polling 0x8405 status till 0x07 for capture
			{
				mt9p111_i2c_read(0x8405, &model_id) ;
				model_id = (model_id & 0xFF00 ) >> 8;				
				CDBG(" %d  mt9p111 reg=0x8405, value = 0x%x\n", time_out, model_id);
				if (0x07 == model_id){
					CDBG("WAC:  succeed to entry snapshot mode now !!!\n");
					break;
				}
				msleep(20);
        	}
			if (0x07 != model_id){
				for (i=0; i<2;i++)
				{
					CDBG("mt9p111_sensor_setting--setting Capture--time = %d\n", i+2);			

					mt9p111_i2c_write_b_sensor(0x098E, 0x843C, 2);  
		                
					mt9p111_i2c_write_b_sensor(0x843C, 0xFF,   1);    
					                
					mt9p111_i2c_write_b_sensor(0x8404, 0x02,   1);    

					msleep(100);
			        for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_LONG; time_out++)//polling 0x8404 status until 0x00
		        	{
						mt9p111_i2c_read(0x8404, &model_id); 
						model_id = (model_id & 0xFF00 ) >> 8;	
						CDBG(" %d, mt9p111 reg=0x8404, value = 0x%x\n", time_out, model_id);
						if (0 == model_id)
							break;
						msleep(10);
		        	}

					//mt9p111_i2c_write_b_sensor(0x098E, 0x8405, 2);  
		            for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_MIDDLE; time_out++)//polling 0x8404 status until 0x07;
					{
						mt9p111_i2c_read(0x8405, &model_id) ;
						model_id = (model_id & 0xFF00 ) >> 8;				
						CDBG(" %d,  mt9p111 reg=0x8405, value = 0x%x\n", time_out, model_id);
						if (0x07 == model_id){
							CDBG(" succeed to entry snapshot mode now i=%d\n",i+2);
							break;
						}			
						msleep(20);
		        	}
					if (0x07 == model_id){
						CDBG(" succeed to entry snapshot mode now !!!\n");
						break;
					}
					
				}

			}
			CDBG("mt9p111_sensor_setting--setting Capture---end\n");
		}
		
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static int32_t mt9p111_close_AF(void)
{
    int32_t rc = 0;
    uint16_t model_id = 0;
    int32_t timeout = 0;
    mt9p111_i2c_write_b_sensor(0x098E, 0x843C, 2);
    mt9p111_i2c_write_b_sensor(0x843C, 0x01,   1);
    mt9p111_i2c_write_b_sensor(0x8404, 0x01,   1);
    mt9p111_i2c_write_b_sensor(0x0016, 0x0447, 2);
    msleep(100);
    for (timeout = 0; timeout < 400; timeout+=10) //polling 0x8404 status until 0x00
    {
        mt9p111_i2c_read(0x8404, &model_id); 
        model_id = (model_id & 0xFF00 ) >> 8;	
        printk(" %d, mt9p111 reg=0x8404, value = 0x%x\n", timeout, model_id);
        if (0 == model_id)
    	    break;
        msleep(10);
    }
    for (timeout = 0; timeout < 400; timeout+=10)//polling 0x8405 status until 0x03;
    {
        mt9p111_i2c_read(0x8405, &model_id) ;
        model_id = (model_id & 0xFF00 ) >> 8;				
        printk(" %d  mt9p111 reg=0x8405, value = 0x%x\n", timeout, model_id);
        if (0x03 == model_id){
    	    printk("WAC:  succeed to entry preview mode now\n");
    	    break;
        }
        msleep(20);
    }
    mt9p111_i2c_write_b_sensor(0x098E, 0x8419, 2); // LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_1_AF]
    mt9p111_i2c_write_b_sensor(0x8419, 0x00,   1); // SEQ_STATE_CFG_1_AF
    mt9p111_i2c_write_b_sensor(0xC428, 0x82,   1); // AFM_DRIVER_CONTROL
    mt9p111_i2c_write_b_sensor(0x8404, 0x05,   1); // SEQ_CMD      
    msleep(100);
    for(timeout = 0;timeout < 600;timeout += 50)
    {
        mt9p111_i2c_read(0x8404, &model_id);
        model_id = (model_id & 0xFF00 ) >> 8;
        if(0x00 == model_id)
        {
            printk("OK!,timeout = %d,model_id = 0x%02x\n",timeout+50,model_id);
            break;
        } 
       else
        {
            printk("Doing! ,timeout = %d,model_id = 0x%02x\n",timeout+50,model_id);
            msleep(50);
        }
    }
    printk("Doing!,timeout = %d,model_id = 0x%02x\n",timeout+50,model_id);
    return rc;
}

static int32_t mt9p111_video_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/* change sensor resolution if needed */
	rt = RES_PREVIEW;
	if (mt9p111_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	mt9p111_ctrl->curr_res = mt9p111_ctrl->prev_res;
	mt9p111_ctrl->sensormode = mode;
	return rc;
}

static int32_t mt9p111_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;
	printk("mt9p111_snapshot_config-----begin\n");
	/* change sensor resolution if needed */
	rt = RES_CAPTURE;
	if (mt9p111_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	mt9p111_ctrl->curr_res = mt9p111_ctrl->prev_res;
	mt9p111_ctrl->sensormode = mode;
	printk("mt9p111_snapshot_config-----end\n");
	return rc;
}

static int32_t mt9p111_set_sensor_mode(int mode,
	int res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9p111_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = mt9p111_snapshot_config(mode);
	case SENSOR_RAW_SNAPSHOT_MODE:
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static int mt9p111_set_effect(int mode,int8_t effect)
{
	int rc = 0;	
	printk("mt9p111_set_effect  effect=%d\n",effect);
	switch (effect) {
	case CAMERA_EFFECT_OFF: 
	      mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2); 	// LOGICAL_ADDRESS_ACCESS [SYS_SELECT_FX]       
		mt9p111_i2c_write_b_sensor(0xDC38, 0x00,   1); 	// SYS_SELECT_FX                                  
		mt9p111_i2c_write_b_sensor(0xDC02, 0x307E, 2);  	// SYS_ALGO  0x302E                                   
		//mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                        
 
		msleep(50);	   
		break;
		
	case CAMERA_EFFECT_MONO: 

		mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2);  	// LOGICAL_ADDRESS_ACCESS [SYS_SELECT_FX] 
		mt9p111_i2c_write_b_sensor(0xDC38, 0x01,   1); 	// SYS_SELECT_FX                            
		mt9p111_i2c_write_b_sensor(0xDC02, 0x307E, 2);  	// SYS_ALGO  0x306E                             
		//mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                  

		msleep(50);			
		break;

	case CAMERA_EFFECT_NEGATIVE: 
		mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2);  	// LOGICAL_ADDRESS_ACCESS [SYS_SELECT_FX]  
		mt9p111_i2c_write_b_sensor(0xDC38, 0x03,   1); 	// SYS_SELECT_FX                             
		mt9p111_i2c_write_b_sensor(0xDC02, 0x307E, 2);  	// SYS_ALGO  0x306E                           
		//mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                   

		msleep(50);
			
		break;

	case CAMERA_EFFECT_SOLARIZE: 
		mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2);  	// LOGICAL_ADDRESS_ACCESS [SYS_SELECT_FX] 
		mt9p111_i2c_write_b_sensor(0xDC38, 0x04,   1); 	// SYS_SELECT_FX                            
		mt9p111_i2c_write_b_sensor(0xDC02, 0x307E, 2);  	// SYS_ALGO  0x306E                             
		mt9p111_i2c_write_b_sensor(0xDC39, 0x4E,   1);               // SYS_SOLARIZATION_TH
		//mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                  

		msleep(50);		
		break;

	case CAMERA_EFFECT_SEPIA: 
	    mt9p111_i2c_write_b_sensor(0x098E, 0xDC38, 2);  	// LOGICAL_ADDRESS_ACCESS [SYS_SELECT_FX]     
	    mt9p111_i2c_write_b_sensor(0xDC38, 0x02,   1); 	// SYS_SELECT_FX                                
	    mt9p111_i2c_write_b_sensor(0xDC02, 0x307E, 2);      // SYS_ALGO  0x306E                               
	    //mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                      

	    msleep(50);			
	    break;

	default: {
	            
	   return rc;
	     }
	}
return rc;

}
static long mt9p111_set_whitebalance(int mode,int8_t wb)
{
	long rc = 0;
    int timeout =0;
    uint16_t value =0x00;
	printk("mt9p111_set_whitebalance  wb=%d\n",wb);

	switch (wb) {	
	case CAMERA_WB_AUTO: {
		mt9p111_i2c_write_b_sensor(0x098E, 0x8410, 2); 	// LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_0_AWB]  
		mt9p111_i2c_write_b_sensor(0x8410, 0x02,   1); 	// SEQ_STATE_CFG_0_AWB                             
		mt9p111_i2c_write_b_sensor(0x8418, 0x02,   1);  	// SEQ_STATE_CFG_1_AWB                             
		mt9p111_i2c_write_b_sensor(0x8420, 0x02,   1);  	// SEQ_STATE_CFG_2_AWB                             
		mt9p111_i2c_write_b_sensor(0xAC44, 0x00,   1);  	// AWB_LEFT_CCM_POS_RANGE_LIMIT                    
		mt9p111_i2c_write_b_sensor(0xAC45, 0x7F,   1);  	// AWB_RIGHT_CCM_POS_RANGE_LIMIT                   
#if 1
		mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1);  	// SEQ_CMD
#endif
		mt9p111_i2c_write_b_sensor(0xAC04, 0x40,   1);  	// AWB_PRE_AWB_R2G_RATIO                           
		mt9p111_i2c_write_b_sensor(0xAC05, 0x4D,   1);  	// AWB_PRE_AWB_B2G_RATIO
		//msleep(50);
		for(timeout = 0;timeout < 600;timeout += 20)
        {
            mt9p111_i2c_read(0x8404, &value);
            value = (value & 0xFF00 ) >> 8;
            if(0x00 == value)
            {
                printk("OK!,timeout = %d,value = 0x%02x\n",timeout+50,value);
                break;
            } 
             else
            {
                printk("Doing!,timeout = %d,value = 0x%02x\n",timeout+50,value);
                msleep(20);
            }
        }
	}
		break;
	case CAMERA_WB_INCANDESCENT: {
		
		mt9p111_i2c_write_b_sensor(0x098E, 0x8410, 2); 	// LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_0_AWB]     
		mt9p111_i2c_write_b_sensor(0x8410, 0x01,   1); 	// SEQ_STATE_CFG_0_AWB                                
		mt9p111_i2c_write_b_sensor(0x8418, 0x01,   1); 	// SEQ_STATE_CFG_1_AWB                                
		mt9p111_i2c_write_b_sensor(0x8420, 0x01,   1); 	// SEQ_STATE_CFG_2_AWB                                
		mt9p111_i2c_write_b_sensor(0xAC44, 0x7F,   1); 	// AWB_LEFT_CCM_POS_RANGE_LIMIT                       
		mt9p111_i2c_write_b_sensor(0xAC45, 0x7F,   1); 	// AWB_RIGHT_CCM_POS_RANGE_LIMIT                      
		mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                            
		msleep(50);
		mt9p111_i2c_write_b_sensor(0xAC04, 0x5B,   1); 	// AWB_PRE_AWB_R2G_RATIO                              
		mt9p111_i2c_write_b_sensor(0xAC05, 0x28,   1); 	// AWB_PRE_AWB_B2G_RATIO                              

	     
	}
		break;

 	case CAMERA_WB_DAYLIGHT: {
		mt9p111_i2c_write_b_sensor(0x098E, 0x8410, 2); 	// LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_0_AWB]
		mt9p111_i2c_write_b_sensor(0x8410, 0x01,   1); 	// SEQ_STATE_CFG_0_AWB                           
		mt9p111_i2c_write_b_sensor(0x8418, 0x01,   1); 	// SEQ_STATE_CFG_1_AWB                           
		mt9p111_i2c_write_b_sensor(0x8420, 0x01,   1); 	// SEQ_STATE_CFG_2_AWB                           
		mt9p111_i2c_write_b_sensor(0xAC44, 0x7F,   1); 	// AWB_LEFT_CCM_POS_RANGE_LIMIT                  
		mt9p111_i2c_write_b_sensor(0xAC45, 0x7F,   1); 	// AWB_RIGHT_CCM_POS_RANGE_LIMIT                 
		mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                       
		msleep(50);
		mt9p111_i2c_write_b_sensor(0xAC04, 0x3C,   1); 	// AWB_PRE_AWB_R2G_RATIO                         
		mt9p111_i2c_write_b_sensor(0xAC05, 0x41,   1); 	// AWB_PRE_AWB_B2G_RATIO

	    
	}
		break;

	case CAMERA_WB_CLOUDY_DAYLIGHT: {
		mt9p111_i2c_write_b_sensor(0x098E, 0x8410, 2); 	// LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_0_AWB]
		mt9p111_i2c_write_b_sensor(0x8410, 0x01,   1); 	// SEQ_STATE_CFG_0_AWB                           
		mt9p111_i2c_write_b_sensor(0x8418, 0x01,   1); 	// SEQ_STATE_CFG_1_AWB                           
		mt9p111_i2c_write_b_sensor(0x8420, 0x01,   1); 	// SEQ_STATE_CFG_2_AWB                           
		mt9p111_i2c_write_b_sensor(0xAC44, 0x7F,   1); 	// AWB_LEFT_CCM_POS_RANGE_LIMIT                  
		mt9p111_i2c_write_b_sensor(0xAC45, 0x7F,   1); 	// AWB_RIGHT_CCM_POS_RANGE_LIMIT                 
		mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                       
		msleep(50);
		mt9p111_i2c_write_b_sensor(0xAC04, 0x3C,   1); 	// AWB_PRE_AWB_R2G_RATIO                         
		mt9p111_i2c_write_b_sensor(0xAC05, 0x55,   1); 	// AWB_PRE_AWB_B2G_RATIO                         

		
	}
		break;
        case CAMERA_WB_FLUORESCENT: {
		mt9p111_i2c_write_b_sensor(0x098E, 0x8410, 2); 	// LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_0_AWB] 
		mt9p111_i2c_write_b_sensor(0x8410, 0x01,   1); 	// SEQ_STATE_CFG_0_AWB                            
		mt9p111_i2c_write_b_sensor(0x8418, 0x01,   1); 	// SEQ_STATE_CFG_1_AWB                            
		mt9p111_i2c_write_b_sensor(0x8420, 0x01,   1); 	// SEQ_STATE_CFG_2_AWB                            
		mt9p111_i2c_write_b_sensor(0xAC44, 0x7F,   1); 	// AWB_LEFT_CCM_POS_RANGE_LIMIT                   
		mt9p111_i2c_write_b_sensor(0xAC45, 0x7F,   1); 	// AWB_RIGHT_CCM_POS_RANGE_LIMIT                  
		mt9p111_i2c_write_b_sensor(0x8404, 0x06,   1); 	// SEQ_CMD                                        
		msleep(50);
		mt9p111_i2c_write_b_sensor(0xAC04, 0x4A,   1); 	// AWB_PRE_AWB_R2G_RATIO                          
		mt9p111_i2c_write_b_sensor(0xAC05, 0x32,   1); 	// AWB_PRE_AWB_B2G_RATIO                          

		
	}
		break;

	case CAMERA_WB_TWILIGHT: {

	}
		break;

	case CAMERA_WB_SHADE:
	default: {

		return rc;
		}
	}

	return rc;
}

static long mt9p111_set_default_focus(int mode,int32_t af_step)
{

	long rc = 0;
	uint16_t model_id = 0;
	uint16_t bit15 = 0, bit13 = 0;
	int time_out = 0;
	CDBG("mt9p111_set_default_focus  begin \n");
	
	//[VCM_Enable  full scan1]

	mt9p111_i2c_write_b_sensor(0x098E, 0x8419, 2); // LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_1_AF]
	
	mt9p111_i2c_write_b_sensor(0x8419, 0x04,   1); // SEQ_STATE_CFG_1_AF
	
	mt9p111_i2c_write_b_sensor(0xC428, 0x81,   1); // AFM_DRIVER_CONTROL
	
	mt9p111_i2c_write_b_sensor(0x8404, 0x05,   1); // SEQ_CMD

    msleep(100);

	//[AF Trigger]                                                
                                      
	mt9p111_i2c_write_b_sensor(0x098E, 0xB006, 2);     // LOGICAL_ADDRESS_ACCESS [AF_PROGRESS]  
	                                         
	mt9p111_i2c_write_b_sensor(0xB006, 0x01,   1);       // AF_PROGRESS                        

	for (time_out = 0; time_out < MT9P111_STATUS_WAIT_TIMES_LONG;  time_out++)
	{
		mt9p111_i2c_write_b_sensor(0x098E, 0xB007, 2);  
		mt9p111_i2c_read(0xB007, &model_id);
		model_id = (model_id & 0xFF00 ) >> 8;
		CDBG(" %d, lens pos = 0x%x\n", time_out, model_id);

		mt9p111_i2c_write_b_sensor(0x098E, 0x3000, 2); 	
		mt9p111_i2c_read(0xB000, &model_id);
		CDBG(" %d, reg=0xB000, value = 0x%x\n", time_out, model_id);
		model_id = (model_id & 0x0010) >> 4;
		if (model_id == 1)
			break;
		msleep(43);
	}
		
    if (MT9P111_STATUS_WAIT_TIMES_LONG == time_out){
		
		mt9p111_i2c_write_b_sensor(0x098E, 0x8419, 2); // LOGICAL_ADDRESS_ACCESS [SEQ_STATE_CFG_1_AF]

		mt9p111_i2c_write_b_sensor(0x8419, 0x00,   1); // SEQ_STATE_CFG_1_AF

		mt9p111_i2c_write_b_sensor(0xC428, 0x82,   1); // AFM_DRIVER_CONTROL

		mt9p111_i2c_write_b_sensor(0x8404, 0x05,   1); // SEQ_CMD

		msleep(100);	

		rc = -EIO;
		CDBG("mt9p111_set_default_focus  rc=%ld \n", rc);
		CDBG("mt9p111_set_default_focus  end \n");
		
		return rc;
	}

	msleep(30);

	mt9p111_i2c_write_b_sensor(0x098E, 0x3000, 2); 	
	mt9p111_i2c_read(0xB000, &model_id);
	CDBG(" reg=0xB000, value = 0x%x\n", model_id);
	
	bit15 = (model_id & 0x8000) >> 15; 
	bit13 = (model_id & 0x2000) >> 13;
	CDBG(" reg=0xB000, bit 15 value = %x\n", bit15);
	CDBG(" reg=0xB000, bit 13 value = %x\n", bit13);
	if (bit15 | bit13)
		rc = -EIO;
 	CDBG("mt9p111_set_default_focus  rc=%ld \n", rc);
	CDBG("mt9p111_set_default_focus  end \n");
	return rc;
}

static int mt9p111_set_antibanding(int mode,int8_t antibanding)
{
	int rc = 0;	
	printk("%s  antibanding =%d\n",__FUNCTION__,antibanding);
	switch (antibanding) {
	case CAMERA_ANTIBANDING_AUTO: 
   		//printk("%s  antibanding =CAMERA_ANTIBANDING_AUTO\n",__FUNCTION__);
		mt9p111_i2c_write_b_sensor(0x098E, 0x8417, 2); 
		mt9p111_i2c_write_b_sensor(0x8417, 0x02, 1); 
		mt9p111_i2c_write_b_sensor(0x8404, 0x06, 1); 
		break;
		
	case CAMERA_ANTIBANDING_60HZ: 
		//printk("%s  antibanding =CAMERA_ANTIBANDING_60HZ\n",__FUNCTION__);
		mt9p111_i2c_write_b_sensor(0x098E, 0x8417, 2); 
		mt9p111_i2c_write_b_sensor(0x8417, 0x01, 1); 
		mt9p111_i2c_write_b_sensor(0xA004, 0x3C, 1); 
		mt9p111_i2c_write_b_sensor(0x8404, 0x06, 1);
		break;

	case CAMERA_ANTIBANDING_50HZ: 
           // printk("%s  antibanding =CAMERA_ANTIBANDING_50HZ\n",__FUNCTION__);
		mt9p111_i2c_write_b_sensor(0x098E, 0x8417, 2);   
		mt9p111_i2c_write_b_sensor(0x8417, 0x01, 1); 
		mt9p111_i2c_write_b_sensor(0xA004, 0x32, 1); 
		mt9p111_i2c_write_b_sensor(0x8404, 0x06, 1);
		break;

	case CAMERA_ANTIBANDING_OFF: 
		//printk("%s  antibanding =CAMERA_ANTIBANDING_OFF\n",__FUNCTION__);
		mt9p111_i2c_write_b_sensor(0x098E, 0x8417, 2); 
		mt9p111_i2c_write_b_sensor(0x8417, 0x00, 1); 
		mt9p111_i2c_write_b_sensor(0x8404, 0x06, 1); 
		break;

	default: {
	     // printk("%s  antibanding =wrong\n",__FUNCTION__);      
	     	rc = -EINVAL;		
		break;
	     }
	}
	return rc;
}
static int32_t mt9p111_power_down(void)
{
	printk("mt9p111_power_down\n");
	return 0;
}
static int mt9p111_probe_init_done(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	printk("mt9p111_probe_init_done----begin\n");
	rc = gpio_request(data->sensor_reset, "mt9p111");
	if (!rc) {
		printk("sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		msleep(20);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		msleep(20);
	} else {
		printk("mt9p111 gpio reset fail");
		goto init_probe_done;
	}
	//gpio_direction_output(data->sensor_pwd, 1);
	//msleep(20);
	gpio_direction_output(data->sensor_reset, 0);
	//gpio_free(data->sensor_pwd);
	gpio_free(data->sensor_reset);
	printk("mt9p111_probe_init_done----end\n");
	return rc;
init_probe_done:
	return rc;
}
static int mt9p111_probe_open_sensor(void)
{
    uint16_t model_id;
    int32_t rc = 0;
    
    printk("mt9p111_probe_open_sensor-----------begin\n");
    
    // Read sensor Model ID://need to check
    rc = mt9p111_i2c_read(REG_MT9P111_MODEL_ID, &model_id);
    if(rc<0)
        goto init_probe_fail;

    printk("mt9p111_probe_open_sensor model_id = 0x%04x\n", model_id);
    // Compare sensor ID to MT9P111 ID:
    if (model_id != MT9P111_MODEL_ID)
    {
        rc = -ENODEV;
        goto init_probe_fail;
    }

    printk("mt9p111_probe_open_sensor-----------end\n");
    goto init_probe_done;
    
init_probe_fail:
    printk("mt9p111_probe_open_sensor fails_kalyani\n");
    return rc;//if error,then return -1

init_probe_done:
    printk("mt9p111_probe_open_sensor finishes\n");
    return rc;
}
#ifndef CONFIG_MT9P111_OTP_LSC
static int mt9p111_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id;
	int32_t rc = 0;
	
	printk("mt9p111_probe_init_sensor-----------begin\n");
	
	printk("data->sensor_pwd = %d\n", data->sensor_pwd);
	printk("data->sensor_reset = %d\n", data->sensor_reset);

	rc = gpio_request(data->sensor_pwd, "mt9p111");
	if (!rc) {
		gpio_direction_output(data->sensor_pwd, 0);
		msleep(5);
	} else {
		printk("mt9p111 gpio pwdn fail");
		goto init_probe_done;
	}
	
	rc = gpio_request(data->sensor_reset, "mt9p111");
	if (!rc) {
		printk("sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		msleep(20);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		msleep(20);
	} else {
		printk("mt9p111 gpio reset fail");
		goto init_probe_done;
	}

	/* 3. Read sensor Model ID: *///need to check
	rc = mt9p111_i2c_read(REG_MT9P111_MODEL_ID, &model_id);
	if(rc<0)
		goto init_probe_fail;

	printk("mt9p111 model_id = 0x%x\n",
		 model_id);
	/* 4. Compare sensor ID to MT9P111 ID: */
	if (model_id != MT9P111_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}

	printk("mt9p111_probe_init_sensor-----------end\n");
	goto init_probe_done;
init_probe_fail:
	printk(KERN_INFO " mt9p111_probe_init_sensor fails_kalyani\n");
	gpio_free(data->sensor_pwd);//release the GPIO for liteon sensor
	gpio_free(data->sensor_reset);//release the GPIO for liteon sensor
    //gpio_set_value_cansleep(data->sensor_reset, 0);//release the GPIO for liteon sensor
	return rc;//if error,then return -1

init_probe_done:
	printk(KERN_INFO " mt9p111_probe_init_sensor finishes\n");
	return rc;
}
#endif

#ifdef CONFIG_MT9P111_OTP_LSC    
static int mt9p111_probe_init_sensor_probe(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id;
	uint16_t OTPCheckValue;
	int32_t rc = 0;
	int32_t i,array_length;
	int32_t time_out;
	
	printk("mt9p111_probe_init_sensor_probe-----------begin\n");
	msm_camio_clk_rate_set(12000000);
	printk("data->sensor_pwd = %d\n", data->sensor_pwd);
	printk("data->sensor_reset = %d\n", data->sensor_reset);

	rc = gpio_request(data->sensor_pwd, "mt9p111");
	if (!rc) {
		gpio_direction_output(data->sensor_pwd, 0);
		msleep(5);
	} else {
		printk("mt9p111 gpio pwdn fail");
		goto init_probe_done;
	}
	
	rc = gpio_request(data->sensor_reset, "mt9p111");
	if (!rc) {
		printk("sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		msleep(20);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		msleep(20);
	} else {
		printk("mt9p111 gpio reset fail");
		goto init_probe_done;
	}
	/* 1. Read sensor Model ID: */
	rc = mt9p111_i2c_read(REG_MT9P111_MODEL_ID, &model_id);
	if(rc<0)
		goto init_probe_fail;

	printk("mt9p111 model_id = 0x%x\n",
		 model_id);
	/* 2. Compare sensor ID to MT9P111 ID: */
	if (model_id != MT9P111_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}
	/* 3. OTP initialization */
	array_length = sizeof(mt9p111_OTP_init_settings_array) /
					sizeof(mt9p111_OTP_init_settings_array[0]);
	for (i = 0; i < array_length; i++) {
		rc = mt9p111_i2c_write_b_sensor(
			mt9p111_OTP_init_settings_array[i].reg_addr,
			mt9p111_OTP_init_settings_array[i].reg_val, 
			mt9p111_OTP_init_settings_array[i].lens);
		if (rc < 0)
			return rc;				
	}

	/* POLL R0x0014[15] until its value = 1  PLL locked DELAY 50ms*/
	for (time_out = 0; time_out <200; time_out += 10 ){
		rc = mt9p111_i2c_read(0x0014, &OTPCheckValue);
		if (rc < 0)
			return rc;
		OTPCheckValue = (OTPCheckValue & 0x8000) >> 15;
		printk("reg = 0x0014,  bit 15 value  = 0x%x\n",  OTPCheckValue);
		if (OTPCheckValue != 1)
			{
			mdelay(10);
			}
		else
			{
			break;
			}
	}
    msleep(50);
	/*POLL R0x0018[14] until its value = 0 */
	for (time_out = 0; time_out <200; time_out += 10 ){
		rc = mt9p111_i2c_read(0x0018, &OTPCheckValue);
		if (rc < 0)
			return rc;
		OTPCheckValue = (OTPCheckValue & 0x4000) >> 14;
		printk("reg = 0x0018,  bit 14 value  = 0x%x\n",  OTPCheckValue);
		if (OTPCheckValue != 0)
			{
			mdelay(10);
			}
		else
			{
			break;
			}
	}

	rc = mt9p111_i2c_write_b_sensor(0x3812, 0x2124, 2);        // OTPM_CFG
	if (rc < 0)
		return rc;
	rc = mt9p111_i2c_write_b_sensor(0x3814, 0x0F1F, 2);       // OTPM_TCFG_01   
	if (rc < 0)
		return rc;
	rc = mt9p111_i2c_write_b_sensor(0x3816, 0x0F1F, 2);        // OTPM_TCFG_23 
	if (rc < 0)
		return rc;
	rc = mt9p111_i2c_write_b_sensor(0x3818, 0x4407, 2);      // OTPM_TCFG_4B
	if (rc < 0)
		return rc;
	rc = mt9p111_i2c_write_b_sensor(0x098E, 0x602A, 2);     
	if (rc < 0)
		return rc;

	/* WRITE VAR = 24, R0x002A[0] = 0x0001 //Probe the OTPM */
	rc = mt9p111_i2c_read(0xE023, &OTPCheckValue);
	if (rc < 0)
		return rc;
    OTPCheckValue = OTPCheckValue | 0x1;

	rc = mt9p111_i2c_write_b_sensor(0xE02A, OTPCheckValue, 2);    
	if (rc < 0)
		return rc;
	
	/*POLL VAR = 24, R0x0023[6] until its value = 1*/
	for (time_out = 0; time_out <200; time_out += 10 ){
		rc = mt9p111_i2c_read(0xE023, &OTPCheckValue);
		if (rc < 0)
			return rc;
		OTPCheckValue = (OTPCheckValue & 0x4000) >> 14;
		printk("reg = R0x0023,  bit 6 value  = 0x%x\n",  OTPCheckValue);
		if (OTPCheckValue != 1)
			{
			mdelay(10);
			}
		else
			{
			break;
			}
	}

	/*4. OTP read lenshading */
	printk("WAC: --- Call mt9p111_ReadOTPLensShading\n");
	rc = mt9p111_ReadOTPLensShading();
	if(rc<0)
	{
		printk("WAC: --- Call mt9p111_ReadOTPLensShading  failed  Sensor_OTPReaLensshading_MT9P111 = 0\n");
		Sensor_OTPReaLensshading_MT9P111 = 0;
	}else
	{
		printk("WAC: --- Call mt9p111_ReadOTPLensShading  succeed  Sensor_OTPReaLensshading_MT9P111 = 1\n");
		Sensor_OTPReaLensshading_MT9P111 = 1;	
	}
    /*5. finish OTP read lenshading, go to hardreset */      
	msm_camio_clk_rate_set(24000000);
    msleep(20);
    printk("mt9p111_probe_init_sensor_otp----change the mclk\n");

	printk("sensor_reset = %d\n", rc);
	gpio_direction_output(data->sensor_reset, 0);
	msleep(20);
	gpio_set_value_cansleep(data->sensor_reset, 1);
	msleep(20);

	printk("mt9p111_probe_init_sensor_probe-----------end\n");
	goto init_probe_done;
init_probe_fail:
	printk(KERN_INFO " mt9p111_probe_init_sensor_probe fails_kalyani\n");
	gpio_free(data->sensor_pwd);//release the GPIO for liteon sensor
	gpio_free(data->sensor_reset);//release the GPIO for liteon sensor
    //gpio_set_value_cansleep(data->sensor_reset, 0);//release the GPIO for liteon sensor
	return rc;//if error,then return -1

    //return rc;

init_probe_done:
	printk(KERN_INFO " mt9p111_probe_init_sensor_probe finishes\n");
	return rc;
}
#endif


int mt9p111_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	printk("%s: %d\n", __func__, __LINE__);
	printk("Calling mt9p111_sensor_open_init\n");
	mt9p111_ctrl = kzalloc(sizeof(struct mt9p111_ctrl_t), GFP_KERNEL);
	if (!mt9p111_ctrl) {
		CDBG("mt9p111_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	mt9p111_ctrl->fps_divider = 1 * 0x00000400;
	mt9p111_ctrl->pict_fps_divider = 1 * 0x00000400;
	mt9p111_ctrl->fps = 30 * Q8;
	mt9p111_ctrl->set_test = TEST_OFF;
	mt9p111_ctrl->prev_res = QTR_SIZE;
	mt9p111_ctrl->pict_res = FULL_SIZE;
	mt9p111_ctrl->curr_res = INVALID_SIZE;

	if (data)
		mt9p111_ctrl->sensordata = data;

	/* enable mclk first */

	msm_camio_clk_rate_set(24000000);
	msleep(20);

	  //rc = mt9p111_probe_init_sensor(data);
      rc = mt9p111_probe_open_sensor();
      if (rc < 0)
      {
          printk("Calling mt9p111_sensor_open_init fail\n");
          goto init_fail;
      }


	rc = mt9p111_sensor_setting(REG_INIT, RES_PREVIEW);

	if (rc < 0) {
		gpio_set_value_cansleep(data->sensor_reset, 0);
		goto init_fail;
	} else
		goto init_done;
	
init_fail:
	printk(" mt9p111_sensor_open_init fail\n");
	mt9p111_probe_init_done(data);
	kfree(mt9p111_ctrl);
init_done:
	printk("mt9p111_sensor_open_init done\n");
	return rc;
}

static int mt9p111_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9p111_wait_queue);
	return 0;
}

static const struct i2c_device_id mt9p111_i2c_id[] = {
	{"mt9p111", 0},
	{ }
};

static int mt9p111_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	printk("mt9p111_i2c_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	mt9p111_sensorw = kzalloc(sizeof(struct mt9p111_work_t), GFP_KERNEL);
	if (!mt9p111_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9p111_sensorw);
	mt9p111_init_client(client);
	mt9p111_client = client;

	msleep(50);

	printk("mt9p111_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	printk("mt9p111_i2c_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit mt9p111_remove(struct i2c_client *client)
{
	struct mt9p111_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	mt9p111_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver mt9p111_i2c_driver = {
	.id_table = mt9p111_i2c_id,
	.probe  = mt9p111_i2c_probe,
	.remove = __exit_p(mt9p111_remove),
	.driver = {
		.name = "mt9p111",
	},
};

int mt9p111_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&mt9p111_mut);
	CDBG("mt9p111_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SET_MODE:
		rc = mt9p111_set_sensor_mode(cdata.mode,
			cdata.rs);
		break;
	case CFG_SET_DEFAULT_FOCUS:
		rc=mt9p111_set_default_focus(
					cdata.mode,
					cdata.cfg.focus.steps);
		break;
	case CFG_SET_EFFECT:
		rc = mt9p111_set_effect(
					cdata.mode,
					cdata.cfg.effect);
		break;	
	case CFG_SET_WB:
		rc=mt9p111_set_whitebalance(
					cdata.mode,
					cdata.cfg.whitebalance);
		break;

	case CFG_PWR_DOWN:
		rc = mt9p111_power_down();
		break;
	case CFG_SET_ANTIBANDING:
		//printk("fuck------------------------");
		rc = mt9p111_set_antibanding(
						cdata.mode,
						cdata.cfg.antibanding);
		break;  
	default:
		rc = -EFAULT;
		break;
	}
	mutex_unlock(&mt9p111_mut);

	return rc;
}
static int mt9p111_sensor_release(void)
{
	int rc = -EBADF;
	printk("mt9p111_release begin\n");
	mutex_lock(&mt9p111_mut);
#if 0
	mt9p111_power_down();
	gpio_set_value_cansleep(mt9p111_ctrl->sensordata->sensor_reset, 0);
	gpio_free(mt9p111_ctrl->sensordata->sensor_reset);

	msleep(5);
	gpio_direction_output(mt9p111_ctrl->sensordata->sensor_pwd, 1);
	gpio_free(mt9p111_ctrl->sensordata->sensor_pwd);
	msleep(10);
#else
	mt9p111_enter_standby();
#endif
	kfree(mt9p111_ctrl);
	mt9p111_ctrl = NULL;
	printk("mt9p111_release completed\n");
	mutex_unlock(&mt9p111_mut);

	return rc;
}
static int mt9p111_sensor_probe(const struct msm_camera_sensor_info *info,
        struct msm_sensor_ctrl *s)
{
    int rc = 0;
    int32_t i = 0;
    int32_t array_length = 0;
    //if the system have find a sensor for back camera,goto else.
    if(0x0000 == BACK_CAMERA_SENSOR_DETECT)
    {
        printk("mt9p111_sensor_probe-----------begin\n");
        rc = i2c_add_driver(&mt9p111_i2c_driver);
        if (rc < 0 || mt9p111_client == NULL)
        {
            rc = -ENOTSUPP;
            goto probe_fail;
        }
#ifdef CONFIG_MT9P111_OTP_LSC
        rc = mt9p111_probe_init_sensor_probe(info);
        if (rc < 0)
            goto probe_fail;
#else
        msm_camio_clk_rate_set(24000000);
        printk("msm_camio_clk_rate_set(24000000)--probe sensor--end\n");
        
        rc = mt9p111_probe_init_sensor(info);
        if (rc < 0)
            goto probe_fail;
#endif

        //if in mt9p111_probe_init_sensor(info) the system find a sensor,the system will run the follow program.
        //And you must set BACK_CAMERA_SENSOR_DETECT to be 0x0001 before the systm write the sensor's register.
        BACK_CAMERA_SENSOR_DETECT = 0x0001;

	    s->s_init = mt9p111_sensor_open_init;
	    s->s_release = mt9p111_sensor_release;
	    s->s_config  = mt9p111_sensor_config;
	    s->s_camera_type = BACK_CAMERA_2D;     
	    s->s_mount_angle = 0;

#ifdef CONFIG_MT9P111_OTP_LSC
        //********* write init setting  init setting( A )  ***********
        printk("WAC: write init setting  init setting( A )--\n");
        array_length = sizeof(mt9p111_init_settings_A_array) /
            sizeof(mt9p111_init_settings_A_array[0]);
        for (i = 0; i < array_length; i++)
        {
            rc = mt9p111_i2c_write_b_sensor(
                mt9p111_init_settings_A_array[i].reg_addr,
                mt9p111_init_settings_A_array[i].reg_val, 
                mt9p111_init_settings_A_array[i].lens);
            if (rc < 0)
            {
                printk("%s: Write the sensor register failed!----setting( A )\n",__FUNCTION__);
                goto probe_fail;
            }
        }

        //********* write lens shading from OTP  ***********                           
        if(Sensor_OTPReaLensshading_MT9P111)
        {
            printk("WAC:write lens shading from OTP--\n");
            mt9p111_i2c_write_b_sensor(0x3210, 0x49B0, 2);
            for(i=0;i<102;i++)
            {
                rc = mt9p111_i2c_write_b_sensor(SOC5140_RegAddr[i], SOC5140_RegValue[i], 2);
#if 0
                printk("i =%3d, reg = 0x%04x,   val = 0x%04x\n", i, SOC5140_RegAddr[i], SOC5140_RegValue[i]);
#endif
                if(rc < 0)
                {
                    printk("%s: Write the sensor register failed!----From OTP\n",__FUNCTION__);
                    goto probe_fail;
                }
            }
            mt9p111_i2c_write_b_sensor(0x3210, 0x49B8, 2);
        }
        else
           {
            printk("WAC:write lens shading from default setting--\n");
            array_length = sizeof(mt9p111_lensshading_default_array) /
                sizeof(mt9p111_lensshading_default_array[0]);
            for (i = 0; i < array_length; i++)
            {
                rc = mt9p111_i2c_write_b_sensor(
                    mt9p111_lensshading_default_array[i].reg_addr,
                    mt9p111_lensshading_default_array[i].reg_val, 
                    mt9p111_lensshading_default_array[i].lens);
                if (rc < 0)
                {
                    printk("%s: Write the sensor register failed!----From default setting\n",__FUNCTION__);
                    goto probe_fail;
                }
            }
        }                        
        //********* write init setting  init setting( B )  ***********
        printk("WAC:write init setting  init setting( B )--\n");
            array_length = sizeof(mt9p111_init_settings_B_array) /
                sizeof(mt9p111_init_settings_B_array[0]);
        for (i = 0; i < array_length; i++)
        {
            rc = mt9p111_i2c_write_b_sensor(
                mt9p111_init_settings_B_array[i].reg_addr,
                mt9p111_init_settings_B_array[i].reg_val, 
                mt9p111_init_settings_B_array[i].lens);
            if (rc < 0)
            {
                printk("%s: Write the sensor register failed!----setting( B )\n",__FUNCTION__);
                goto probe_fail;
            }
        }
        //if  write the sensor register successful,the system will run the follow program.
        //And if system have find the sensor,but write the register failed.When open the camera,
        //it's must initialization the sensor again.
        MT9P111_INIT_FLAG = 0x0001;
#else
        //********* write init setting all  init setting  ***********
        array_length = sizeof(mt9p111_init_settings_array) /
            sizeof(mt9p111_init_settings_array[0]);
        for (i = 0; i < array_length; i++)
        {
            rc = mt9p111_i2c_write_b_sensor(
                mt9p111_init_settings_array[i].reg_addr,
                mt9p111_init_settings_array[i].reg_val, 
                mt9p111_init_settings_array[i].lens);
            if (rc < 0)
            {
                printk("%s: Write the sensor register failed!----All  init setting\n",__FUNCTION__);
                goto probe_fail;
            }                
        }
        
        //if  write the sensor register successful,the system will run the follow program.
        //And if system have find the sensor,but write the register failed.When open the camera,
        //it's must initialization the sensor again.
        MT9P111_INIT_FLAG = 0x0001;
        
#endif
        mt9p111_close_AF();
        //mt9p111_probe_init_done(info);
        //Enter the software standby mode.
        rc = mt9p111_enter_standby();
        if (rc < 0)
        {
            printk("The system have find the sensor and intialization it,but enter software standby failed!\n");
            goto probe_fail;

        } 
        else
        {
            printk("The system will use the sensor of  MT9P111\n");
            printk("mt9p111_sensor_probe-----------end\n");
            return rc;
        }
    }
    else
    {
        printk("The system have finded other sensor !\n");
        //return -EIO,it's show the system have find other sensor.
        //Attention,if you return 0,the system will add the device node,it's wrong!!!
        //return -EIO;
        return -1;
    }
    
probe_fail:
    printk("mt9p111_sensor_probe: SENSOR PROBE FAILS!\n");
    mt9p111_probe_init_done(info);
    i2c_del_driver(&mt9p111_i2c_driver);
    return rc;
}

static int __mt9p111_probe(struct platform_device *pdev)
{

	return msm_camera_drv_start(pdev, mt9p111_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9p111_probe,
	.driver = {
		.name = "msm_camera_mt9p111",
		.owner = THIS_MODULE,
	},
};

static int __init mt9p111_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9p111_init);

MODULE_DESCRIPTION("Aptina 5M YUV sensor driver");
MODULE_LICENSE("GPL v2");


