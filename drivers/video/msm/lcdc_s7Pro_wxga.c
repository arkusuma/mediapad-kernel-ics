/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/pwm.h>
#ifdef CONFIG_PMIC8058_PWM
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwm.h>
#endif
#include <mach/gpio.h>
#include "msm_fb.h"

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>

#include <mach/board.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/miscdevice.h>

// IOCTL Command
#define 	LCDC_IOC_MAGIC 	  'D'
#define     IOCTL_SET_CABC    _IO(LCDC_IOC_MAGIC, 0)

static struct pwm_device *bl_pwm;

#define PWM_FREQ_HZ 20000
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_DUTY_LEVEL (PWM_PERIOD_USEC / PWM_LEVEL)
#define PWM_LEVEL 255
#define LCDC_GPIO_COLOR_EN 39

extern unsigned int my_hpd_state;
static struct msm_panel_common_pdata *cm_pdata;
static struct platform_device *cm_fbpdev;
static int led_pwm;		/* pm8058 gpio 24, channel 0 */
static int led_en;		/* pm8058 gpio 1 */
static int lvds_pwr_down;	/* msm gpio 30 */

static int led_vccs_en;
static int lcd_h_rev;
static int lcd_v_rev;
static int lcd_cabc_en;
static int lcd_panel_id;


static int chimei_bl_level = 127;



static int colorEn_state;

#define LCDC_NUM_GPIO 28
#define LCDC_GPIO_START 0

static void lcdc_gpio_config(int on)
{
      int n, ret = 0;
 	for (n = 0; n < LCDC_NUM_GPIO; n++) {
		if (on) {
			ret = gpio_request(LCDC_GPIO_START + n, "LCDC_GPIO");
			if (unlikely(ret)) {
				pr_err("%s not able to get gpio\n", __func__);
				break;
			}
		} else
			gpio_free(LCDC_GPIO_START + n);
	}

	if (ret) {
		for (n--; n >= 0; n--)
			gpio_free(LCDC_GPIO_START + n);
	}
}

#define _GET_REGULATOR(var, name) do {					\
	if (var == NULL) {						\
		var = regulator_get(NULL, name);			\
		if (IS_ERR(var)) {					\
			pr_err("'%s' regulator not found, rc=%ld\n",	\
				name, PTR_ERR(var));			\
			var = NULL;					\
		}							\
	}								\
} while (0)

static int display_power_on;
static void display_panel_power(int on)
{
	int rc;
	static struct regulator *display_reg;
	static struct regulator *lvds_reg;
      #if S7_HWID_L3H(S7, S7301, B)
      static struct regulator *iovcc_reg;
      #endif

      if(on) {
          /* LCD VCCS set to 3.3V */
          _GET_REGULATOR(display_reg, "8901_l2");
          if (!display_reg)
            return;
          rc = regulator_set_voltage(display_reg,
          	3300000, 3300000);
          if (rc)
          	goto out_display_reg;
          rc = regulator_enable(display_reg);
          if (rc)
          	goto out_display_reg;

          /* LVDS VCCS set to 3.3V */
          _GET_REGULATOR(lvds_reg, "8901_l3");
          if (!lvds_reg)
          	return;
#if 0
          rc = regulator_set_voltage(lvds_reg,
          	3000000, 3000000);
#else
          rc = regulator_set_voltage(lvds_reg,
          	3300000, 3300000);
#endif
          if (rc)
          	goto out_lvds_reg;
          rc = regulator_enable(lvds_reg);
          if (rc)
          	goto out_lvds_reg;

          #if S7_HWID_L3H(S7, S7301, B)
          /* LVDS_IOVCC set to 1.8V */
          _GET_REGULATOR(iovcc_reg, "8058_l11");
          if (!iovcc_reg)
          	return;
          rc = regulator_set_voltage(iovcc_reg,
          	1800000, 1800000);
          if (rc)
          	goto out_iovcc_reg;
          rc = regulator_enable(iovcc_reg);
          if (rc)
          	goto out_iovcc_reg;
          #endif

          /* LCD_H_REV */
          rc = gpio_request(lcd_h_rev, "lcd_h_rev");
          if (rc == 0) {
		/* output, pull low to enable */
		gpio_direction_output(lcd_h_rev, 0);
          } else {
		pr_err("%s: lcd_h_rev=%d, gpio_request failed\n",
			__func__, lcd_h_rev);
             goto out_pm_reg;
          }
          
          /* LCD_V_REV */
          rc = gpio_request(lcd_v_rev, "lcd_v_rev");
          if (rc == 0) {
		/* output, pull low to enable */
		gpio_direction_output(lcd_v_rev, 0);
          } else {
		pr_err("%s: lcd_v_rev=%d, gpio_request failed\n",
			__func__, lcd_v_rev);
             goto out_lcd_h_rev;
          }

		  
          /* LCD_ID */
          rc = gpio_request(lcd_panel_id, "lcd_panel_id");
          if (rc == 0) {
		/* output, pull high to enable */
		gpio_direction_input(lcd_panel_id);
          } else {
		pr_err("%s: lcd_panel_id=%d, gpio_request failed\n",
			__func__, lcd_panel_id);
             goto out_lcd_v_rev;
          } 

          display_power_on = 1;
      }else {
          if (display_power_on) {       
            display_power_on = 0;
            goto out_lcd_panel_id;
          }
      }
      return;


out_lcd_panel_id:
       gpio_free(lcd_panel_id);          
out_lcd_v_rev:
       gpio_free(lcd_v_rev);     
out_lcd_h_rev:
       gpio_free(lcd_h_rev);      
out_pm_reg:
#if S7_HWID_L3L(S7, S7301, T0)
	regulator_disable(lvds_reg);
#elif S7_HWID_L3H(S7, S7301, B)
	regulator_disable(iovcc_reg);
#endif      
#if S7_HWID_L3H(S7, S7301, B)   
out_iovcc_reg:
	regulator_disable(lvds_reg);
	regulator_put(iovcc_reg);
	iovcc_reg = NULL;     
#endif
out_lvds_reg:
	regulator_disable(display_reg);
	regulator_put(lvds_reg);
	lvds_reg = NULL;
out_display_reg:
	regulator_put(display_reg);
	display_reg = NULL;    
}
#undef _GET_REGULATOR



/*****************************************************************************
 Prototype    : set_cabc_state
 Description  : set cabc state
 Input        : int on  
 				1----set cabc function on
 				0----set cabc function off
 Output       : None
 Return Value : 0---success  <0 ----fail
 Calls        : when play the video or preview camera, turn the cabc, on other scencs, turn off cabc
 Called By    : 
 
  History        :
  1.Date         : 2011/7/14
    Author       : nielimin
    Modification : Created function

*****************************************************************************/
static ssize_t set_cabc_state(int state)
{	
	/* output, pull low to disable */
	gpio_direction_output(lcd_cabc_en, state);	
	return 0;
}


static int usrfs_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "call usrfs_open\n");
    return 0;
}

static int usrfs_close(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "call usrfs_open\n");
    return 0;
}

/* Begin: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
static long usrfs_ioctl(struct file* file, unsigned int cmd, unsigned long param)
{
    int ret = 0;
    switch (cmd)
    {	   
		case IOCTL_SET_CABC:
			if(1 == param)
			{
				set_cabc_state(1);
			}
			else if(0 == param)
			{
				set_cabc_state(0);
			}
			break;
		default:
			break;
    }
    return ret;
}
/* End: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/

static const struct file_operations usrfs_fops =
{
    .open = usrfs_open,
/* Begin: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
    .unlocked_ioctl  = usrfs_ioctl,
/* End: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
    .release = usrfs_close,
};

static struct miscdevice usrfs_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "lcdc_pro",            
    .fops  = &usrfs_fops,
};



static void lcdc_chimei_set_backlight(int level)
{
	
	int ret;
	int a[11] = {24,44,80,108,120,84,88,100,104,112,620};
	int b[11] = {800,300,-1500,-3600,-4800,-300,-900,-3000,-3800,-5600,-132600};
	int backlight;
	/* As the pwm frquency set to 20kHZ, the level should be larger than 6, As the backlight require the level larger than 255 * 20% = 51 */
	
	
	chimei_bl_level = level;
	
	
	if(0 == level)
	{
		backlight = 0;	
	}
	else
	{
		backlight = (a[level/25] * level + b[level/25])/100;
	}
	if (bl_pwm) {
		ret = pwm_config(bl_pwm, backlight * PWM_PERIOD_USEC / PWM_LEVEL, PWM_PERIOD_USEC);
		if (ret) {
			pr_err("%s: pwm_config on pwm failed %d\n",
					__func__, ret);
			return;
		}

		ret = pwm_enable(bl_pwm);
		if (ret) {
			pr_err("%s: pwm_enable on pwm failed %d\n",
					__func__, ret);
			return;
		}
	}    
}

#define LCD_STATUS_OFF      0
#define LCD_STATUS_ON       1
#define LCD_STATUS_BEGIN    2

static int status_of_lcd = LCD_STATUS_BEGIN;

static int display_panel_on = 0;
static int display_panel_operate = 0;

static int GpioColorRequestStatusFlags = 0;
static int lcdc_chimei_panel_on(struct platform_device *pdev)
{
    
	int ret = 0;
    

#if 1
      printk(KERN_INFO "lcdc_chimei_panel_on---------------begin");

    
	if (1 == display_panel_operate) 
	{       
		return ret;
	}
    display_panel_operate = 1;
    if (1 == display_panel_on) 
	{   
	    display_panel_operate = 0;
		return ret;
	}
    display_panel_on = 1;
    
	/* panel powered on here */
      display_panel_power(1);

	ret = gpio_request(lvds_pwr_down, "lvds_pwr_down");
	if (ret == 0) {
		/* output, pull high to enable */
		gpio_direction_output(lvds_pwr_down, 1);
	} else {
		pr_err("%s: lvds_pwr_down=%d, gpio_request failed\n",
			__func__, lvds_pwr_down);
	} 

	msleep(200);
	/* power on led pwm power >= 200 ms */
      #if S7_HWID_L3H(S7, S7301, B)
	ret = gpio_request(led_vccs_en, "led_vccs_en");
	if (ret == 0) {
		/* output, pull high to enable */
		gpio_direction_output(led_vccs_en, 1);
	} else {
		pr_err("%s: led_vccs_en=%d, gpio_request failed\n",
			__func__, led_vccs_en);
	}
      #endif
      msleep(10);

#if 0
	lcdc_chimei_set_backlight(chimei_bl_level);

	msleep(10);
#endif

	ret = gpio_request(led_en, "led_en");
	if (ret == 0) {
		/* output, pull high */
		gpio_direction_output(led_en, 1);
	} else {
		pr_err("%s: led_en=%d, gpio_request failed\n",
			__func__, led_en);
	}

	/* LCD_CABC_EN */
	ret = gpio_request(lcd_cabc_en, "lcd_cabc_en");
	if (ret == 0) 
	{
		/* output, pull low to disable */
		gpio_direction_output(lcd_cabc_en, 0);
	} 
	else
	{
		pr_err("%s: cabc_en=%d, gpio_request failed\n",
			__func__, lcd_cabc_en);
	}

	ret = gpio_request(LCDC_GPIO_COLOR_EN, "lcdc_gpio_color_en");
	if (ret == 0) 
	{
        GpioColorRequestStatusFlags = 1;
		gpio_direction_output(LCDC_GPIO_COLOR_EN, colorEn_state);
	}
	else 
	{
		pr_err("%s: LCDC_GPIO_COLOR_EN=%d, gpio_request failed\n",
			__func__, LCDC_GPIO_COLOR_EN);
	}	
      lcdc_gpio_config(1);
      msleep(20);
      lcdc_chimei_set_backlight(chimei_bl_level);
      printk(KERN_INFO "lcdc_chimei_panel_on---------------end");
#endif
    
    status_of_lcd = LCD_STATUS_ON;
  
    if (-EINVAL == ret) 
    {
		display_panel_on = 0;
	} 
    display_panel_operate = 0;
	
	return ret;

}

static int lcdc_chimei_panel_off(struct platform_device *pdev)
{
    unsigned int my_bl_level = 0; 
      printk(KERN_INFO "lcdc_chimei_panel_off---------------begin");
	/* pull low to disable */
    
	if (1 == display_panel_operate) 
	{       
		return 0;
	}
    display_panel_operate = 1;

    if (0 == display_panel_on) 
	{   
	    display_panel_operate = 0;
		return 0;
	}
    display_panel_on = 0;
	
	gpio_set_value_cansleep(led_en, 0);
	gpio_free(led_en);

	msleep(10);

    if (my_hpd_state) {
        my_bl_level = chimei_bl_level;
    }

    lcdc_chimei_set_backlight(0);

    if (my_hpd_state) {
        chimei_bl_level = my_bl_level;
    }

      msleep(10);
	#if S7_HWID_L3H(S7, S7301, B)	
	gpio_set_value_cansleep(led_vccs_en, 0);
	gpio_free(led_vccs_en);
      #endif
      /* power off led pwm power >= 200 ms */
      msleep(200);
      
	/* pull low to shut down lvds */
	gpio_set_value_cansleep(lvds_pwr_down, 0);
	gpio_free(lvds_pwr_down);

	gpio_set_value_cansleep(lcd_cabc_en, 0);
	gpio_free(lcd_cabc_en);

	gpio_set_value_cansleep(LCDC_GPIO_COLOR_EN, 0);
	gpio_free(LCDC_GPIO_COLOR_EN);	
    GpioColorRequestStatusFlags = 0;
	/* panel power off here */
      display_panel_power(0);
      lcdc_gpio_config(0);
	  
      display_panel_operate = 0;
	  status_of_lcd = LCD_STATUS_OFF;
	  
      printk(KERN_INFO "lcdc_chimei_panel_off---------------end");
	return 0;
}

static void lcdc_chimei_panel_backlight(struct msm_fb_data_type *mfd)
{
	if(0 != mfd->bl_level)
		lcdc_chimei_set_backlight(mfd->bl_level);
}

static int __devinit chimei_probe(struct platform_device *pdev)
{
	int rc = 0;
      printk(KERN_INFO "chimei_probe--------begin");

	if (pdev->id == 0) {
		cm_pdata = pdev->dev.platform_data;
		if (cm_pdata == NULL) {
			pr_err("%s: no PWM gpio specified\n", __func__);
			return 0;
		}
		led_pwm = cm_pdata->gpio_num[0];
		led_en = cm_pdata->gpio_num[1];
		lvds_pwr_down = cm_pdata->gpio_num[2];
#if S7_HWID_L3H(S7, S7301, B)
		led_vccs_en = cm_pdata->gpio_num[3];
#endif
		lcd_h_rev = cm_pdata->gpio_num[4];
		lcd_v_rev = cm_pdata->gpio_num[5];
		lcd_cabc_en = cm_pdata->gpio_num[6];
		lcd_panel_id = cm_pdata->gpio_num[7];
		pr_info("%s: led_pwm=%d led_en=%d lvds_pwr_down=%d led_vccs_en=%d\n",
			__func__, led_pwm, led_en, lvds_pwr_down, led_vccs_en);
		return 0;
	}

	if (cm_pdata == NULL)
		return -ENODEV;

	bl_pwm = pwm_request(led_pwm, "backlight");
	if (bl_pwm == NULL || IS_ERR(bl_pwm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_pwm = NULL;
	}
	cm_fbpdev = msm_fb_add_device(pdev);
	if (!cm_fbpdev) {
		dev_err(&pdev->dev, "failed to add msm_fb device\n");
		rc = -ENODEV;
		goto probe_exit;
	}
       printk(KERN_INFO "chimei_probe--------end");

probe_exit:
    return rc;
}

static struct platform_driver this_driver = {
	.probe  = chimei_probe,
	.driver = {
		.name   = "lcdc_s7Pro_lvds_wxga",
	},
};

static struct msm_fb_panel_data chimei_panel_data = {
	.on = lcdc_chimei_panel_on,
	.off = lcdc_chimei_panel_off,
	.set_backlight = lcdc_chimei_panel_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_s7Pro_lvds_wxga",
	.id	= 1,
	.dev	= {
		.platform_data = &chimei_panel_data,
	}
};

static int DRV_LCDDevOpen(struct inode *inode, struct file *file);
static int DRV_LCDDevClose(struct inode *inode, struct file *file);
/* Begin: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
static long DRV_LCDDevIoctl(struct file *file, unsigned int cmd, unsigned long arg);
/* End: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/

#define BSP_LCD_DEVICE_NAME "lcd_ctrl"

#define BSP_LCD_MODE_CRTL    0x11223344


#define BSP_LCD_MODE_STATE   0x11223355


struct file_operations LCD_fops = 
{
/* Begin: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
    .unlocked_ioctl   = DRV_LCDDevIoctl,
/* End: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
    .open    = DRV_LCDDevOpen,
    .release = DRV_LCDDevClose
};

static struct miscdevice LCD_dev = {
	MISC_DYNAMIC_MINOR,
	BSP_LCD_DEVICE_NAME,
	&LCD_fops,
};

/* Open the device */
static int DRV_LCDDevOpen(struct inode *inode, struct file *file)
{
    return 0;
}

/* Close the device */
static int DRV_LCDDevClose(struct inode *inode, struct file *file)
{
    return 0;
}

/* Control the device */
/* Begin: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
static long DRV_LCDDevIoctl(struct file *file, unsigned int cmd, unsigned long arg)
/* End: modified by z00176551 20111208 for ioctl --> unlocked_ioctl in kernel 3.0*/
{
    unsigned int ulRet = 0;
    unsigned int LCD_MODE;
    
	//static int pre_backlight;
    switch(cmd)
    {
       case BSP_LCD_MODE_CRTL: 
            ulRet = copy_from_user((void *)&LCD_MODE,(void __user *)arg, sizeof(LCD_MODE));
            if (0 != ulRet) {
                printk("%s: copy_from_user error.\n", __func__);     
                return -1;
            } else {                             
                if (LCD_MODE > 1) {
                    
					chimei_bl_level = LCD_MODE; 
                    
                } 
                else if(1 == LCD_MODE) {
                    printk("~~~lcd ON!~~~\n");
                    lcdc_chimei_panel_on(cm_fbpdev);                                        
                } else { 
                    printk("~~~lcd OFF!~~~\n");
    
                    lcdc_chimei_panel_off(cm_fbpdev);                  
                }                              
            }
            break;

       
       case BSP_LCD_MODE_STATE:
            if (copy_to_user((void *) arg, &status_of_lcd, sizeof(status_of_lcd))) {
                printk("%s: copy_to_user error.\n", __func__);   
                return -1;
            }       
            break;
       

       default:
           printk("Error Ioctl operation.\r\n");
           return -1;         
    }
    return 0;
}


static ssize_t set_ColorEn_state(int state)
{
    if (0 != GpioColorRequestStatusFlags) {
        return gpio_direction_output(LCDC_GPIO_COLOR_EN, state);
    } else {
        printk("~~~GPIO 39 was not acquirable, failed to enable color function!~~~\n");
        return 0;
    }
}
static ssize_t lcdc_colorEn_show(struct device_driver *drv, char *buf)
{
	return sprintf(buf, "colorEn = %d\n",colorEn_state);
}

static ssize_t lcdc_colorEn_store(struct device_driver *drv, const char *buf, size_t count)
{
	int status = 0;
    if (buf == NULL) {
		return count;
    }
	status = simple_strtoul(buf, NULL, 0);
	if ((status != 0) && (status != 1)) 
	{
		printk(KERN_ERR "[%s %d] Must be 1 or 0\n", __func__, __LINE__);
    }
	else
	{
		colorEn_state = 1 - status;
    }    
	set_ColorEn_state(colorEn_state);
	return count;
}


static DRIVER_ATTR(color_en, 0644, lcdc_colorEn_show, lcdc_colorEn_store);

static int __init lcdc_chimei_lvds_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;
      printk(KERN_INFO "lcdc_chimei_lvds_panel_init---------------begin");

#ifdef CONFIG_FB_MSM_MIPI_PANEL_DETECT
	if (msm_fb_detect_client("lcdc_s7Pro_lvds_wxga"))
		return 0;
#endif

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &chimei_panel_data.panel_info;
	pinfo->xres = 1280;
	pinfo->yres = 800;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
    pinfo->clk_rate = 69300000;//69300000; //71110000;  //48000000;
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	/*
	 * this panel is operated by de,
	 * vsycn and hsync are ignored
	 */
    pinfo->lcdc.h_back_porch  = 96;
    pinfo->lcdc.h_front_porch = 0;
    pinfo->lcdc.h_pulse_width = 64;
    pinfo->lcdc.v_back_porch  = 7;
    pinfo->lcdc.v_front_porch = 0;
    pinfo->lcdc.v_pulse_width = 16;
    pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0x00;//0xff;
	pinfo->lcdc.hsync_skew = 0;
	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);
      printk(KERN_INFO "lcdc_chimei_lvds_panel_init---------------end");
   
	ret = misc_register(&usrfs_device);
    if (ret)
    {
        printk(KERN_ERR "HDMI: can't register misc device for minor %d\n", usrfs_device.minor);
        misc_deregister(&usrfs_device);
    }

    ret = misc_register(&LCD_dev);
    if (ret < 0) {
        printk("LCD_dev register error.\n");
        misc_deregister(&LCD_dev);
    }
	if(driver_create_file(&(this_driver.driver), &driver_attr_color_en) < 0) 
	{
       pr_err("failed to create sysfs entry(state): \n");
	}
	return ret;
}

module_init(lcdc_chimei_lvds_panel_init);
