#ifndef HARDWARE_SELF_ADAPT_H
#define HARDWARE_SELF_ADAPT_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                    GPIO/BIO   S E R V I C E S

GENERAL DESCRIPTION

REFERENCES

EXTERNALIZED FUNCTIONS
  None.

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2009 by S7, Incorporated.  All Rights Reserved.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/
/*===========================================================================

                      EDIT HISTORY FOR FILE

 This section contains comments describing changes made to this file.
  Notice that changes are listed in reverse chronological order.


when       who      what, where, why
-------------------------------------------------------------------------------
20091123    hqt      creat
=========================================================================== */


/*<MSM7X25 hw modify --fix board id-- yuxuesong 20091209 £ºbegin */
  /*del old product type*/

/*< MSM7X25 hw modify board id haoqingtao 20091125 begin*/
typedef enum
{
    LCD_S6D74A0_SAMSUNG_HVGA,
    LCD_ILI9325_INNOLUX_QVGA,
    LCD_ILI9325_BYD_QVGA,
    LCD_ILI9325_WINTEK_QVGA,
    LCD_SPFD5408B_KGM_QVGA,

    /*<BU5D01169 yuxuesong 20100121 begin*/
    LCD_HX8357A_BYD_QVGA,
    LCD_HX8368A_SEIKO_QVGA,
    /* BU5D01169 yuxuesong 20100121 end>*/

    /* < BU5D04653 gaohuajiang 20100310 begin */
    LCD_HX8347D_TRULY_QVGA,
	/* < BU5D08135 gaohuajiang 20100419 begin */
    LCD_HX8347D_INNOLUX_QVGA,
	/* BU5D08135 gaohuajiang 20100419 end > */
    LCD_ILI9325C_WINTEK_QVGA,    
    /* BU5D04653 gaohuajiang 20100310 end > */
    
/* < BU5D07317 heping 20100413 begin */  
/* U8300 need to support the HX8368a ic driver of TRULY LCD */
    LCD_HX8368A_TRULY_QVGA,
/* BU5D07317 heping 20100413 end > */  
    /* < BU5D06369 wuxiaojin 2010401 BEGIN*/
    LCD_HX8357A_TRULY_HVGA,
    LCD_HX8357A_WINTEK_HVGA,
    /* BU5D06369 wuxiaojin 20100401 END > */ 
	
    LCD_MAX_NUM,
    LCD_NONE =0xFF
}lcd_panel_type;
/* MSM7X25 hw modify board id haoqingtao 20091125 end>*/

/*<BU5D01169 yuxuesong 20100121 begin*/
typedef enum
{
    HW_VER_SUB_VA            = 0x0,
    HW_VER_SUB_VB            = 0x1,
    HW_VER_SUB_VC            = 0x2,
    HW_VER_SUB_VD            = 0x3,
    HW_VER_SUB_VE            = 0x4,
    HW_VER_SUB_SURF          = 0xF,
    HW_VER_SUB_MAX           = 0xF
}hw_ver_sub_type;

/* <BU5D01928 zhangxiangdang 20100316 begin */
typedef enum
{
	GS_ADIX345 	= 0x01,
	GS_ST35DE	= 0x02,
	GS_ST303DLH = 0X03
}hw_gs_type;
/* BU5D01928 zhangxiangdang 20100316 end> */ 
	
#define HW_VER_MAIN_MASK (0xFFF0)
#define HW_VER_SUB_MASK  (0x000F)
/* BU5D01169 yuxuesong 20100121 end>*/

lcd_panel_type lcd_panel_probe(void);
/*<MSM7X25 hw modify --self-adapt TOUCH-- yuxuesong 20091207 £ºbegin */
int board_use_tssc_touch(bool * use_touch_key);
/* MSM7X25 hw modify --self-adapt TOUCH-- yuxuesong 20091207 £ºend> */
/* MSM7X25 hw modify --fix board id-- yuxuesong 20091209 £ºend> */

/*< BU5D01069 haoqingtao 20100115 begin*/
/*add code to support JOY(U8120) */
int board_support_ofn(bool * ofn_support);
/* BU5D01069 haoqingtao 20100115 end >*/

/*<BU5D08141 yuxuesong 20100419 begin*/
bool st303_gs_is_supported(void);
void set_st303_gs_support(bool status);

/*
 *  return: 0 ----not support bcm wifi
 *          1 ----support bcm wifi
 *          *p_gpio  return MSM WAKEUP WIFI gpio value
 */
unsigned int board_support_bcm_wifi(unsigned *p_gpio);
/* BU5D08141 yuxuesong 20100419 end>*/

/*<MSM7X25 hw modify --self-adapt project menu-- mazhenhua 20091207 £ºbegin */
char *get_lcd_panel_name(void);
/* MSM7X25 hw modify --self-adapt project menu-- mazhenhua 20091207 £ºend> */

/*<BU5D01169 yuxuesong 20100121 begin*/
hw_ver_sub_type get_hw_sub_board_id(void);
/* BU5D01169 yuxuesong 20100121 end>*/

#endif

