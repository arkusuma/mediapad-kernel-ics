/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
*
* File Name          : lsm303dlh_mag.c
* Authors            : MSH - Motion Mems BU - Application Team
*		     : Carmine Iascone (carmine.iascone@st.com)
*		     : Matteo Dameno (matteo.dameno@st.com)
* Version            : V 1.0.1
* Date               : 19/03/2010
* Description        : LSM303DLH 6D module sensor API
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
*******************************************************************************/

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include "linux/hardware_self_adapt.h"

#include <linux/i2c/lsm303dlh.h>
#include <linux/akm8973.h>
#include <linux/slab.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define NAME	"st303_compass"

/** Maximum polled-device-reported g value */
#define H_MAX			8100

#define SHIFT_ADJ_2G		4
#define SHIFT_ADJ_4G		3
#define SHIFT_ADJ_8G		2

/* Magnetometer registers */
#define CRA_REG_M		0x00	/* Configuration register A */
#define CRB_REG_M		0x01	/* Configuration register B */
#define MR_REG_M		0x02	/* Mode register */

/* Output register start address*/
#define OUT_X_M			0x03

/* Magnetic Sensor Operation Mode */
#define NORMAL_MODE     	0x00
#define POS_BIAS         	0x01
#define NEG_BIAS         	0x02
#define CC_MODE          	0x00
#define IDLE_MODE        	0x02

/* Magnetometer X-Y sensitivity  */
#define XY_SENSITIVITY_1_3	1055	/* XY sensitivity at 1.3G */
#define XY_SENSITIVITY_1_9	795	/* XY sensitivity at 1.9G */
#define XY_SENSITIVITY_2_5	635	/* XY sensitivity at 2.5G */
#define XY_SENSITIVITY_4_0	430	/* XY sensitivity at 4.0G */
#define XY_SENSITIVITY_4_7	375	/* XY sensitivity at 4.7G */
#define XY_SENSITIVITY_5_6	320	/* XY sensitivity at 5.6G */
#define XY_SENSITIVITY_8_1	230	/* XY sensitivity at 8.1G */

/* Magnetometer Z sensitivity  */
#define Z_SENSITIVITY_1_3	950	/* Z sensitivity at 1.3G */
#define Z_SENSITIVITY_1_9	710	/* Z sensitivity at 1.9G */
#define Z_SENSITIVITY_2_5	570	/* Z sensitivity at 2.5G */
#define Z_SENSITIVITY_4_0	385	/* Z sensitivity at 4.0G */
#define Z_SENSITIVITY_4_7	335	/* Z sensitivity at 4.7G */
#define Z_SENSITIVITY_5_6	285	/* Z sensitivity at 5.6G */
#define Z_SENSITIVITY_8_1	205	/* Z sensitivity at 8.1G */

#define FUZZ			0
#define FLAT			0
#define I2C_RETRY_DELAY	5
#define I2C_RETRIES		5

//#define LSMS303DLH_MAG_OPEN_ENABLE
extern struct input_dev *sensor_dev;
static atomic_t use_num;
static atomic_t open_num;

static int who_am_i = 0;
#define WHO_AM_I	0x3C

static struct {
	unsigned int cutoff;
	unsigned int mask;
} odr_table[] = {
	{
	34,	LSM303DLH_MAG_ODR75}, {
	67,	LSM303DLH_MAG_ODR30}, {
	134,	LSM303DLH_MAG_ODR15}, {
	334,	LSM303DLH_MAG_ODR7_5}, {
	667,	LSM303DLH_MAG_ODR3_0}, {
	1334,	LSM303DLH_MAG_ODR1_5}, {
	0,	LSM303DLH_MAG_ODR_75}, };

struct lsm303dlh_mag_data {
	struct i2c_client *client;
	struct lsm303dlh_mag_platform_data *pdata;

	struct mutex lock;

	struct delayed_work input_work;
	struct input_dev *input_dev;

	int hw_initialized;
	atomic_t enabled;
	int on_before_suspend;

	u16 xy_sensitivity;
	u16 z_sensitivity;
	u8 resume_state[3];
};


#ifdef CONFIG_HAS_EARLYSUSPEND
struct early_suspend lsm303dlh_mag_earlysuspend;
static void lsm303dlh_mag_early_suspend(struct early_suspend *h);
static void lsm303dlh_mag_late_resume(struct early_suspend *h);
#endif

/*
 * Because misc devices can not carry a pointer from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct lsm303dlh_mag_data *lsm303dlh_mag_misc_data;

static int lsm303dlh_mag_i2c_read(struct lsm303dlh_mag_data *mag,
				  u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = mag->client->addr,
		 .flags = mag->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = mag->client->addr,
		 .flags = (mag->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = len,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(mag->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&mag->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlh_mag_i2c_write(struct lsm303dlh_mag_data *mag,
				   u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = mag->client->addr,
		 .flags = mag->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(mag->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&mag->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlh_mag_hw_init(struct lsm303dlh_mag_data *mag)
{
	int err = -1;
	u8 buf[4];

	buf[0] = CRA_REG_M;
	buf[1] = mag->resume_state[0];
	buf[2] = mag->resume_state[1];
	buf[3] = mag->resume_state[2];
	err = lsm303dlh_mag_i2c_write(mag, buf, 3);

	if (err < 0)
		return err;

	mag->hw_initialized = 1;

	return 0;
}
//static void lsm303dlh_mag_device_power_off(struct lsm303dlh_mag_data *mag)
static int lsm303dlh_mag_device_power_off(struct lsm303dlh_mag_data *mag)
{
	int err = 0;
	u8 buf[2] = { MR_REG_M, IDLE_MODE };

	err = lsm303dlh_mag_i2c_write(mag, buf, 1);
	if (err < 0)
		dev_err(&mag->client->dev, "soft power off failed\n");

	if (mag->pdata->power_off) {
		mag->pdata->power_off();
		mag->hw_initialized = 0;
	}
	return err;		
}

static int lsm303dlh_mag_device_power_on(struct lsm303dlh_mag_data *mag)
{
	int err = 0;

	if (mag->pdata->power_on) {
		err = mag->pdata->power_on();
		if (err < 0)
			return err;
	}

	if (!mag->hw_initialized) {
		err = lsm303dlh_mag_hw_init(mag);
		if (err < 0) {
			lsm303dlh_mag_device_power_off(mag);
			return err;
		}
	} else {
	        u8 buf[2] = { MR_REG_M, NORMAL_MODE };

	        err = lsm303dlh_mag_i2c_write(mag, buf, 1);
	        if (err < 0){
		        dev_err(&mag->client->dev, "power on failed\n");	
	                return err;
		}
	}
	return err;
}

int lsm303dlh_mag_update_h_range(struct lsm303dlh_mag_data *mag,
				 u8 new_h_range)
{
	int err;
	u8 buf[2];

	switch (new_h_range) {
	case LSM303DLH_H_1_3G:
		mag->xy_sensitivity = XY_SENSITIVITY_1_3;
		mag->z_sensitivity = Z_SENSITIVITY_1_3;
		break;
	case LSM303DLH_H_1_9G:
		mag->xy_sensitivity = XY_SENSITIVITY_1_9;
		mag->z_sensitivity = Z_SENSITIVITY_1_9;
		break;
	case LSM303DLH_H_2_5G:
		mag->xy_sensitivity = XY_SENSITIVITY_2_5;
		mag->z_sensitivity = Z_SENSITIVITY_2_5;
		break;
	case LSM303DLH_H_4_0G:
		mag->xy_sensitivity = XY_SENSITIVITY_4_0;
		mag->z_sensitivity = Z_SENSITIVITY_4_0;
		break;
	case LSM303DLH_H_4_7G:
		mag->xy_sensitivity = XY_SENSITIVITY_4_7;
		mag->z_sensitivity = Z_SENSITIVITY_4_7;
		break;
	case LSM303DLH_H_5_6G:
		mag->xy_sensitivity = XY_SENSITIVITY_5_6;
		mag->z_sensitivity = Z_SENSITIVITY_5_6;
		break;
	case LSM303DLH_H_8_1G:
		mag->xy_sensitivity = XY_SENSITIVITY_8_1;
		mag->z_sensitivity = Z_SENSITIVITY_8_1;
		break;
	default:
		return -EINVAL;
	}

	if (atomic_read(&mag->enabled)) {
		/* Set configuration register 4, which contains g range setting
		 *  NOTE: this is a straight overwrite because this driver does
		 *  not use any of the other configuration bits in this
		 *  register.  Should this become untrue, we will have to read
		 *  out the value and only change the relevant bits --XX----
		 *  (marked by X) */
		buf[0] = CRB_REG_M;
		buf[1] = new_h_range;
		err = lsm303dlh_mag_i2c_write(mag, buf, 1);
		if (err < 0)
			return err;
	}

	mag->resume_state[1] = new_h_range;

	return 0;
}

int lsm303dlh_mag_update_odr(struct lsm303dlh_mag_data *mag, int poll_interval)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Convert the poll interval into an output data rate configuration
	 *  that is as low as possible.  The ordering of these checks must be
	 *  maintained due to the cascading cut off values - poll intervals are
	 *  checked from shortest to longest.  At each check, if the next lower
	 *  ODR cannot support the current poll interval, we stop searching */
	for (i = 0; i < ARRAY_SIZE(odr_table); i++) {
		config[1] = odr_table[i].mask;
		if (poll_interval < odr_table[i].cutoff)
			break;
	}

	config[1] |= NORMAL_MODE;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&mag->enabled)) {
		config[0] = CRA_REG_M;
		err = lsm303dlh_mag_i2c_write(mag, config, 1);
		if (err < 0)
			return err;
	}

	mag->resume_state[0] = config[1];

	return 0;
}

static int lsm303dlh_mag_get_acceleration_data(struct lsm303dlh_mag_data *mag,
					       int *xyz)
{
	int err = -1;
	int tmp = 0;
	/* Data bytes from hardware HxL, HxH, HyL, HyH, HzL, HzH */
	u8 mag_data[6];
	/* x,y,z hardware data */
	int hw_d[3] = { 0 };
	mag_data[0] = OUT_X_M;
	err = lsm303dlh_mag_i2c_read(mag, mag_data, 6);
        
	if (err < 0)
		return err;

	hw_d[0] = (int) (((mag_data[0]) << 8) | mag_data[1]);
	hw_d[1] = (int) (((mag_data[2]) << 8) | mag_data[3]);
	hw_d[2] = (int) (((mag_data[4]) << 8) | mag_data[5]);

    if (who_am_i == WHO_AM_I) {
		tmp = hw_d[1];
    	hw_d[1] = hw_d[2];
        hw_d[2] = tmp;
    }    

	hw_d[0] = (hw_d[0] & 0x8000) ? (hw_d[0] | 0xFFFF0000) : (hw_d[0]);
	hw_d[1] = (hw_d[1] & 0x8000) ? (hw_d[1] | 0xFFFF0000) : (hw_d[1]);
	hw_d[2] = (hw_d[2] & 0x8000) ? (hw_d[2] | 0xFFFF0000) : (hw_d[2]);

	hw_d[0] = hw_d[0] * 1000 / mag->xy_sensitivity;
	hw_d[1] = hw_d[1] * 1000 / mag->xy_sensitivity;
	hw_d[2] = hw_d[2] * 1000 / mag->z_sensitivity;

	xyz[0] = ((mag->pdata->negate_x) ? (-hw_d[mag->pdata->axis_map_x])
		  : (hw_d[mag->pdata->axis_map_x]));
	xyz[1] = ((mag->pdata->negate_y) ? (-hw_d[mag->pdata->axis_map_y])
		  : (hw_d[mag->pdata->axis_map_y]));
	xyz[2] = ((mag->pdata->negate_z) ? (-hw_d[mag->pdata->axis_map_z])
		  : (hw_d[mag->pdata->axis_map_z]));

	return err;
}

static void lsm303dlh_mag_report_values(struct lsm303dlh_mag_data *mag,
					int *xyz)
{
	input_report_abs(mag->input_dev, ABS_HAT0X, xyz[0]);
	input_report_abs(mag->input_dev, ABS_HAT0Y, xyz[1]);
	input_report_abs(mag->input_dev, ABS_BRAKE, xyz[2]);
	input_sync(mag->input_dev);
}

static int coordinate_mag = -1;
static ssize_t coordinate_mag_show(struct device_driver *drv, char *buf)
{
	return sprintf(buf, "coordinate_mag = %d\n",coordinate_mag);
}

static ssize_t coordinate_mag_store(struct device_driver *drv, const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
	if ((status != 0) && (status != 1)) {
		printk(KERN_ERR "[%s %d] Must be 1 or 0\n", __func__, __LINE__);
    } else {
    	coordinate_mag = status;
        if (coordinate_mag == 1) {
            lsm303dlh_mag_misc_data->pdata->axis_map_x = 0;
            lsm303dlh_mag_misc_data->pdata->axis_map_y = 1;
            lsm303dlh_mag_misc_data->pdata->axis_map_z = 2;
            
            lsm303dlh_mag_misc_data->pdata->negate_x = 0;
            lsm303dlh_mag_misc_data->pdata->negate_y = 1;
            lsm303dlh_mag_misc_data->pdata->negate_z = 1;
                
        } else if (coordinate_mag == 0) {
            lsm303dlh_mag_misc_data->pdata->axis_map_x = 1;
            lsm303dlh_mag_misc_data->pdata->axis_map_y = 0;
            lsm303dlh_mag_misc_data->pdata->axis_map_z = 2;
            
            lsm303dlh_mag_misc_data->pdata->negate_x = 0;
            lsm303dlh_mag_misc_data->pdata->negate_y = 0;
            lsm303dlh_mag_misc_data->pdata->negate_z = 1;

        }
    }    
	return count;
}

static DRIVER_ATTR(coordinate_mag, 0644, coordinate_mag_show, coordinate_mag_store);

static int lsm303dlh_mag_enable(struct lsm303dlh_mag_data *mag)
{
	int err;

	if (!atomic_cmpxchg(&mag->enabled, 0, 1)) {

		err = lsm303dlh_mag_device_power_on(mag);
		if (err < 0) {
			atomic_set(&mag->enabled, 0);
			return err;
		}
		schedule_delayed_work(&mag->input_work,
				      msecs_to_jiffies(mag->
						       pdata->poll_interval));
	}

	return 0;
}
static int lsm303dlh_mag_disable(struct lsm303dlh_mag_data *mag)
{

    int err = 0;
	if (atomic_cmpxchg(&mag->enabled, 1, 0)) {
		cancel_delayed_work_sync(&mag->input_work);

		err = lsm303dlh_mag_device_power_off(mag);
		if(err < 0){
			printk(KERN_ERR "%s,%d error",__func__,__LINE__);
			return err;
		}
		
		
	}

	return 0;
}

static int lsm303dlh_mag_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = lsm303dlh_mag_misc_data;
    atomic_inc(&open_num);   
	return 0;
}

static int lsm303dlh_mag_misc_close(struct inode *inode, struct file *file)
{
    int err = 0;
    atomic_dec(&open_num);  
    if ((atomic_read(&open_num) == 0) || (atomic_read(&open_num) < 0)) {
		err = lsm303dlh_mag_disable(lsm303dlh_mag_misc_data);
		if(err < 0){
		        printk(KERN_ERR "%s,%d error",__func__,__LINE__);
		        return err;
		}
	}
    return 0;
}

/* Begin: leyihua modified for Linux Kernel 3.0, begin 2011/11/26 */
static long lsm303dlh_mag_misc_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	u8 buf[4];
	int err;
	int interval;    
    
	struct lsm303dlh_mag_data *mag = file->private_data;

	switch (cmd) {
	case LSM303DLH_MAG_IOCTL_GET_DELAY:
		interval = mag->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;
	case LSM303DLH_MAG_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < 0 || interval > 1500)
			return -EINVAL;

		mutex_lock(&mag->lock);
		mag->pdata->poll_interval =
		    max(interval, mag->pdata->min_interval);
		err = lsm303dlh_mag_update_odr(mag, mag->pdata->poll_interval);
		mutex_unlock(&mag->lock);
		/* TODO: if update fails poll is still set */
		if (err < 0)
			return err;

		break;
	case LSM303DLH_MAG_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval > 1)
			return -EINVAL;
		if (interval == 1) {
            atomic_inc(&use_num);   
		}  else if (atomic_read(&use_num) > 0) {
			atomic_dec(&use_num);
        }    
		if (atomic_read(&use_num)) {
			if (!(atomic_read(&mag->enabled))) {
				printk(KERN_ERR "[%s], enable, use_num %d",__func__ ,atomic_read(&use_num));
				err = lsm303dlh_mag_enable(mag);
				if(err < 0){
					printk(KERN_ERR "%s,%d error",__func__,__LINE__);
					return err;
				}		
			}
        } else {
			printk(KERN_ERR "[%s], disable, use_num %d",__func__ ,atomic_read(&use_num));
			err = lsm303dlh_mag_disable(mag);
			if(err < 0){
				printk(KERN_ERR "%s,%d error",__func__,__LINE__);
				return err;
			}
		}
		break;
	case LSM303DLH_MAG_IOCTL_GET_ENABLE:
		interval = atomic_read(&mag->enabled);
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;

		break;
	case LSM303DLH_MAG_IOCTL_SET_H_RANGE:
		if (copy_from_user(&buf, argp, 1))
			return -EFAULT;
		mutex_lock(&mag->lock);
		err = lsm303dlh_mag_update_h_range(mag, buf[0]);
		mutex_unlock(&mag->lock);
		if (err < 0)
			return err;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}
/* End: leyihua modified for Linux Kernel 3.0, end 2011/11/26 */

static const struct file_operations lsm303dlh_mag_misc_fops = {
	.owner = THIS_MODULE,
	.open = lsm303dlh_mag_misc_open,
    /* Begin: leyihua modified for Linux Kernel 3.0, begin 2011/11/26 */
    .unlocked_ioctl = lsm303dlh_mag_misc_ioctl,
    /* End: leyihua modified for Linux Kernel 3.0, end 2011/11/26 */
	.release = lsm303dlh_mag_misc_close,
};

static struct miscdevice lsm303dlh_mag_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &lsm303dlh_mag_misc_fops,
};

static void lsm303dlh_mag_input_work_func(struct work_struct *work)
{
	struct lsm303dlh_mag_data *mag;
	int xyz[3] = { 0 };
	int err;

	mag  = lsm303dlh_mag_misc_data;

	mutex_lock(&mag->lock);
	err = lsm303dlh_mag_get_acceleration_data(mag, xyz);
	if (err < 0)
		dev_err(&mag->client->dev, "get_magnetometer_data failed\n");
	else
		lsm303dlh_mag_report_values(mag, xyz);

	schedule_delayed_work(&mag->input_work,
			      msecs_to_jiffies(mag->pdata->poll_interval));
	mutex_unlock(&mag->lock);
}

#ifdef LSMS303DLH_MAG_OPEN_ENABLE
int lsm303dlh_mag_input_open(struct input_dev *input)
{
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;

	return lsm303dlh_mag_enable(mag);
}

void lsm303dlh_mag_input_close(struct input_dev *dev)
{
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;

	lsm303dlh_mag_disable(mag);
}
#endif

static int lsm303dlh_mag_validate_pdata(struct lsm303dlh_mag_data *mag)
{
	mag->pdata->poll_interval = max(mag->pdata->poll_interval,
					mag->pdata->min_interval);

	if (mag->pdata->axis_map_x > 2 ||
	    mag->pdata->axis_map_y > 2 || mag->pdata->axis_map_z > 2) {
		dev_err(&mag->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			mag->pdata->axis_map_x, mag->pdata->axis_map_y,
			mag->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (mag->pdata->negate_x > 1 || mag->pdata->negate_y > 1 ||
	    mag->pdata->negate_z > 1) {
		dev_err(&mag->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			mag->pdata->negate_x, mag->pdata->negate_y,
			mag->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (mag->pdata->poll_interval < mag->pdata->min_interval) {
		dev_err(&mag->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lsm303dlh_mag_input_init(struct lsm303dlh_mag_data *mag)
{
	int err;

	INIT_DELAYED_WORK(&mag->input_work, lsm303dlh_mag_input_work_func);

	mag->input_dev = sensor_dev;
	if ((mag->input_dev == NULL)||((mag->input_dev->id.vendor != GS_ADIX345)&&(mag->input_dev->id.vendor != GS_ST35DE))) {   //NOTICE(suchangyu): we only accept already known input device
	  err = -ENOMEM;
	  printk(KERN_ERR "akm8973_probe: Failed to allocate input device\n");
	  goto err0;
	}

#ifdef LSMS303DLH_MAG_OPEN_ENABLE
	mag->input_dev->open = lsm303dlh_mag_input_open;
	mag->input_dev->close = lsm303dlh_mag_input_close;
#endif

	set_bit(EV_ABS, mag->input_dev->evbit);

	input_set_abs_params(mag->input_dev, ABS_HAT0X, -H_MAX, H_MAX, FUZZ, FLAT);
	input_set_abs_params(mag->input_dev, ABS_HAT0Y, -H_MAX, H_MAX, FUZZ, FLAT);
	input_set_abs_params(mag->input_dev, ABS_BRAKE, -H_MAX, H_MAX, FUZZ, FLAT);

	return 0;

err0:
	return err;
}

static int lsm303dlh_mag_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct lsm303dlh_mag_data *mag;
	int err = -1;
    int i;
	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto err0;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err0;
	}

	mag = kzalloc(sizeof(*mag), GFP_KERNEL);
	if (mag == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err0;
	}

	mutex_init(&mag->lock);
	mutex_lock(&mag->lock);
	mag->client = client;

	mag->pdata = kmalloc(sizeof(*mag->pdata), GFP_KERNEL);
	if (mag->pdata == NULL)
		goto err1;

	memcpy(mag->pdata, client->dev.platform_data, sizeof(*mag->pdata));

	err = lsm303dlh_mag_validate_pdata(mag);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto err1_1;
	}

	i2c_set_clientdata(client, mag);

	if (mag->pdata->init) {
		err = mag->pdata->init();
		if (err < 0)
			goto err1_1;
	}

	memset(mag->resume_state, 0, ARRAY_SIZE(mag->resume_state));

	mag->resume_state[0] = 0x10;
	mag->resume_state[1] = 0x20;
	mag->resume_state[2] = 0x00;

	err = lsm303dlh_mag_device_power_on(mag);
	if (err < 0)
		goto err2;

	atomic_set(&mag->enabled, 1);

	err = lsm303dlh_mag_update_h_range(mag, mag->pdata->h_range);
	if (err < 0) {
		dev_err(&client->dev, "update_h_range failed\n");
		goto err2;
	}

	err = lsm303dlh_mag_update_odr(mag, mag->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err2;
	}

	err = lsm303dlh_mag_input_init(mag);
	if (err < 0)
		goto err3;

	lsm303dlh_mag_misc_data = mag;

	err = misc_register(&lsm303dlh_mag_misc_device);
	if (err < 0) {
		dev_err(&client->dev, "lsm_mag_device register failed\n");
		goto err3;
	}

    for (i = 0; i < 3; i++) {
		who_am_i = i2c_smbus_read_word_data(mag->client, 0xF);
		if (who_am_i == WHO_AM_I) {
            printk(KERN_DEBUG "[%s] the compass chip is LSM303DLM", __func__);
            break;
		}
    }

    if (who_am_i != WHO_AM_I) {
        printk(KERN_DEBUG "[%s] who_am_i = %d, the compass chip is LSM303DLH",__func__,who_am_i);
    }    

	lsm303dlh_mag_device_power_off(mag);

	/* As default, do not report information */
	atomic_set(&mag->enabled, 0);

	mutex_unlock(&mag->lock);
	atomic_set(&use_num, 0);   
	atomic_set(&open_num, 0);   

	#ifdef CONFIG_HAS_EARLYSUSPEND
	lsm303dlh_mag_earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	lsm303dlh_mag_earlysuspend.suspend = lsm303dlh_mag_early_suspend;
	lsm303dlh_mag_earlysuspend.resume = lsm303dlh_mag_late_resume;
    	register_early_suspend(&lsm303dlh_mag_earlysuspend);
	#endif
	
	printk(KERN_ERR "lsm303dlh_mag probed\n");
    //lsm303dlh_mag_enable(mag);    

	return 0;

err3:
	lsm303dlh_mag_device_power_off(mag);
err2:
	if (mag->pdata->exit)
		mag->pdata->exit();
err1_1:
	mutex_unlock(&mag->lock);
	kfree(mag->pdata);
err1:
	kfree(mag);
err0:
	return err;
}

static int __devexit lsm303dlh_mag_remove(struct i2c_client *client)
{
	/* TODO: revisit ordering here once _probe order is finalized */
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;

	#ifdef CONFIG_HAS_EARLYSUSPEND
    	unregister_early_suspend(&lsm303dlh_mag_earlysuspend);
	#endif
	
	misc_deregister(&lsm303dlh_mag_misc_device);
	lsm303dlh_mag_device_power_off(mag);
	if (mag->pdata->exit)
		mag->pdata->exit();
	kfree(mag->pdata);
	kfree(mag);

	return 0;
}

#ifndef CONFIG_HAS_EARLYSUSPEND
static int lsm303dlh_mag_resume(struct i2c_client *client)
{
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;

	if (mag->on_before_suspend)
		return lsm303dlh_mag_enable(mag);
	return 0;
}

static int lsm303dlh_mag_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;

	mag->on_before_suspend = atomic_read(&mag->enabled);
	return lsm303dlh_mag_disable(mag);
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lsm303dlh_mag_early_suspend(struct early_suspend *h)
{
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;
	printk(KERN_ERR "%s enter! \n",__func__);
	mag->on_before_suspend = atomic_read(&mag->enabled);
	lsm303dlh_mag_disable(mag);
}

static void lsm303dlh_mag_late_resume(struct early_suspend *h)
{
	struct lsm303dlh_mag_data *mag = lsm303dlh_mag_misc_data;
	printk(KERN_ERR "%s enter !\n",__func__);
	if (mag->on_before_suspend)
		lsm303dlh_mag_enable(mag);
}
#endif


static const struct i2c_device_id lsm303dlh_mag_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lsm303dlh_mag_id);

static struct i2c_driver lsm303dlh_mag_driver = {
	.driver = {
		   .name = NAME,
		   },
	.probe = lsm303dlh_mag_probe,
	.remove = __devexit_p(lsm303dlh_mag_remove),
	#ifndef CONFIG_HAS_EARLYSUSPEND
	.resume = lsm303dlh_mag_resume,
	.suspend = lsm303dlh_mag_suspend,
	#endif
	.id_table = lsm303dlh_mag_id,
};

static int __init lsm303dlh_mag_init(void)
{
	int ret;
    ret = i2c_add_driver(&lsm303dlh_mag_driver);
    if (ret) {
        printk(KERN_ERR "[%s %d] add lsm303dlh_mag failed", __func__, __LINE__);
		return ret;
    }    
    ret = driver_create_file(&lsm303dlh_mag_driver.driver, &driver_attr_coordinate_mag);
	if (ret < 0) {
		printk(KERN_ERR "[%s %d] create lsm303dlh_mag coordinate sys failed", __func__, __LINE__);
        return ret;
    }
    
	return 0;
}

static void __exit lsm303dlh_mag_exit(void)
{
	i2c_del_driver(&lsm303dlh_mag_driver);
	return;
}

__define_initcall("7s",lsm303dlh_mag_init,7s);
module_exit(lsm303dlh_mag_exit);

MODULE_DESCRIPTION("lsm303dlh driver for the magnetometer section");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");
