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
#include "mt9m114.h"
static bool CSI_CONFIG_MT9M114; 
static bool Sensor_CONFIG_MT9M114 = 0;
/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define Q8    0x00000100

#define REG_MT9M114_MODEL_ID                       0x0000

#define MT9M114_MODEL_ID                       0x2481
//enter or exit software standby mode the time of timeout 
#define STANDBY_TIMEOUT 600

/*==============================================================================
**
** FRONT_CAMERA_SENSOR_DETECT
**
** Value: FRONT_CAMERA_SENSOR_DETECT must be 0x000 or 0x0001.
** Method:
** During the system register the camera sensor driver,if the module id is right,set it to be 0x0001,
** or else set it to be 0x0000.
** When adding a new sensor driver,you may add the sensor's prob function here.
**
** Alarm:
** When modify the value,you must be careful about the follow function.
** mt9m114_sensor_probe();
** 
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
//it's initialization in mt9m114.c.
//extern unsigned short FRONT_CAMERA_SENSOR_DETECT;
unsigned short FRONT_CAMERA_SENSOR_DETECT = 0x0000;

/*========================================================================
**
** MT9M114_INIT_FLAG
**
** After the system register the camera sensor driver,it will write all the register.if it's failed,
** set the globle variable to 0x0000;
**
** It's value will be used in function mt9p111_sensor_setting();
**
==========================================================================*/

static unsigned short MT9M114_INIT_FLAG = 0x0000;


/*=========================================================================
**
** MT9M114_ENTER_STANDBY_FLAG
**
** After the system initialization the senor,it will enter the standby mode.if it's failed,
** set the globle variable to 0x0000;
**
** It's value will be used in function mt9m114_setting();
** 
===========================================================================*/

static unsigned short MT9M114_ENTER_STANDBY_FLAG = 0x0000;


/*=========================================================================
**
** MT9M114_EXIT_STANDBY_FLAG
**
** After the system initialization the senor,it will enter the standby mode.if it's failed,
** set the globle variable to 0x0000;
**
** It's value will be used in function mt9m111_sensor_setting();
** 
===========================================================================*/
//First time,it will not  run exit standby.
static unsigned short MT9M114_EXIT_STANDBY_FLAG = 0x0001;

/*============================================================================
							DATA DECLARATIONS
============================================================================*/
/*  720P @ 24MHz MCLK */
struct reg_addr_val_pair_struct mt9m114_init_settings_array[] = {

{0x3E14, 0xFF39, 2}, 
{0x316A, 0x8270, 2}, 
{0x316C, 0x8270, 2}, 
{0x3ED0, 0x2305, 2},
{0x3ED2, 0x77CF, 2},
{0x316E, 0x8202, 2}, 
{0x3180, 0x87FF, 2},
{0x30D4, 0x6080, 2},
{0xA802, 0x0008, 2},

//[Step2-PLL_Timing]
{0x098E, 0x1000 ,2},
{0xC97E, 0x01  , 1},  //cam_sysctl_pll_enable = 1
{0xC980, 0x0120, 2},  //cam_sysctl_pll_divider_m_n = 288
{0xC982, 0x0700, 2},  //cam_sysctl_pll_divider_p = 1792
{0xC984, 0x8001, 2},  //cam_port_output_control = 32769
{0xC988, 0x0F00, 2},  //cam_port_mipi_timing_t_hs_zero = 3840
{0xC98A, 0x0B07, 2}, //cam_port_mipi_timing_t_hs_exit_hs_trail = 2823
{0xC98C, 0x0D01, 2}, //cam_port_mipi_timing_t_clk_post_clk_pre = 3329
{0xC98E, 0x071D, 2},  //cam_port_mipi_timing_t_clk_trail_clk_zero = 1821
{0xC990, 0x0006, 2},  //cam_port_mipi_timing_t_lpx = 6
{0xC992, 0x0A0C, 2}, //cam_port_mipi_timing_init_timing = 2572
{0xC800, 0x0004, 2},  //cam_sensor_cfg_y_addr_start = 4
{0xC802, 0x0004, 2},  //cam_sensor_cfg_x_addr_start = 4
{0xC804, 0x03CB, 2},  //cam_sensor_cfg_y_addr_end = 971
{0xC806, 0x050B, 2},  //cam_sensor_cfg_x_addr_end = 1291
{0xC808, 0x02DC, 2},  //cam_sensor_cfg_pixclk = 48000000
{0xc80a, 0x6c00, 2},
{0xC80C, 0x0001, 2},  //cam_sensor_cfg_row_speed = 1
{0xC80E, 0x00DB, 2},  //cam_sensor_cfg_fine_integ_time_min = 219
{0xC810, 0x05C1, 2},  //cam_sensor_cfg_fine_integ_time_max = 1473
{0xC812, 0x03F3, 2},  //cam_sensor_cfg_frame_length_lines = 1011
{0xC814, 0x0644, 2},  //cam_sensor_cfg_line_length_pck = 1604
{0xC816, 0x0060, 2},  //cam_sensor_cfg_fine_correction = 96
{0xC818, 0x03C3, 2},  //cam_sensor_cfg_cpipe_last_row = 963
{0xC826, 0x0020, 2},  //cam_sensor_cfg_reg_0_data = 32
{0xC834, 0x0003, 2},  //cam_sensor_control_read_mode = 0
{0xC854, 0x0000, 2},  //cam_crop_window_xoffset = 0
{0xC856, 0x0000, 2},  //cam_crop_window_yoffset = 0
{0xC858, 0x0500, 2},  //cam_crop_window_width = 1280
{0xC85A, 0x03C0, 2},  //cam_crop_window_height = 960
{0xC85C, 0x03  , 1},  //cam_crop_cropmode = 3
{0xC868, 0x0500, 2},  //cam_output_width = 1280
{0xC86A, 0x03C0, 2},  //cam_output_height = 960
{0xC878, 0x00  , 1},  //cam_aet_aemode = 0
{0xC88C, 0x1D99, 2},  //cam_aet_max_frame_rate = 7577
{0xC88E, 0x0e1a, 2},  //0x0ECD //cam_aet_min_frame_rate = 3789
{0xC914, 0x0000, 2},  //cam_stat_awb_clip_window_xstart = 0
{0xC916, 0x0000, 2},  //cam_stat_awb_clip_window_ystart = 0
{0xC918, 0x04FF, 2},  //cam_stat_awb_clip_window_xend = 1279
{0xC91A, 0x03BF, 2},  //cam_stat_awb_clip_window_yend = 959
{0xC91C, 0x0000, 2},  //cam_stat_ae_initial_window_xstart = 0
{0xC91E, 0x0000, 2},  //cam_stat_ae_initial_window_ystart = 0
{0xC920, 0x00FF, 2},  //cam_stat_ae_initial_window_xend = 255
{0xC922, 0x00BF, 2},  //cam_stat_ae_initial_window_yend = 191
{0x098E, 0xDC00, 2},  // LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE]s already set"
{0xDC00, 0x28  , 1},  // SYSMGR_NEXT_STATE      
{0x0080, 0x8002, 2},   // COMMAND_REGISTER   //FIELD_WR= COMMAND_REGISTER, 0x8002  // REG= 0x0080, 0x8002             
{0, 100, 0},//delay=100

//[Step3-Recommended]                    
#if 0
{0x316A, 0x8270, 2},                    
{0x316C, 0x8270, 2},                    
{0x3ED0, 0x3605, 2},                    
{0x3ED2, 0x77CF, 2},                    
{0x316E, 0x8203, 2},                    
{0x3180, 0x87FF, 2},                    
{0x30D4, 0x6080, 2},                    
{0xA802, 0x0008, 2},                    
#endif
                    
{0x3E14, 0xFF39, 2},                    
{0x301A, 0x0234, 2}, 	// RESET_REGISTER  

// [Patch 0302; Feature Recommended; Adaptive Sensitivity]34: FIELD_WR=ACCESS_CTL_STAT, 0x0001
{0x0982, 0x0001, 2}, 	// ACCESS_CTL_STAT
{0x098A, 0x512C, 2}, 	// PHYSICAL_ADDRESS_ACCESS
{0xD12C, 0x70CF, 2},
{0xD12E, 0xFFFF, 2},
{0xD130, 0xC5D4, 2},
{0xD132, 0x903A, 2},
{0xD134, 0x2144, 2},
{0xD136, 0x0C00, 2},
{0xD138, 0x2186, 2},
{0xD13A, 0x0FF3, 2},
{0xD13C, 0xB844, 2},
{0xD13E, 0x262F, 2},
{0xD140, 0xF008, 2},
{0xD142, 0xB948, 2},
{0xD144, 0x21CC, 2},
{0xD146, 0x8021, 2},
{0xD148, 0xD801, 2},
{0xD14A, 0xF203, 2},
{0xD14C, 0xD800, 2},
{0xD14E, 0x7EE0, 2},
{0xD150, 0xC0F1, 2},
{0xD152, 0x71CF, 2},
{0xD154, 0xFFFF, 2},
{0xD156, 0xC610, 2},
{0xD158, 0x910E, 2},
{0xD15A, 0x208C, 2},
{0xD15C, 0x8014, 2},
{0xD15E, 0xF418, 2},
{0xD160, 0x910F, 2},
{0xD162, 0x208C, 2},
{0xD164, 0x800F, 2},
{0xD166, 0xF414, 2},
{0xD168, 0x9116, 2},
{0xD16A, 0x208C, 2},
{0xD16C, 0x800A, 2},
{0xD16E, 0xF410, 2},
{0xD170, 0x9117, 2},
{0xD172, 0x208C, 2},
{0xD174, 0x8807, 2},
{0xD176, 0xF40C, 2},
{0xD178, 0x9118, 2},
{0xD17A, 0x2086, 2},
{0xD17C, 0x0FF3, 2},
{0xD17E, 0xB848, 2},
{0xD180, 0x080D, 2},
{0xD182, 0x0090, 2},
{0xD184, 0xFFEA, 2},
{0xD186, 0xE081, 2},
{0xD188, 0xD801, 2},
{0xD18A, 0xF203, 2},
{0xD18C, 0xD800, 2},
{0xD18E, 0xC0D1, 2},
{0xD190, 0x7EE0, 2},
{0xD192, 0x78E0, 2},
{0xD194, 0xC0F1, 2},
{0xD196, 0x71CF, 2},
{0xD198, 0xFFFF, 2},
{0xD19A, 0xC610, 2},
{0xD19C, 0x910E, 2},
{0xD19E, 0x208C, 2},
{0xD1A0, 0x800A, 2},
{0xD1A2, 0xF418, 2},
{0xD1A4, 0x910F, 2},
{0xD1A6, 0x208C, 2},
{0xD1A8, 0x8807, 2},
{0xD1AA, 0xF414, 2},
{0xD1AC, 0x9116, 2},
{0xD1AE, 0x208C, 2},
{0xD1B0, 0x800A, 2},
{0xD1B2, 0xF410, 2},
{0xD1B4, 0x9117, 2},
{0xD1B6, 0x208C, 2},
{0xD1B8, 0x8807, 2},
{0xD1BA, 0xF40C, 2},
{0xD1BC, 0x9118, 2},
{0xD1BE, 0x2086, 2},
{0xD1C0, 0x0FF3, 2},
{0xD1C2, 0xB848, 2},
{0xD1C4, 0x080D, 2},
{0xD1C6, 0x0090, 2},
{0xD1C8, 0xFFD9, 2},
{0xD1CA, 0xE080, 2},
{0xD1CC, 0xD801, 2},
{0xD1CE, 0xF203, 2},
{0xD1D0, 0xD800, 2},
{0xD1D2, 0xF1DF, 2},
{0xD1D4, 0x9040, 2},
{0xD1D6, 0x71CF, 2},
{0xD1D8, 0xFFFF, 2},
{0xD1DA, 0xC5D4, 2},
{0xD1DC, 0xB15A, 2},
{0xD1DE, 0x9041, 2},
{0xD1E0, 0x73CF, 2},
{0xD1E2, 0xFFFF, 2},
{0xD1E4, 0xC7D0, 2},
{0xD1E6, 0xB140, 2},
{0xD1E8, 0x9042, 2},
{0xD1EA, 0xB141, 2},
{0xD1EC, 0x9043, 2},
{0xD1EE, 0xB142, 2},
{0xD1F0, 0x9044, 2},
{0xD1F2, 0xB143, 2},
{0xD1F4, 0x9045, 2},
{0xD1F6, 0xB147, 2},
{0xD1F8, 0x9046, 2},
{0xD1FA, 0xB148, 2},
{0xD1FC, 0x9047, 2},
{0xD1FE, 0xB14B, 2},
{0xD200, 0x9048, 2},
{0xD202, 0xB14C, 2},
{0xD204, 0x9049, 2},
{0xD206, 0x1958, 2},
{0xD208, 0x0084, 2},
{0xD20A, 0x904A, 2},
{0xD20C, 0x195A, 2},
{0xD20E, 0x0084, 2},
{0xD210, 0x8856, 2},
{0xD212, 0x1B36, 2},
{0xD214, 0x8082, 2},
{0xD216, 0x8857, 2},
{0xD218, 0x1B37, 2},
{0xD21A, 0x8082, 2},
{0xD21C, 0x904C, 2},
{0xD21E, 0x19A7, 2},
{0xD220, 0x009C, 2},
{0xD222, 0x881A, 2},
{0xD224, 0x7FE0, 2},
{0xD226, 0x1B54, 2},
{0xD228, 0x8002, 2},
{0xD22A, 0x78E0, 2},
{0xD22C, 0x71CF, 2},
{0xD22E, 0xFFFF, 2},
{0xD230, 0xC350, 2},
{0xD232, 0xD828, 2},
{0xD234, 0xA90B, 2},
{0xD236, 0x8100, 2},
{0xD238, 0x01C5, 2},
{0xD23A, 0x0320, 2},
{0xD23C, 0xD900, 2},
{0xD23E, 0x78E0, 2},
{0xD240, 0x220A, 2},
{0xD242, 0x1F80, 2},
{0xD244, 0xFFFF, 2},
{0xD246, 0xD4E0, 2},
{0xD248, 0xC0F1, 2},
{0xD24A, 0x0811, 2},
{0xD24C, 0x0051, 2},
{0xD24E, 0x2240, 2},
{0xD250, 0x1200, 2},
{0xD252, 0xFFE1, 2},
{0xD254, 0xD801, 2},
{0xD256, 0xF006, 2},
{0xD258, 0x2240, 2},
{0xD25A, 0x1900, 2},
{0xD25C, 0xFFDE, 2},
{0xD25E, 0xD802, 2},
{0xD260, 0x1A05, 2},
{0xD262, 0x1002, 2},
{0xD264, 0xFFF2, 2},
{0xD266, 0xF195, 2},
{0xD268, 0xC0F1, 2},
{0xD26A, 0x0E7E, 2},
{0xD26C, 0x05C0, 2},
{0xD26E, 0x75CF, 2},
{0xD270, 0xFFFF, 2},
{0xD272, 0xC84C, 2},
{0xD274, 0x9502, 2},
{0xD276, 0x77CF, 2},
{0xD278, 0xFFFF, 2},
{0xD27A, 0xC344, 2},
{0xD27C, 0x2044, 2},
{0xD27E, 0x008E, 2},
{0xD280, 0xB8A1, 2},
{0xD282, 0x0926, 2},
{0xD284, 0x03E0, 2},
{0xD286, 0xB502, 2},
{0xD288, 0x9502, 2},
{0xD28A, 0x952E, 2},
{0xD28C, 0x7E05, 2},
{0xD28E, 0xB5C2, 2},
{0xD290, 0x70CF, 2},
{0xD292, 0xFFFF, 2},
{0xD294, 0xC610, 2},
{0xD296, 0x099A, 2},
{0xD298, 0x04A0, 2},
{0xD29A, 0xB026, 2},
{0xD29C, 0x0E02, 2},
{0xD29E, 0x0560, 2},
{0xD2A0, 0xDE00, 2},
{0xD2A2, 0x0A12, 2},
{0xD2A4, 0x0320, 2},
{0xD2A6, 0xB7C4, 2},
{0xD2A8, 0x0B36, 2},
{0xD2AA, 0x03A0, 2},
{0xD2AC, 0x70C9, 2},
{0xD2AE, 0x9502, 2},
{0xD2B0, 0x7608, 2},
{0xD2B2, 0xB8A8, 2},
{0xD2B4, 0xB502, 2},
{0xD2B6, 0x70CF, 2},
{0xD2B8, 0x0000, 2},
{0xD2BA, 0x5536, 2},
{0xD2BC, 0x7860, 2},
{0xD2BE, 0x2686, 2},
{0xD2C0, 0x1FFB, 2},
{0xD2C2, 0x9502, 2},
{0xD2C4, 0x78C5, 2},
{0xD2C6, 0x0631, 2},
{0xD2C8, 0x05E0, 2},
{0xD2CA, 0xB502, 2},
{0xD2CC, 0x72CF, 2},
{0xD2CE, 0xFFFF, 2},
{0xD2D0, 0xC5D4, 2},
{0xD2D2, 0x923A, 2},
{0xD2D4, 0x73CF, 2},
{0xD2D6, 0xFFFF, 2},
{0xD2D8, 0xC7D0, 2},
{0xD2DA, 0xB020, 2},
{0xD2DC, 0x9220, 2},
{0xD2DE, 0xB021, 2},
{0xD2E0, 0x9221, 2},
{0xD2E2, 0xB022, 2},
{0xD2E4, 0x9222, 2},
{0xD2E6, 0xB023, 2},
{0xD2E8, 0x9223, 2},
{0xD2EA, 0xB024, 2},
{0xD2EC, 0x9227, 2},
{0xD2EE, 0xB025, 2},
{0xD2F0, 0x9228, 2},
{0xD2F2, 0xB026, 2},
{0xD2F4, 0x922B, 2},
{0xD2F6, 0xB027, 2},
{0xD2F8, 0x922C, 2},
{0xD2FA, 0xB028, 2},
{0xD2FC, 0x1258, 2},
{0xD2FE, 0x0101, 2},
{0xD300, 0xB029, 2},
{0xD302, 0x125A, 2},
{0xD304, 0x0101, 2},
{0xD306, 0xB02A, 2},
{0xD308, 0x1336, 2},
{0xD30A, 0x8081, 2},
{0xD30C, 0xA836, 2},
{0xD30E, 0x1337, 2},
{0xD310, 0x8081, 2},
{0xD312, 0xA837, 2},
{0xD314, 0x12A7, 2},
{0xD316, 0x0701, 2},
{0xD318, 0xB02C, 2},
{0xD31A, 0x1354, 2},
{0xD31C, 0x8081, 2},
{0xD31E, 0x7FE0, 2},
{0xD320, 0xA83A, 2},
{0xD322, 0x78E0, 2},
{0xD324, 0xC0F1, 2},
{0xD326, 0x0DC2, 2},
{0xD328, 0x05C0, 2},
{0xD32A, 0x7608, 2},
{0xD32C, 0x09BB, 2},
{0xD32E, 0x0010, 2},
{0xD330, 0x75CF, 2},
{0xD332, 0xFFFF, 2},
{0xD334, 0xD4E0, 2},
{0xD336, 0x8D21, 2},
{0xD338, 0x8D00, 2},
{0xD33A, 0x2153, 2},
{0xD33C, 0x0003, 2},
{0xD33E, 0xB8C0, 2},
{0xD340, 0x8D45, 2},
{0xD342, 0x0B23, 2},
{0xD344, 0x0000, 2},
{0xD346, 0xEA8F, 2},
{0xD348, 0x0915, 2},
{0xD34A, 0x001E, 2},
{0xD34C, 0xFF81, 2},
{0xD34E, 0xE808, 2},
{0xD350, 0x2540, 2},
{0xD352, 0x1900, 2},
{0xD354, 0xFFDE, 2},
{0xD356, 0x8D00, 2},
{0xD358, 0xB880, 2},
{0xD35A, 0xF004, 2},
{0xD35C, 0x8D00, 2},
{0xD35E, 0xB8A0, 2},
{0xD360, 0xAD00, 2},
{0xD362, 0x8D05, 2},
{0xD364, 0xE081, 2},
{0xD366, 0x20CC, 2},
{0xD368, 0x80A2, 2},
{0xD36A, 0xDF00, 2},
{0xD36C, 0xF40A, 2},
{0xD36E, 0x71CF, 2},
{0xD370, 0xFFFF, 2},
{0xD372, 0xC84C, 2},
{0xD374, 0x9102, 2},
{0xD376, 0x7708, 2},
{0xD378, 0xB8A6, 2},
{0xD37A, 0x2786, 2},
{0xD37C, 0x1FFE, 2},
{0xD37E, 0xB102, 2},
{0xD380, 0x0B42, 2},
{0xD382, 0x0180, 2},
{0xD384, 0x0E3E, 2},
{0xD386, 0x0180, 2},
{0xD388, 0x0F4A, 2},
{0xD38A, 0x0160, 2},
{0xD38C, 0x70C9, 2},
{0xD38E, 0x8D05, 2},
{0xD390, 0xE081, 2},
{0xD392, 0x20CC, 2},
{0xD394, 0x80A2, 2},
{0xD396, 0xF429, 2},
{0xD398, 0x76CF, 2},
{0xD39A, 0xFFFF, 2},
{0xD39C, 0xC84C, 2},
{0xD39E, 0x082D, 2},
{0xD3A0, 0x0051, 2},
{0xD3A2, 0x70CF, 2},
{0xD3A4, 0xFFFF, 2},
{0xD3A6, 0xC90C, 2},
{0xD3A8, 0x8805, 2},
{0xD3AA, 0x09B6, 2},
{0xD3AC, 0x0360, 2},
{0xD3AE, 0xD908, 2},
{0xD3B0, 0x2099, 2},
{0xD3B2, 0x0802, 2},
{0xD3B4, 0x9634, 2},
{0xD3B6, 0xB503, 2},
{0xD3B8, 0x7902, 2},
{0xD3BA, 0x1523, 2},
{0xD3BC, 0x1080, 2},
{0xD3BE, 0xB634, 2},
{0xD3C0, 0xE001, 2},
{0xD3C2, 0x1D23, 2},
{0xD3C4, 0x1002, 2},
{0xD3C6, 0xF00B, 2},
{0xD3C8, 0x9634, 2},
{0xD3CA, 0x9503, 2},
{0xD3CC, 0x6038, 2},
{0xD3CE, 0xB614, 2},
{0xD3D0, 0x153F, 2},
{0xD3D2, 0x1080, 2},
{0xD3D4, 0xE001, 2},
{0xD3D6, 0x1D3F, 2},
{0xD3D8, 0x1002, 2},
{0xD3DA, 0xFFA4, 2},
{0xD3DC, 0x9602, 2},
{0xD3DE, 0x7F05, 2},
{0xD3E0, 0xD800, 2},
{0xD3E2, 0xB6E2, 2},
{0xD3E4, 0xAD05, 2},
{0xD3E6, 0x0511, 2},
{0xD3E8, 0x05E0, 2},
{0xD3EA, 0xD800, 2},
{0xD3EC, 0xC0F1, 2},
{0xD3EE, 0x0CFE, 2},
{0xD3F0, 0x05C0, 2},
{0xD3F2, 0x0A96, 2},
{0xD3F4, 0x05A0, 2},
{0xD3F6, 0x7608, 2},
{0xD3F8, 0x0C22, 2},
{0xD3FA, 0x0240, 2},
{0xD3FC, 0xE080, 2},
{0xD3FE, 0x20CA, 2},
{0xD400, 0x0F82, 2},
{0xD402, 0x0000, 2},
{0xD404, 0x190B, 2},
{0xD406, 0x0C60, 2},
{0xD408, 0x05A2, 2},
{0xD40A, 0x21CA, 2},
{0xD40C, 0x0022, 2},
{0xD40E, 0x0C56, 2},
{0xD410, 0x0240, 2},
{0xD412, 0xE806, 2},
{0xD414, 0x0E0E, 2},
{0xD416, 0x0220, 2},
{0xD418, 0x70C9, 2},
{0xD41A, 0xF048, 2},
{0xD41C, 0x0896, 2},
{0xD41E, 0x0440, 2},
{0xD420, 0x0E96, 2},
{0xD422, 0x0400, 2},
{0xD424, 0x0966, 2},
{0xD426, 0x0380, 2},
{0xD428, 0x75CF, 2},
{0xD42A, 0xFFFF, 2},
{0xD42C, 0xD4E0, 2},
{0xD42E, 0x8D00, 2},
{0xD430, 0x084D, 2},
{0xD432, 0x001E, 2},
{0xD434, 0xFF47, 2},
{0xD436, 0x080D, 2},
{0xD438, 0x0050, 2},
{0xD43A, 0xFF57, 2},
{0xD43C, 0x0841, 2},
{0xD43E, 0x0051, 2},
{0xD440, 0x8D04, 2},
{0xD442, 0x9521, 2},
{0xD444, 0xE064, 2},
{0xD446, 0x790C, 2},
{0xD448, 0x702F, 2},
{0xD44A, 0x0CE2, 2},
{0xD44C, 0x05E0, 2},
{0xD44E, 0xD964, 2},
{0xD450, 0x72CF, 2},
{0xD452, 0xFFFF, 2},
{0xD454, 0xC700, 2},
{0xD456, 0x9235, 2},
{0xD458, 0x0811, 2},
{0xD45A, 0x0043, 2},
{0xD45C, 0xFF3D, 2},
{0xD45E, 0x080D, 2},
{0xD460, 0x0051, 2},
{0xD462, 0xD801, 2},
{0xD464, 0xFF77, 2},
{0xD466, 0xF025, 2},
{0xD468, 0x9501, 2},
{0xD46A, 0x9235, 2},
{0xD46C, 0x0911, 2},
{0xD46E, 0x0003, 2},
{0xD470, 0xFF49, 2},
{0xD472, 0x080D, 2},
{0xD474, 0x0051, 2},
{0xD476, 0xD800, 2},
{0xD478, 0xFF72, 2},
{0xD47A, 0xF01B, 2},
{0xD47C, 0x0886, 2},
{0xD47E, 0x03E0, 2},
{0xD480, 0xD801, 2},
{0xD482, 0x0EF6, 2},
{0xD484, 0x03C0, 2},
{0xD486, 0x0F52, 2},
{0xD488, 0x0340, 2},
{0xD48A, 0x0DBA, 2},
{0xD48C, 0x0200, 2},
{0xD48E, 0x0AF6, 2},
{0xD490, 0x0440, 2},
{0xD492, 0x0C22, 2},
{0xD494, 0x0400, 2},
{0xD496, 0x0D72, 2},
{0xD498, 0x0440, 2},
{0xD49A, 0x0DC2, 2},
{0xD49C, 0x0200, 2},
{0xD49E, 0x0972, 2},
{0xD4A0, 0x0440, 2},
{0xD4A2, 0x0D3A, 2},
{0xD4A4, 0x0220, 2},
{0xD4A6, 0xD820, 2},
{0xD4A8, 0x0BFA, 2},
{0xD4AA, 0x0260, 2},
{0xD4AC, 0x70C9, 2},
{0xD4AE, 0x0451, 2},
{0xD4B0, 0x05C0, 2},
{0xD4B2, 0x78E0, 2},
{0xD4B4, 0xD900, 2},
{0xD4B6, 0xF00A, 2},
{0xD4B8, 0x70CF, 2},
{0xD4BA, 0xFFFF, 2},
{0xD4BC, 0xD520, 2},
{0xD4BE, 0x7835, 2},
{0xD4C0, 0x8041, 2},
{0xD4C2, 0x8000, 2},
{0xD4C4, 0xE102, 2},
{0xD4C6, 0xA040, 2},
{0xD4C8, 0x09F1, 2},
{0xD4CA, 0x8114, 2},
{0xD4CC, 0x71CF, 2},
{0xD4CE, 0xFFFF, 2},
{0xD4D0, 0xD4E0, 2},
{0xD4D2, 0x70CF, 2},
{0xD4D4, 0xFFFF, 2},
{0xD4D6, 0xC594, 2},
{0xD4D8, 0xB03A, 2},
{0xD4DA, 0x7FE0, 2},
{0xD4DC, 0xD800, 2},
{0xD4DE, 0x0000, 2},
{0xD4E0, 0x0000, 2},
{0xD4E2, 0x0500, 2},
{0xD4E4, 0x0500, 2},
{0xD4E6, 0x0200, 2},
{0xD4E8, 0x0330, 2},
{0xD4EA, 0x0000, 2},
{0xD4EC, 0x0000, 2},
{0xD4EE, 0x03CD, 2},
{0xD4F0, 0x050D, 2},
{0xD4F2, 0x01C5, 2},
{0xD4F4, 0x03B3, 2},
{0xD4F6, 0x00E0, 2},
{0xD4F8, 0x01E3, 2},
{0xD4FA, 0x0280, 2},
{0xD4FC, 0x01E0, 2},
{0xD4FE, 0x0109, 2},
{0xD500, 0x0080, 2},
{0xD502, 0x0500, 2},
{0xD504, 0x0000, 2},
{0xD506, 0x0000, 2},
{0xD508, 0x0000, 2},
{0xD50A, 0x0000, 2},
{0xD50C, 0x0000, 2},
{0xD50E, 0x0000, 2},
{0xD510, 0x0000, 2},
{0xD512, 0x0000, 2},
{0xD514, 0x0000, 2},
{0xD516, 0x0000, 2},
{0xD518, 0x0000, 2},
{0xD51A, 0x0000, 2},
{0xD51C, 0x0000, 2},
{0xD51E, 0x0000, 2},
{0xD520, 0xFFFF, 2},
{0xD522, 0xC9B4, 2},
{0xD524, 0xFFFF, 2},
{0xD526, 0xD324, 2},
{0xD528, 0xFFFF, 2},
{0xD52A, 0xCA34, 2},
{0xD52C, 0xFFFF, 2},
{0xD52E, 0xD3EC, 2},

{0x098E, 0x0000, 2}, 	// LOGICAL_ADDRESS_ACCESS
{0xE000, 0x04B4, 2}, 	// PATCHLDR_LOADER_ADDRESS
{0xE002, 0x0302, 2}, 	// PATCHLDR_PATCH_ID
//REG= 0xE004, 0x41030202 	// PATCHLDR_FIRMWARE_ID   
{0xE004, 0x4103, 2}, 	// PATCHLDR_FIRMWARE_ID
{0xE006, 0x0202, 2}, 	// PATCHLDR_FIRMWARE_ID
{0x0080, 0xFFF0, 2}, 	// COMMAND_REGISTER
//DELAY=50
{0, 50, 0},
{0x0080, 0xFFF1, 2}, 	// COMMAND_REGISTER
//DELAY=50
{0, 50, 0},

//[Step4-APGA] //LSC
//[//lens shading 90%]
{0x098E, 0x495E, 2}, 	 // LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]
{0xC95E, 0x0000, 2}, 	 // CAM_PGA_PGA_CONTROL    
{0x3640, 0x0350, 2},
{0x3642, 0x0B8C, 2},
{0x3644, 0x1A51, 2},
{0x3646, 0x9A4C, 2},
{0x3648, 0x938F, 2},
{0x364A, 0x7F8F, 2},
{0x364C, 0x1F4C, 2},
{0x364E, 0x2BF1, 2},
{0x3650, 0x290B, 2},
{0x3652, 0xA20E, 2},
{0x3654, 0x0130, 2},
{0x3656, 0x36AC, 2},
{0x3658, 0x5E30, 2},
{0x365A, 0x56EC, 2},
{0x365C, 0xD72D, 2},
{0x365E, 0x0030, 2},
{0x3660, 0x594B, 2},
{0x3662, 0x1EF1, 2},
{0x3664, 0xF4EB, 2},
{0x3666, 0xBCAF, 2},
{0x3680, 0x8A2B, 2},
{0x3682, 0xEACB, 2},
{0x3684, 0x196E, 2},
{0x3686, 0x63CD, 2},
{0x3688, 0xA16E, 2},
{0x368A, 0xDFEB, 2},
{0x368C, 0x972C, 2},
{0x368E, 0x6DCC, 2},
{0x3690, 0x27CC, 2},
{0x3692, 0xA44D, 2},
{0x3694, 0x45CC, 2},
{0x3696, 0x9A2D, 2},
{0x3698, 0xEA6E, 2},
{0x369A, 0xBEAA, 2},
{0x369C, 0x324F, 2},
{0x369E, 0x07AB, 2},
{0x36A0, 0xEECC, 2},
{0x36A2, 0x89AE, 2},
{0x36A4, 0x268D, 2},
{0x36A6, 0x01EE, 2},
{0x36C0, 0x3A71, 2},
{0x36C2, 0x83CF, 2},
{0x36C4, 0xAB92, 2},
{0x36C6, 0x20F1, 2},
{0x36C8, 0x0E74, 2},
{0x36CA, 0x5391, 2},
{0x36CC, 0x876E, 2},
{0x36CE, 0xF091, 2},
{0x36D0, 0x1130, 2},
{0x36D2, 0x4893, 2},
{0x36D4, 0x1491, 2},
{0x36D6, 0xA9AE, 2},
{0x36D8, 0xCC32, 2},
{0x36DA, 0x72B0, 2},
{0x36DC, 0x25B4, 2},
{0x36DE, 0x3871, 2},
{0x36E0, 0x954F, 2},
{0x36E2, 0xABD2, 2},
{0x36E4, 0x1BD1, 2},
{0x36E6, 0x0CB4, 2},
{0x3700, 0x37EE, 2},
{0x3702, 0x5B4E, 2},
{0x3704, 0x7BEF, 2},
{0x3706, 0xB830, 2},
{0x3708, 0xA7D2, 2},
{0x370A, 0x516C, 2},
{0x370C, 0x26EF, 2},
{0x370E, 0x622E, 2},
{0x3710, 0x8811, 2},
{0x3712, 0xFE90, 2},
{0x3714, 0x4AAD, 2},
{0x3716, 0x1BEF, 2},
{0x3718, 0x3AEF, 2},
{0x371A, 0x8C90, 2},
{0x371C, 0x9C32, 2},
{0x371E, 0x158F, 2},
{0x3720, 0x250E, 2},
{0x3722, 0x0290, 2},
{0x3724, 0x83CE, 2},
{0x3726, 0x9052, 2},
{0x3740, 0xE7B0, 2},
{0x3742, 0x6250, 2},
{0x3744, 0x17F4, 2},
{0x3746, 0xAA53, 2},
{0x3748, 0xD795, 2},
{0x374A, 0x8010, 2},
{0x374C, 0x504F, 2},
{0x374E, 0x6C53, 2},
{0x3750, 0xCC12, 2},
{0x3752, 0xC9D5, 2},
{0x3754, 0x9D10, 2},
{0x3756, 0x5110, 2},
{0x3758, 0x4D34, 2},
{0x375A, 0x9CB3, 2},
{0x375C, 0x8356, 2},
{0x375E, 0x83D1, 2},
{0x3760, 0x5E30, 2},
{0x3762, 0x1E34, 2},
{0x3764, 0xA693, 2},
{0x3766, 0xDE15, 2},
{0x3782, 0x01B0, 2},
{0x3784, 0x0280, 2},
{0x37C0, 0x9C4A, 2},
{0x37C2, 0x8EAA, 2},
{0x37C4, 0xC22B, 2},
{0x37C6, 0xFFE9, 2},
{0xC95E, 0x0000, 2}, 	 // CAM_PGA_PGA_CONTROL                
{0xC95E, 0x0001, 2}, 	 // CAM_PGA_PGA_CONTROL 

//[Step5-AWB_CCM]
//[Color Correction Matrices 06/04/11 15:35:24]
{0x098E, 0x0000, 2}, // LOGICAL_ADDRESS_ACCESS
{0xC892, 0x0220, 2}, // CAM_AWB_CCM_L_0
{0xC894, 0xFEB3, 2}, // CAM_AWB_CCM_L_1
{0xC896, 0x002D, 2}, // CAM_AWB_CCM_L_2
{0xC898, 0xFFAD, 2}, // CAM_AWB_CCM_L_3
{0xC89A, 0x014E, 2}, // CAM_AWB_CCM_L_4
{0xC89C, 0x0005, 2}, // CAM_AWB_CCM_L_5
{0xC89E, 0xFF5D, 2}, // CAM_AWB_CCM_L_6
{0xC8A0, 0xFEC8, 2}, // CAM_AWB_CCM_L_7
{0xC8A2, 0x02DB, 2}, // CAM_AWB_CCM_L_8
{0xC8C8, 0x006A, 2}, // CAM_AWB_CCM_L_RG_GAIN
{0xC8CA, 0x0119, 2}, // CAM_AWB_CCM_L_BG_GAIN
{0xC8A4, 0x01F9, 2}, // CAM_AWB_CCM_M_0
{0xC8A6, 0xFED6, 2}, // CAM_AWB_CCM_M_1
{0xC8A8, 0x0031, 2}, // CAM_AWB_CCM_M_2
{0xC8AA, 0xFFD3, 2}, // CAM_AWB_CCM_M_3
{0xC8AC, 0x0130, 2}, // CAM_AWB_CCM_M_4
{0xC8AE, 0xFFFE, 2}, // CAM_AWB_CCM_M_5
{0xC8B0, 0xFFC4, 2}, // CAM_AWB_CCM_M_6
{0xC8B2, 0xFF0B, 2}, // CAM_AWB_CCM_M_7
{0xC8B4, 0x0231, 2}, // CAM_AWB_CCM_M_8
{0xC8CC, 0x0094, 2}, // CAM_AWB_CCM_M_RG_GAIN
{0xC8CE, 0x00FE, 2}, // CAM_AWB_CCM_M_BG_GAIN
{0xC8B6, 0x0203, 2}, // CAM_AWB_CCM_R_0
{0xC8B8, 0xFF62, 2}, // CAM_AWB_CCM_R_1
{0xC8BA, 0xFF9C, 2}, // CAM_AWB_CCM_R_2
{0xC8BC, 0xFF93, 2}, // CAM_AWB_CCM_R_3
{0xC8BE, 0x018D, 2}, // CAM_AWB_CCM_R_4
{0xC8C0, 0xFFE0, 2}, // CAM_AWB_CCM_R_5
{0xC8C2, 0xFFDC, 2}, // CAM_AWB_CCM_R_6
{0xC8C4, 0xFF54, 2}, // CAM_AWB_CCM_R_7
{0xC8C6, 0x01D0, 2}, // CAM_AWB_CCM_R_8
{0xC8D0, 0x00A9, 2}, // CAM_AWB_CCM_R_RG_GAIN
{0xC8D2, 0x008C, 2}, // CAM_AWB_CCM_R_BG_GAIN
{0xC8D4, 0x09C4, 2}, // CAM_AWB_CCM_L_CTEMP
{0xC8D6, 0x0D67, 2}, // CAM_AWB_CCM_M_CTEMP
{0xC8D8, 0x1964, 2}, // CAM_AWB_CCM_R_CTEMP
{0xC8F2, 0x0003, 2}, // CAM_AWB_AWB_XSCALE
{0xC8F3, 0x0002, 2}, // CAM_AWB_AWB_YSCALE
{0xC8F4, 0xBFFD, 2}, // CAM_AWB_AWB_WEIGHTS_0
{0xC8F6, 0xD824, 2}, // CAM_AWB_AWB_WEIGHTS_1
{0xC8F8, 0x175E, 2}, // CAM_AWB_AWB_WEIGHTS_2
{0xC8FA, 0x3C8B, 2}, // CAM_AWB_AWB_WEIGHTS_3
{0xC8FC, 0x29F0, 2}, // CAM_AWB_AWB_WEIGHTS_4
{0xC8FE, 0x5363, 2}, // CAM_AWB_AWB_WEIGHTS_5
{0xC900, 0x5DAA, 2}, // CAM_AWB_AWB_WEIGHTS_6
{0xC902, 0x8A5A, 2}, // CAM_AWB_AWB_WEIGHTS_7
{0xC904, 0x003D, 2}, // CAM_AWB_AWB_XSHIFT_PRE_ADJ
{0xC906, 0x0033, 2}, // CAM_AWB_AWB_YSHIFT_PRE_ADJ

//[Step7-CPIPE_Preference]
{0x098E, 0x4926, 2}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_START_BRIGHTNESS]
{0xC926, 0x0020, 2}, 	// CAM_LL_START_BRIGHTNESS
{0xC928, 0x009A, 2}, 	// CAM_LL_STOP_BRIGHTNESS
{0xC946, 0x0070, 2}, 	// CAM_LL_START_GAIN_METRIC
{0xC948, 0x00F3, 2}, 	// CAM_LL_STOP_GAIN_METRIC
{0xC952, 0x0020, 2}, 	// CAM_LL_START_TARGET_LUMA_BM
{0xC954, 0x009A, 2}, 	// CAM_LL_STOP_TARGET_LUMA_BM
{0xC92A, 0x80,   1}, 	// CAM_LL_START_SATURATION
{0xC92B, 0x4B,   1}, 	// CAM_LL_END_SATURATION
{0xC92C, 0x00,   1}, 	// CAM_LL_START_DESATURATION
{0xC92D, 0xFF,   1}, 	// CAM_LL_END_DESATURATION
{0xC92E, 0x3C,   1}, 	// CAM_LL_START_DEMOSAIC
{0xC92F, 0x02,   1}, 	// CAM_LL_START_AP_GAIN
{0xC930, 0x06,   1}, 	// CAM_LL_START_AP_THRESH
{0xC931, 0x64,   1}, 	// CAM_LL_STOP_DEMOSAIC
{0xC932, 0x01,   1}, 	// CAM_LL_STOP_AP_GAIN
{0xC933, 0x0C,   1}, 	// CAM_LL_STOP_AP_THRESH
{0xC934, 0x3C,   1}, 	// CAM_LL_START_NR_RED
{0xC935, 0x3C,   1}, 	// CAM_LL_START_NR_GREEN
{0xC936, 0x3C,   1}, 	// CAM_LL_START_NR_BLUE
{0xC937, 0x0F,   1}, 	// CAM_LL_START_NR_THRESH
{0xC938, 0x64,   1}, 	// CAM_LL_STOP_NR_RED
{0xC939, 0x64,   1}, 	// CAM_LL_STOP_NR_GREEN
{0xC93A, 0x64,   1}, 	// CAM_LL_STOP_NR_BLUE
{0xC93B, 0x32,   1}, 	// CAM_LL_STOP_NR_THRESH
{0xC93C, 0x0020, 2}, 	// CAM_LL_START_CONTRAST_BM
{0xC93E, 0x009A, 2}, 	// CAM_LL_STOP_CONTRAST_BM
{0xC940, 0x00DC, 2}, 	// CAM_LL_GAMMA
{0xC942, 0x38,   1}, 	// CAM_LL_START_CONTRAST_GRADIENT
{0xC943, 0x30,   1}, 	// CAM_LL_STOP_CONTRAST_GRADIENT
{0xC944, 0x50,   1}, 	// CAM_LL_START_CONTRAST_LUMA_PERCENTAGE
{0xC945, 0x19,   1}, 	// CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE
{0xC94A, 0x0230, 2}, 	// CAM_LL_START_FADE_TO_BLACK_LUMA
{0xC94C, 0x0010, 2}, 	// CAM_LL_STOP_FADE_TO_BLACK_LUMA
{0xC94E, 0x01CD, 2}, 	// CAM_LL_CLUSTER_DC_TH_BM
{0xC950, 0x05,   1}, 	// CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
{0xC951, 0x40,   1}, 	// CAM_LL_SUMMING_SENSITIVITY_FACTOR
{0xC87B, 0x1B,   1}, 	// CAM_AET_TARGET_AVERAGE_LUMA_DARK
{0xC878, 0x0E,   1}, 	// CAM_AET_AEMODE
{0xC890, 0x0080, 2}, 	// CAM_AET_TARGET_GAIN
{0xC81C, 0x0100, 2}, 	// CAM_SENSOR_CFG_MAX_ANALOG_GAIN
{0xC87C, 0x005A, 2}, 	// CAM_AET_BLACK_CLIPPING_TARGET
{0xB42A, 0x05,   1}, 	// CCM_DELTA_GAIN
{0xA80A, 0x20,   1}, 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED

//[Step8-Features]
{0xC984, 0x8041, 2}, 	// CAM_PORT_OUTPUT_CONTROL
{0xC988, 0x0F00, 2}, 	// CAM_PORT_MIPI_TIMING_T_HS_ZERO
{0xC98A, 0x0B07, 2}, 	// CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL
{0xC98C, 0x0D01, 2}, 	// CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE
{0xC98E, 0x071D, 2}, 	// CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO
{0xC990, 0x0006, 2}, 	// CAM_PORT_MIPI_TIMING_T_LPX
{0xC992, 0x0A0C, 2}, 	// CAM_PORT_MIPI_TIMING_INIT_TIMING

//[Anti-Flicker for MT9M114][50Hz]
{0x098E, 0xC88B, 2}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_FLICKER_FREQ_HZ]
{0xC88B, 0x32,   1}, 	// CAM_AET_FLICKER_FREQ_HZ

//[Change-Config]
{0x098E, 0xDC00, 2}, 	// LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE]
{0xDC00, 0x28,   1}, 	// SYSMGR_NEXT_STATE
{0x0080, 0x8002, 2}, 	// COMMAND_REGISTER
//DELAY=100
{0, 100, 0},


};


struct mt9m114_work_t {
	struct work_struct work;
};
static struct  mt9m114_work_t *mt9m114_sensorw;
static struct  i2c_client *mt9m114_client;
struct mt9m114_ctrl_t {
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
	enum mt9m114_resolution_t prev_res;
	enum mt9m114_resolution_t pict_res;
	enum mt9m114_resolution_t curr_res;
	enum mt9m114_test_mode_t  set_test;
	unsigned short imgaddr;
};
static struct mt9m114_ctrl_t *mt9m114_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(mt9m114_wait_queue);
DEFINE_MUTEX(mt9m114_mut);

/*=============================================================*/
static int mt9m114_i2c_rxdata(unsigned short saddr,
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
	if (i2c_transfer(mt9m114_client->adapter, msgs, 2) < 0) {
		printk("mt9m114_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}
static int32_t mt9m114_i2c_txdata(unsigned short saddr,
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
	if (i2c_transfer(mt9m114_client->adapter, msg, 1) < 0) {
		printk("mt9m114_i2c_txdata faild 0x%x\n", mt9m114_client->addr);
		return -EIO;
	}

	return 0;
}

static int32_t mt9m114_i2c_read(unsigned short raddr,
	unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = mt9m114_i2c_rxdata(mt9m114_client->addr, buf, 2);
	if (rc < 0) {
		printk("mt9m114_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = buf[0] << 8 | buf[1];
	return rc;
}
static int32_t mt9m114_i2c_write_b_sensor(unsigned short waddr, unsigned short bdata,  int width)
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

		rc = mt9m114_i2c_txdata(mt9m114_client->addr, buf, 4);
	}
		break;

	case BYTE_LEN: {
	
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (bdata & 0x00FF);

		rc = mt9m114_i2c_txdata(mt9m114_client->addr, buf, 3);
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
static int32_t mt9m114_enter_standby(void)
{
    int32_t rc = 0;
    uint16_t model_id=0xFFFF;
    uint16_t timeout = 0;
    
    printk("%s----begin\n",__FUNCTION__);
    printk("1 here ==================================================\n");
    //Step 1========================================================
    rc = mt9m114_i2c_write_b_sensor(0x098E, 0xDC00, 2);
    if(rc < 0)
        goto enter_standby_fail;
    //0xDC00,write 0x50;
    rc = mt9m114_i2c_write_b_sensor(0xDC00,0x50,1);
    if(rc < 0)
        goto enter_standby_fail;

    printk("2 here ==================================================\n");
    //Step 2========================================================
    //read 0x0080,bit1 ? R : W.
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_read(0x0080, &model_id);
        model_id = ((model_id>> 1) & 0x0001);
        if(0x0000 == model_id)
        {
            printk("%s: --1 Successful -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            printk("%s: --1 Successful -- timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --1 Failed -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            goto enter_standby_fail;
        }
    }

    printk("3 here ==================================================\n");
    //Step 3========================================================
    rc = mt9m114_i2c_write_b_sensor(0x0080,0x8002,2);
    if(rc < 0)
        goto enter_standby_fail;
    
    printk("4 here ==================================================\n");
    //Step 4========================================================
    //read 0x0080,bit1 ? R : W.
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_read(0x0080, &model_id);
        model_id = ((model_id >> 1) & 0x0001);
        if(0x0000 == model_id)
        {
            printk("%s: --2 Successful -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            printk("%s: --2 Successful -- timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --2 Failed -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            goto enter_standby_fail;
        }
    }
    
    printk("5 here ==================================================\n");
    //Step 5========================================================  
    //read 0x0080,bit15 ? R : W.
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_read(0x0080, &model_id);
        model_id = ((model_id >> 15) & 0x0001);
        if(0x0001 == model_id)
        {
            printk("%s: --3 Successful -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            printk("%s: --3 Successful -- timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --3 Failed -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            goto enter_standby_fail;
        }
    }
    printk("6 here ==================================================\n");
    //Step 6========================================================
    //read 0xDC01,0x52?
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_write_b_sensor(0x098E, 0xDC01, 2);
        mt9m114_i2c_read(0xDC01, &model_id);
        model_id = ((model_id >> 8) & 0x00FF);
        if(0x52 == model_id)
        {
            printk("%s: --4 Successful -- 0xDC01 = 0x%02x\n",__FUNCTION__,model_id);
            printk("%s: --4 Successful -- timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }

        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --4 Failed -- 0xDC01 = 0x%02x\n",__FUNCTION__,model_id);
            goto enter_standby_fail;
        }
    }

    //All the register is right,then set MT9M114_ENTER_STANDBY_FLAG to be 0x0001. 
    MT9M114_ENTER_STANDBY_FLAG = 0x0001;    
    printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    printk("%s----end\n",__FUNCTION__);
    return rc;

enter_standby_fail:
    printk("*************************************************\n");
    printk("*************************************************\n");
    printk("*************************************************\n");
    printk("%s:Failed !\n",__FUNCTION__);
    return rc;
}
static int32_t mt9m114_exit_standby(void)
{
    int32_t rc = 0;
    uint16_t model_id=0xFFFF;
    uint16_t timeout = 0;
    printk("%s----begin\n",__FUNCTION__);
    //Step 1========================================================
    printk("1 here ==================================================\n");
    rc = mt9m114_i2c_write_b_sensor(0x098E, 0xDC00, 2);
    if(rc < 0)
        goto exit_standby_fail;
    
    //0xDC00,write 0x54;
    rc = mt9m114_i2c_write_b_sensor(0xDC00,0x54,1);
    if(rc < 0)
        goto exit_standby_fail;
    
    //Step 2========================================================
    printk("2 here ==================================================\n");
    //read 0x0080,bit1 ? R : W.
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_read(0x0080, &model_id);
        model_id = ((model_id>> 1) & 0x0001);
        if(0x0000 == model_id)
        {
            printk("%s --1Successful --:0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            printk("%s --1Successful --:timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --1 Failed -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            goto exit_standby_fail;
        }

    }

    //Step 3========================================================
    printk("3 here ==================================================\n");
    rc = mt9m114_i2c_write_b_sensor(0x0080,0x8002,2);
    if(rc < 0)
        goto exit_standby_fail;
    
    //Step 4========================================================
    printk("4 here ==================================================\n");    
    //read 0x0080,bit1 ? R : W.
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_read(0x0080, &model_id);
        model_id = ((model_id >> 1) & 0x0001);
        if(0x0000 == model_id)
        {
            printk("%s --2Successful --:0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            printk("%s --2Successful --:timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --2 Failed -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            goto exit_standby_fail;
        }
    }

    //Step 5========================================================
    printk("5 here ==================================================\n");
    //read 0x0080,bit15 ? R : W.
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_read(0x0080, &model_id);
        model_id = ((model_id >> 15) & 0x0001);
        if(0x0001 == model_id)
        {
            printk("%s --3Successful --:0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            printk("%s --3Successful --:timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }
        
        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --3 Failed -- 0x0080 = 0x%04x\n",__FUNCTION__,model_id);
            goto exit_standby_fail;
        }
    }
    
    //Step 6========================================================
    printk("6 here ==================================================\n");
    //read 0xDC01,0x52?
    for(timeout = 0;timeout < STANDBY_TIMEOUT + 10;timeout += 10)
    {
        mt9m114_i2c_write_b_sensor(0x098E, 0xDC01, 2);
        mt9m114_i2c_read(0xDC01, &model_id);
        model_id = ((model_id >> 8) & 0x00FF);
        if(0x31 == model_id)
        {
            printk("%s --4Successful --:0xDC01 = 0x%02x  \n",__FUNCTION__,model_id);
            printk("%s --4Successful --:timeout = %d\n",__FUNCTION__,timeout+10);
            break;
        }
        else
        {
            msleep(10);
        }

        if(timeout >= STANDBY_TIMEOUT)
        {
            printk("%s: --4 Failed -- 0xDC01 = 0x%02x\n",__FUNCTION__,model_id);
            goto exit_standby_fail;
        }
    }

    //All the register state is right,then set MT9M114_ENTER_STANDBY_FLAG to be 0x0001. 
    MT9M114_EXIT_STANDBY_FLAG = 0x0001;
    printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    printk("%s----end\n",__FUNCTION__);
    return rc;
    
exit_standby_fail:
    printk("*************************************************\n");
    printk("*************************************************\n");
    printk("*************************************************\n");    
    printk("%s:Failed !\n",__FUNCTION__);
    return rc;
}
static int32_t mt9m114_sensor_setting(int update_type, int rt)
{
    int32_t i, array_length;
    int32_t time_out;
    uint16_t model_id, tmp;
    int32_t rc = 0;
    
    struct msm_camera_csi_params mt9m114_csi_params;
    printk("mt9m114_sensor_setting----------begin\n");
    switch (update_type) {
    case REG_INIT:

        printk("mt9m114_sensor_setting---REG_INIT-------begin\n");        

        CSI_CONFIG_MT9M114 = 0;
        Sensor_CONFIG_MT9M114 = 0;
        printk("mt9m114_sensor_setting --- REG_INIT-------end\n");

        //return rc;
        break;
    case UPDATE_PERIODIC:
        if (!CSI_CONFIG_MT9M114)
        {
            printk(":  %s--------The first time setting CSI-----------begin \n", __func__);        
            mt9m114_csi_params.lane_cnt = 1;
            mt9m114_csi_params.data_format = CSI_8BIT;
            mt9m114_csi_params.lane_assign = 0xe4;
            mt9m114_csi_params.dpcm_scheme = 0;
            mt9m114_csi_params.settle_cnt = 0x14; 
            rc = msm_camio_csi_config(&mt9m114_csi_params);
            msleep(10);
            CSI_CONFIG_MT9M114 = 1;
            printk(":  %s--------The first time setting CSI-----------end \n", __func__);    
        }
        if (rt == RES_PREVIEW)
        {
        /*1-**********  mt9m114 sensor initialization **************/
        printk("mt9m114_sensor_setting --- configure MT9M114 ---\n");
        if(!Sensor_CONFIG_MT9M114)
        {
            printk("mt9p111_sensor_setting--setting preview---begin\n");
            //If first time enter the preview mode,running here.
            //If capture to preview,goto else.Config the sensor to be capture to preview.
            Sensor_CONFIG_MT9M114 = 1;
                
            //if MT9M114 enter software standby failed or initialization the register failed,
            //the system must initialization the sensor again,and write all the regiaters.
            if( 0x0000 == MT9M114_ENTER_STANDBY_FLAG 
            || 0x0000 == MT9M114_INIT_FLAG
            || 0x0000 == MT9M114_EXIT_STANDBY_FLAG)
            {
                printk("MT9M114_ENTER_STANDBY_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9M114_ENTER_STANDBY_FLAG);
                printk("MT9M114_INIT_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9M114_INIT_FLAG);
                printk("MT9M114_EXIT_STANDBY_FLAG = %d  Shit!It's running as slow as a snail!\n",MT9M114_EXIT_STANDBY_FLAG);
                    
                rc = mt9m114_i2c_write_b_sensor(0x001A, 0x0001, 2);
                rc = mt9m114_i2c_write_b_sensor(0x001A, 0x0000, 2);

                for (time_out = 0; time_out <110; time_out += 10 )
                {
                    mt9m114_i2c_read(0x0080, &model_id);
                    model_id = (model_id & 0x0002) >> 1;
                    printk("reg = 0x0080,  bit 1 value  = 0x%x\n",  model_id);
                    if (model_id != 0)    mdelay(10);
                    else     break;
                }
                printk("reg = 0x0080 take effect,  timeout = 0x%d\n",  time_out);
                if (time_out > 100) printk("reset failed because of timeout(>100 ms)");

                mt9m114_i2c_read(0x301A, &model_id);
                printk("reg = 0x301A,   value  = 0x%x\n",  model_id);
                tmp = (model_id & 0x0200) >> 9;
                printk("reg = 0x301A,  bit 9 value  = 0x%x\n",  tmp);
                printk("setting  = 0x301A,  bit 9 value  = 1\n");
                model_id = model_id | 0x0200;
                printk("wrint 0x301A:  reg = 0x301A,   value  = 0x%x\n",  model_id);
                rc = mt9m114_i2c_write_b_sensor(0x301A, model_id, 2);

                mt9m114_i2c_read(0x301A, &model_id);
                printk("after setting reg = 0x301A,   value  = 0x%x\n",  model_id);
            
                array_length = sizeof(mt9m114_init_settings_array) /
                    sizeof(mt9m114_init_settings_array[0]);
                for (i = 0; i < array_length; i++)
                {
                    rc = mt9m114_i2c_write_b_sensor(
                        mt9m114_init_settings_array[i].reg_addr,
                        mt9m114_init_settings_array[i].reg_val, 
                        mt9m114_init_settings_array[i].lens);
                
                    if (rc < 0) return rc;

                    if(mt9m114_init_settings_array[i].reg_addr != 0)
                    {
                        if(mt9m114_init_settings_array[i].reg_addr == 0x0080)
                        {
                            for (time_out = 0; time_out < 700; time_out += 10 )
                            {
                                mt9m114_i2c_read(0x0080, &model_id);
                                tmp = (model_id & 0x8000) >> 15;
                                printk("reg = 0x0080,  bit 15 value  = 0x%x\n",  tmp);
                                if (tmp != 1)    mdelay(10);
                                else    break;
                            }
                            printk("reg = 0x0080 take effect,  timeout = %d ms\n",  time_out+10);

                            mt9m114_i2c_write_b_sensor(0x098E, 0xDC00, 2);
                            mt9m114_i2c_read(0xDC00, &model_id);                        
                            printk("after write 0x0080 ,addr=0xDC00, value = 0x%x, \n",model_id);
                            mt9m114_i2c_write_b_sensor(0x098E, 0xDC01, 2);
                            mt9m114_i2c_read(0xDC01, &model_id);
                            printk("after write 0x0080 ,addr=0xDC01, value = 0x%x, \n",model_id);
                          }
                    }
                }
                MT9M114_INIT_FLAG = 0x0001;
#if 0
                /*2-********** mt9m114 entry standby mode **************/
                mt9m114_i2c_write_b_sensor(0x098E, 0xDC00, 2);
                mt9m114_i2c_write_b_sensor(0x0990, 0x5050, 2);
                mt9m114_i2c_write_b_sensor(0x0080, 0x8002, 2);
                mdelay(100);

                /*3-********** check mt9m114 standby mode **************/
                mt9m114_i2c_write_b_sensor(0x098E, 0xDC00, 2);
                mt9m114_i2c_read(0xDC00, &model_id);
                printk("Entry standby mode, reg = 0xDC00, value  = 0x%x\n",  model_id);
                mt9m114_i2c_write_b_sensor(0x098E, 0xDC01, 2);
                mt9m114_i2c_read(0xDC01, &model_id);
                printk("Entry standby mode, reg = 0xDC01, value  = 0x%x\n",  model_id);

                /*4-**********Init CSI for the first time **************/
                if (!CSI_CONFIG_MT9M114) {
                printk(":  %s--------The first time setting CSI-----------begin \n", __func__);        
                mt9m114_csi_params.lane_cnt = 1;
                mt9m114_csi_params.data_format = CSI_8BIT;
                mt9m114_csi_params.lane_assign = 0xe4;
                mt9m114_csi_params.dpcm_scheme = 0;
                mt9m114_csi_params.settle_cnt = 0x14; 
                rc = msm_camio_csi_config(&mt9m114_csi_params);
                msleep(10);
                CSI_CONFIG_MT9M114 = 1;
                printk(":  %s--------The first time setting CSI-----------end \n", __func__);    
                } 

                /*5-**********Wake up mt9m114 from standby mode **************/
                mt9m114_i2c_write_b_sensor(0x098E, 0xDC00, 2);
                mt9m114_i2c_write_b_sensor(0x0990, 0x5454, 2);
                mt9m114_i2c_write_b_sensor(0x0080, 0x8002, 2);
                mdelay(100);
#endif
                Sensor_CONFIG_MT9M114 = 1;
                return rc;
            }
            else
            {
                printk("MT9M114_ENTER_STANDBY_FLAG = %d  Congratulations!It's running so fast!\n",MT9M114_ENTER_STANDBY_FLAG);
                printk("MT9M114_INIT_FLAG = %d  Congratulations!It's running so fast!\n",MT9M114_INIT_FLAG);
                printk("MT9M114_EXIT_STANDBY_FLAG = %d  Congratulations!It's running so fast! \n",MT9M114_EXIT_STANDBY_FLAG);
                mt9m114_exit_standby();
            }
        }
        else
        {
            msleep(10);
        }
    }
    if(rt == RES_CAPTURE)
    {    
        printk("WAC:  %s--------snapshot configuration--no setting----  \n", __func__);    
        mdelay(10);
    }    
        break;
    default:
        rc = -EINVAL;
        break;
    }
    printk("mt9m114_sensor_setting----------end\n");
    return rc;
}
static int32_t mt9m114_video_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/* change sensor resolution if needed */
	rt = RES_PREVIEW;

	if (mt9m114_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	mt9m114_ctrl->curr_res = mt9m114_ctrl->prev_res;
	mt9m114_ctrl->sensormode = mode;
	return rc;
}

static int32_t mt9m114_set_sensor_mode(int mode,
	int res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9m114_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static int32_t mt9m114_power_down(void)
{
	printk("mt9m114_power_down\n");
	return 0;
}
static int mt9m114_probe_init_done(const struct msm_camera_sensor_info *data)
{
    int rc = 0;
    printk("%s:--begin!\n",__FUNCTION__);
    rc = gpio_request(data->sensor_reset, "mt9m114");
    if (!rc)
    {
        printk("sensor_reset = %d\n", rc);
        gpio_direction_output(data->sensor_reset, 0);
        msleep(20);
        gpio_set_value_cansleep(data->sensor_reset, 1);
        msleep(20);
    }
    else
    {
        printk("gpio reset fail\n");
        goto init_probe_done;
    }
    gpio_direction_output(data->sensor_reset, 0);
    gpio_free(data->sensor_reset);
    return rc;
init_probe_done:
    printk("%s:--Failed!",__FUNCTION__);
    return rc;
}
static int mt9m114_probe_open_sensor(void)
{
    uint16_t model_id;
    int32_t rc = 0;
    
    printk("%s-----------begin\n",__FUNCTION__);
    
    //Read sensor Model ID: 
    rc = mt9m114_i2c_read(REG_MT9M114_MODEL_ID, &model_id);
    if (rc < 0)
        goto init_probe_fail;
    
    printk("mt9m114 model_id = 0x%x\n", model_id);
    //Compare sensor ID to MT9M114 ID: 
    if (model_id != MT9M114_MODEL_ID)
    {
        rc = -ENODEV;
        goto init_probe_fail;
    }
    else
    {
        printk("%s-----------Module ID is wrong\n",__FUNCTION__);
        goto init_probe_done;
    }

init_probe_fail:
    printk("%s-----------Failed!\n",__FUNCTION__);
    return rc;
init_probe_done:
    printk("%s-----------End!--Successful!\n",__FUNCTION__);
    printk("mt9m114_probe_open_sensor finishes\n");
    return rc;
}
static int mt9m114_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
    uint16_t model_id;
    int32_t rc = 0;
    
    printk("mt9m114_probe_init_sensor-----------begin\n");
    
    //printk("data->sensor_pwd = %d\n", data->sensor_pwd);
    printk("data->sensor_reset = %d\n", data->sensor_reset);
    rc = gpio_request(data->sensor_reset, "mt9m114");
    if (!rc)
    {
        printk("sensor_reset = %d\n", rc);
        gpio_direction_output(data->sensor_reset, 0);
        msleep(20);
        gpio_set_value_cansleep(data->sensor_reset, 1);
        msleep(20);
    }
    else
    {
        printk("gpio reset fail\n");
        goto init_probe_done;
    }

    /* 3. Read sensor Model ID: */
    if (mt9m114_i2c_read(REG_MT9M114_MODEL_ID, &model_id) < 0)
        goto init_probe_fail;
    printk("mt9m114 model_id = 0x%x\n", model_id);
    /* 4. Compare sensor ID to MT9M114 ID: */
    if (model_id != MT9M114_MODEL_ID) {
        rc = -ENODEV;
        goto init_probe_fail;
    }

    printk("mt9m114_probe_init_sensor-----------end\n");
    goto init_probe_done;
init_probe_fail:
    printk("mt9m114_probe_init_sensor fails_kalyani\n");
    gpio_free(data->sensor_reset);
    //gpio_set_value_cansleep(data->sensor_reset, 0);
    return rc;
init_probe_done:
    printk("mt9m114_probe_init_sensor finishes\n");
    return rc;
}
 static int mt9m114_set_effect(int mode,int8_t effect)
{
	int rc = 0;
	
	printk("mt9m114_set_effect  effect=%d\n",effect);
	switch (effect) {
	case CAMERA_EFFECT_OFF: 
		mt9m114_i2c_write_b_sensor(0x098E, 0xC874, 2); 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
		mt9m114_i2c_write_b_sensor(0xC874, 0x00,   1); 	// CAM_SFX_CONTROL                           
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28,   1); 	// SYSMGR_NEXT_STATE                         
		mt9m114_i2c_write_b_sensor(0x0080, 0x8004, 2); 	// COMMAND_REGISTER                        
             msleep(50);                      
		   
		break;
		
	case CAMERA_EFFECT_MONO: 
		mt9m114_i2c_write_b_sensor(0x098E, 0xC874, 2); 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL] 
		mt9m114_i2c_write_b_sensor(0xC874, 0x01,   1); 	// CAM_SFX_CONTROL                            
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28,   1); 	// SYSMGR_NEXT_STATE                          
		mt9m114_i2c_write_b_sensor(0x0080, 0x8004, 2); 	// COMMAND_REGISTER                         
             msleep(50);                      

		break;

	case CAMERA_EFFECT_NEGATIVE: 
		mt9m114_i2c_write_b_sensor(0x098E, 0xC874, 2); 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
		mt9m114_i2c_write_b_sensor(0xC874, 0x03,   1); 	// CAM_SFX_CONTROL                           
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28,   1); 	// SYSMGR_NEXT_STATE                         
		mt9m114_i2c_write_b_sensor(0x0080, 0x8004, 2); 	// COMMAND_REGISTER                        
             msleep(50);                      
	
		break;

	case CAMERA_EFFECT_SOLARIZE: {
		mt9m114_i2c_write_b_sensor(0x098E, 0xC874, 2); 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]   
		mt9m114_i2c_write_b_sensor(0xC874, 0x04,   1); 	// CAM_SFX_CONTROL                              
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28,   1); 	// SYSMGR_NEXT_STATE                            
		mt9m114_i2c_write_b_sensor(0x0080, 0x8004, 2); 	// COMMAND_REGISTER                           
             msleep(50);                      

	}
		break;

	case CAMERA_EFFECT_SEPIA: 
		mt9m114_i2c_write_b_sensor(0x098E, 0xC874, 2); 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]  
		mt9m114_i2c_write_b_sensor(0xC874, 0x02,   1); 	// CAM_SFX_CONTROL                             
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28,   1); 	// SYSMGR_NEXT_STATE                           
		mt9m114_i2c_write_b_sensor(0x0080, 0x8004, 2); 	// COMMAND_REGISTER                          

	      msleep(50);                      
    
		break;

	default: {
	            
		return rc;
	}
	}
return rc;

}
static long mt9m114_set_whitebalance(int mode,int8_t wb)
{
	long rc = 0;
	printk("mt9m114_set_whitebalance  wb=%d\n",wb);
	switch (wb) {
	case CAMERA_WB_AUTO: {
		//[AWB]
		mt9m114_i2c_write_b_sensor(0x098E, 0x0000, 2);     // LOGICAL_ADDRESS_ACCESS [UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL]                                          
		mt9m114_i2c_write_b_sensor(0xC909, 0x03,   1);          // UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL

		msleep(50);                      
	}
		break;

	case CAMERA_WB_INCANDESCENT: {
		//[Incandescent] [Alight MWB]
		mt9m114_i2c_write_b_sensor(0x098E, 0x0000, 2);     // LOGICAL_ADDRESS_ACCESS [UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL]
		mt9m114_i2c_write_b_sensor(0xC909, 0x01,   1);   // UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL
		mt9m114_i2c_write_b_sensor(0xC8F0, 0x0AF0, 2);     // UVC_WHITE_BALANCE_TEMPERATURE_CONTROL	

		msleep(50);
	}
		break;

 	case CAMERA_WB_CLOUDY_DAYLIGHT: {
		//[Cloudy] U30/TL84 MWB]

		mt9m114_i2c_write_b_sensor(0x098E, 0x0000, 2);     // LOGICAL_ADDRESS_ACCESS [UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL]
		mt9m114_i2c_write_b_sensor(0xC909, 0x01,   1);          // UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL
		mt9m114_i2c_write_b_sensor(0xC8F0, 0x1D4C, 2);     // UVC_WHITE_BALANCE_TEMPERATURE_CONTROL

		msleep(50);
	}
		break;

	case CAMERA_WB_DAYLIGHT: {
		//[DayLight] [D65 MWB]
		mt9m114_i2c_write_b_sensor(0x098E, 0x0000, 2);     // LOGICAL_ADDRESS_ACCESS [UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL]
		mt9m114_i2c_write_b_sensor(0xC909, 0x01,   1);          // UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL
		mt9m114_i2c_write_b_sensor(0xC8F0, 0x1770, 2);     // UVC_WHITE_BALANCE_TEMPERATURE_CONTROL

		msleep(50);
	}
		break;
        case CAMERA_WB_FLUORESCENT: {
		//[Flourescent] [CWF MWB]
		mt9m114_i2c_write_b_sensor(0x098E, 0x0000, 2);     // LOGICAL_ADDRESS_ACCESS [UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL]
		mt9m114_i2c_write_b_sensor(0xC909, 0x01,   1);          // UVC_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL
		mt9m114_i2c_write_b_sensor(0xC8F0, 0x0FA0, 2);    // UVC_WHITE_BALANCE_TEMPERATURE_CONTROL		

		msleep(50);
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

static int mt9m114_set_antibanding(int mode,int8_t antibanding)
{
	int rc = 0;
	printk("%s  antibanding =%d\n",__FUNCTION__,antibanding);
	switch (antibanding) {
	case CAMERA_ANTIBANDING_AUTO: 
   		//printk("%s  antibanding =CAMERA_ANTIBANDING_AUTO\n",__FUNCTION__);	
		break;
		
	case CAMERA_ANTIBANDING_60HZ: 
		//printk("%s  antibanding =CAMERA_ANTIBANDING_60HZ\n",__FUNCTION__);
	        mt9m114_i2c_write_b_sensor(0x098E, 0xC88B, 2); 
		mt9m114_i2c_write_b_sensor(0xC88B, 0x3C, 1); 
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28, 1); 
		mt9m114_i2c_write_b_sensor(0x0080, 0x8002, 2);
		msleep(100);
		break;

	case CAMERA_ANTIBANDING_50HZ: 
            // printk("%s  antibanding =CAMERA_ANTIBANDING_50HZ\n",__FUNCTION__);	
		mt9m114_i2c_write_b_sensor(0x098E, 0xC88B, 2); 
		mt9m114_i2c_write_b_sensor(0xC88B, 0x32, 1); 
		mt9m114_i2c_write_b_sensor(0xDC00, 0x28, 1); 
		mt9m114_i2c_write_b_sensor(0x0080, 0x8002, 2);
		msleep(100);
		break;

	case CAMERA_ANTIBANDING_OFF: 
		//printk("%s  antibanding =CAMERA_ANTIBANDING_OFF\n",__FUNCTION__);	
		break;

	default: {
	     // printk("%s  antibanding =wrong\n",__FUNCTION__);      
	       rc = -EINVAL;		
		 break;
	     }
	}
	return rc;
}

int mt9m114_sensor_open_init(const struct msm_camera_sensor_info *data)
{
    int32_t rc = 0;

    printk("%s: %d\n", __func__, __LINE__);
    printk("Calling mt9m114_sensor_open_init\n");
    mt9m114_ctrl = kzalloc(sizeof(struct mt9m114_ctrl_t), GFP_KERNEL);
    if (!mt9m114_ctrl) {
        printk("mt9m114_init failed!\n");
        rc = -ENOMEM;
        goto init_done;
    }
    mt9m114_ctrl->fps_divider = 1 * 0x00000400;
    mt9m114_ctrl->pict_fps_divider = 1 * 0x00000400;
    mt9m114_ctrl->fps = 30 * Q8;
    mt9m114_ctrl->set_test = TEST_OFF;
    mt9m114_ctrl->prev_res = QTR_SIZE;
    mt9m114_ctrl->pict_res = FULL_SIZE;
    mt9m114_ctrl->curr_res = INVALID_SIZE;

    if (data)
        mt9m114_ctrl->sensordata = data;

    /* enable mclk first */

    msm_camio_clk_rate_set(24000000);

    msleep(20);
    rc = mt9m114_probe_open_sensor();
    //rc = mt9m114_probe_init_sensor(data);
    if (rc < 0)
    {
        printk("Calling mt9m114_sensor_open_init fail\n");
        goto init_fail;
    }

    rc = mt9m114_sensor_setting(REG_INIT, RES_PREVIEW);
    if (rc < 0)
    {
        gpio_set_value_cansleep(data->sensor_reset, 0);
        goto init_fail;
    } else
        goto init_done;
init_fail:
    printk("mt9m114_sensor_open_init fail\n");
    mt9m114_probe_init_done(data);
    kfree(mt9m114_ctrl);
init_done:
    printk("mt9m114_sensor_open_init done\n");
    return rc;
}

static int mt9m114_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9m114_wait_queue);
	return 0;
}

static const struct i2c_device_id mt9m114_i2c_id[] = {
	{"mt9m114", 0},
	{ }
};

static int mt9m114_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	printk("mt9m114_i2c_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	mt9m114_sensorw = kzalloc(sizeof(struct mt9m114_work_t), GFP_KERNEL);
	if (!mt9m114_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9m114_sensorw);
	mt9m114_init_client(client);
	mt9m114_client = client;

	msleep(50);

	printk("mt9m114_i2c_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	printk("mt9m114_i2c_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit mt9m114_remove(struct i2c_client *client)
{
	struct mt9m114_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	mt9m114_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver mt9m114_i2c_driver = {
	.id_table = mt9m114_i2c_id,
	.probe  = mt9m114_i2c_probe,
	.remove = __exit_p(mt9m114_remove),
	.driver = {
		.name = "mt9m114",
	},
};

int mt9m114_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&mt9m114_mut);
	CDBG("mt9m114_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SET_MODE:
		rc = mt9m114_set_sensor_mode(cdata.mode,
			cdata.rs);
		break;
	case CFG_SET_EFFECT:
		rc = mt9m114_set_effect(
					cdata.mode,
					cdata.cfg.effect);
		break;
	case CFG_SET_WB:
		rc=mt9m114_set_whitebalance(
					cdata.mode,
					cdata.cfg.whitebalance);
		break;
	case CFG_PWR_DOWN:
		rc = mt9m114_power_down();
		break;
	case CFG_SET_ANTIBANDING:
		rc = mt9m114_set_antibanding(
						cdata.mode,
						cdata.cfg.antibanding);
		break; 
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(&mt9m114_mut);

	return rc;
}
static int mt9m114_sensor_release(void)
{
    int rc = -EBADF;
    printk("mt9m114_release begin\n");
    mutex_lock(&mt9m114_mut);
    //mt9m114_power_down();
    //gpio_set_value_cansleep(mt9m114_ctrl->sensordata->sensor_reset, 0);
    //gpio_free(mt9m114_ctrl->sensordata->sensor_reset);
    mt9m114_enter_standby();
    kfree(mt9m114_ctrl);
    mt9m114_ctrl = NULL;
    printk("mt9m114_release completed\n");
    mutex_unlock(&mt9m114_mut);

    return rc;
}

static int mt9m114_sensor_probe(const struct msm_camera_sensor_info *info,
        struct msm_sensor_ctrl *s)
{
    int rc = 0;
    int32_t i = 0;
    int32_t array_length = 0;
    //if the system have find a sensor for back camera,goto else.
    if(0x0000 == FRONT_CAMERA_SENSOR_DETECT)
    {
        printk("mt9m114_sensor_probe-----------begin\n");
        rc = i2c_add_driver(&mt9m114_i2c_driver);
        if (rc < 0 || mt9m114_client == NULL)
        {
            rc = -ENOTSUPP;
            goto probe_fail;
        }
        msm_camio_clk_rate_set(24000000);
        msleep(20);
    
        rc = mt9m114_probe_init_sensor(info);
        if (rc < 0)
            goto probe_fail;

	  //if in mt9m114_probe_init_sensor(info) the system find a sensor,the system will run the follow program.
        //And you must set FRONT_CAMERA_SENSOR_DETECT to be 0x0001 before the systm write the sensor's register.
        FRONT_CAMERA_SENSOR_DETECT  = 0x0001;
	  
        array_length = sizeof(mt9m114_init_settings_array) /
            sizeof(mt9m114_init_settings_array[0]);
        for (i = 0; i < array_length; i++)
        {
            rc = mt9m114_i2c_write_b_sensor(
                mt9m114_init_settings_array[i].reg_addr,
                mt9m114_init_settings_array[i].reg_val, 
                mt9m114_init_settings_array[i].lens);
                
            if (rc < 0)
                goto probe_fail;//return rc;
        }

        MT9M114_INIT_FLAG = 0x0001;
    
        s->s_init = mt9m114_sensor_open_init;
        s->s_release = mt9m114_sensor_release;
        s->s_config  = mt9m114_sensor_config;
        s->s_camera_type = FRONT_CAMERA_2D;
        s->s_mount_angle = 0;

	  //mt9m114_probe_init_done(info);
        rc = mt9m114_enter_standby();
	  if(rc < 0)
	  {
               printk("The system have find the sensor and intialization it,but enter software standby failed!\n");
               goto probe_fail;
	  }
	  else
	  {
               printk("The system will use the sensor of  MT9M114\n");
               printk("mt9m114_sensor_probe-----------end\n");
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
    printk("mt9m114_sensor_probe: SENSOR PROBE FAILS!\n");
    mt9m114_probe_init_done(info);
    i2c_del_driver(&mt9m114_i2c_driver);
    return rc;
}

static int __mt9m114_probe(struct platform_device *pdev)
{

	return msm_camera_drv_start(pdev, mt9m114_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9m114_probe,
	.driver = {
		.name = "msm_camera_mt9m114",
		.owner = THIS_MODULE,
	},
};

static int __init mt9m114_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}


late_initcall(mt9m114_init);


MODULE_DESCRIPTION("OMNI VGA YUV sensor driver");
MODULE_LICENSE("GPL v2");

