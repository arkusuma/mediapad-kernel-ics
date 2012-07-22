/* kernel/drivers/input/misc/l3g4200d.c */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/input/l3g4200d.h>
#include "linux/hardware_self_adapt.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* 0: interrupt mode; 1: poll mode */
#define POLL_MODE 1
#define DEBUG	0

#define GPIO_INT1		(149)
/*
 * L3G4200D gyroscope data
 * brief structure containing gyroscope values for yaw, pitch and roll in
 * signed short
 */

struct l3g4200d_t {
	short	x;	/* x-axis angular rate data. Range -2048 to 2047. */
	short	y;	/* y-axis angluar rate data. Range -2048 to 2047. */
	short	z;	/* z-axis angular rate data. Range -2048 to 2047. */
};

/* static struct i2c_client *l3g4200d_client; */

struct l3g4200d_data {
	struct i2c_client *client;    
	struct input_dev *input_dev;    
	struct hrtimer timer;    
	struct work_struct work;
	struct l3g4200d_platform_data *pdata;
};

extern struct input_dev *sensor_dev;

static struct l3g4200d_data *gyro;
static atomic_t gy_flag;
static atomic_t use_num;
static atomic_t open_num;

static struct i2c_driver l3g4200d_driver;

#ifdef CONFIG_HAS_EARLYSUSPEND
struct early_suspend l3g4200d_earlysuspend;
static void l3g4200d_early_suspend(struct early_suspend *h);
static void l3g4200d_late_resume(struct early_suspend *h);
#endif

static int gy_delay = 30;
static char l3g4200d_i2c_write(unsigned char reg_addr,
				    unsigned char *data,
				    unsigned char len);

static char l3g4200d_i2c_read(unsigned char reg_addr,
				   unsigned char *data,
				   unsigned char len);

/* set l3g4200d digital gyroscope bandwidth */
int l3g4200d_set_bandwidth(char bw)
{
	int res = 0;
	unsigned char data;

	res = i2c_smbus_read_word_data(gyro->client, CTRL_REG1);
	if (res >= 0)
		data = res & 0x000F;

	data = data + bw;
	res = l3g4200d_i2c_write(CTRL_REG1, &data, 1);
	return res;
}

/* read selected bandwidth from l3g4200d */
int l3g4200d_get_bandwidth(unsigned char *bw)
{
	int res = 0;    
	res = i2c_smbus_read_byte_data(gyro->client, CTRL_REG1);

	return res & 0xF0;
}

int l3g4200d_set_mode(char mode)
{
	int res = 0;
	unsigned char data;

	res = i2c_smbus_read_word_data(gyro->client, CTRL_REG1);
	if (res >= 0)
		data = res & 0x00F7;

	data = mode + data;

	res = l3g4200d_i2c_write(CTRL_REG1, &data, 1);
	return res;
}

int l3g4200d_set_range(char range)
{
	int res = 0;
	unsigned char data;
	
	res = i2c_smbus_read_word_data(gyro->client, CTRL_REG4);
	if (res >= 0)
		data = res & 0x00CF;

	data = range + data;
	res = l3g4200d_i2c_write(CTRL_REG4, &data, 1);
	return res;

}

#if DEBUG
static ssize_t delay_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{

	return sprintf(buf, "delay =  %d\n",gy_delay);
}

static ssize_t delay_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	gy_delay = simple_strtoul(buf, NULL, 0);

    printk(KERN_ERR "set delay  = %d\n", gy_delay);
	return count;
}

static DEVICE_ATTR(delay, 0777, delay_show, delay_store);

static ssize_t ctrl1_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int status;
    
	status = i2c_smbus_read_byte_data(gyro->client, 0x20);

	return sprintf(buf, "status =  0x%x\n",status);
}

static ssize_t ctrl1_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
    printk(KERN_ERR "status  = 0x%x\n", status);

	i2c_smbus_write_byte_data(gyro->client, 0x20, status);    

	return count;
}

static DEVICE_ATTR(ctrl1, 0777, ctrl1_show, ctrl1_store);

static ssize_t ctrl2_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int status;
    
	status = i2c_smbus_read_byte_data(gyro->client, 0x21);

	return sprintf(buf, "status =  0x%x\n",status);
}

static ssize_t ctrl2_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
    printk(KERN_ERR "status  = 0x%x\n", status);

	i2c_smbus_write_byte_data(gyro->client, 0x21, status);    

	return count;
}

static DEVICE_ATTR(ctrl2, 0777, ctrl2_show, ctrl2_store);

static ssize_t ctrl3_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int status;
    
	status = i2c_smbus_read_byte_data(gyro->client, 0x22);

	return sprintf(buf, "status =  0x%x\n",status);
}

static ssize_t ctrl3_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
    printk(KERN_ERR "status  = 0x%x\n", status);

	i2c_smbus_write_byte_data(gyro->client, 0x22, status);    

	return count;
}

static DEVICE_ATTR(ctrl3, 0777, ctrl3_show, ctrl3_store);

static ssize_t ctrl4_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int status;
    
	status = i2c_smbus_read_byte_data(gyro->client, 0x23);

	return sprintf(buf, "status =  0x%x\n",status);
}

static ssize_t ctrl4_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
    printk(KERN_ERR "status  = 0x%x\n", status);

	i2c_smbus_write_byte_data(gyro->client, 0x23, status);    

	return count;
}

static DEVICE_ATTR(ctrl4, 0777, ctrl4_show, ctrl4_store);

static ssize_t ctrl5_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int status;
    
	status = i2c_smbus_read_byte_data(gyro->client, 0x24);

	return sprintf(buf, "status =  0x%x\n",status);
}

static ssize_t ctrl5_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
    printk(KERN_ERR "status  = 0x%x\n", status);

	i2c_smbus_write_byte_data(gyro->client, 0x24, status);    

	return count;
}

static DEVICE_ATTR(ctrl5, 0777, ctrl5_show, ctrl5_store);

static ssize_t fifo_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int status;
    
	status = i2c_smbus_read_byte_data(gyro->client, 0x2E);

	return sprintf(buf, "status =  0x%x\n",status);
}

static ssize_t fifo_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	status = simple_strtoul(buf, NULL, 0);
    printk(KERN_ERR "status  = 0x%x\n", status);

	i2c_smbus_write_byte_data(gyro->client, 0x2E, status);    

	return count;
}

static DEVICE_ATTR(fifo, 0777, fifo_show, fifo_store);
#endif

static int coordinate_gyro = -1;
static ssize_t coordinate_gyro_show(struct device_driver *drv, char *buf)
{
	return sprintf(buf, "coordinate_gyro = %d\n",coordinate_gyro);
}

static ssize_t coordinate_gyro_store(struct device_driver *drv, const char *buf, size_t count)
{
	int status;
    if (buf == NULL) {
		return count;
    }
	status = simple_strtoul(buf, NULL, 0);
	if ((status != 0) && (status != 1)) {
		printk(KERN_ERR "[%s %d] Must be 1 or 0\n", __func__, __LINE__);
    } else {
    	coordinate_gyro = status;
        if (coordinate_gyro == 1) {
            gyro->pdata->axis_map_x = 0;
            gyro->pdata->axis_map_y = 1;
            gyro->pdata->axis_map_z = 2;
            
            gyro->pdata->negate_x = 0;
            gyro->pdata->negate_y = 1;
            gyro->pdata->negate_z = 1;
                
        } else if (coordinate_gyro == 0) {
            gyro->pdata->axis_map_x = 1;
            gyro->pdata->axis_map_y = 0;
            gyro->pdata->axis_map_z = 2;
            
            gyro->pdata->negate_x = 0;
            gyro->pdata->negate_y = 0;
            gyro->pdata->negate_z = 1;

        }
    }    
	return count;
}

static DRIVER_ATTR(coordinate_gyro, 0644, coordinate_gyro_show, coordinate_gyro_store);

int l3g4200d_read_gyro_values(struct l3g4200d_t *data)
{
	int res;
    unsigned char gyro_data[6];
    short hw_d[3] = {0};

	res = l3g4200d_i2c_read(AXISDATA_REG, &gyro_data[0], 6);
#if DEBUG
		printk(KERN_ERR "X_L = 0x%x\n", gyro_data[0]);
		printk(KERN_ERR "X_H = 0x%x\n", gyro_data[1]);
		printk(KERN_ERR "Y_L = 0x%x\n", gyro_data[2]);
		printk(KERN_ERR "Y_H = 0x%x\n", gyro_data[3]);
		printk(KERN_ERR "Z_L = 0x%x\n", gyro_data[4]);
		printk(KERN_ERR "Z_H = 0x%x\n", gyro_data[5]);
#endif
		hw_d[0] = (short) (((gyro_data[1]) << 8) | gyro_data[0]);
		hw_d[1] = (short) (((gyro_data[3]) << 8) | gyro_data[2]);
		hw_d[2] = (short) (((gyro_data[5]) << 8) | gyro_data[4]);

        data->x = ((gyro->pdata->negate_x) ? (-hw_d[gyro->pdata->axis_map_x])
              : (hw_d[gyro->pdata->axis_map_x]));
        data->y = ((gyro->pdata->negate_y) ? (-hw_d[gyro->pdata->axis_map_y])
              : (hw_d[gyro->pdata->axis_map_y]));
        data->z = ((gyro->pdata->negate_z) ? (-hw_d[gyro->pdata->axis_map_z])
              : (hw_d[gyro->pdata->axis_map_z]));
#if !POLL_MODE
        i2c_smbus_read_byte_data(gyro->client, INT1_SRC);        
#endif
		return 0;
}

static int device_init(void)
{
	int res;
	unsigned char buf[5];
#if POLL_MODE
	buf[0] = 0x0F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0xB0;
	buf[4] = 0x00;
	res = l3g4200d_i2c_write(CTRL_REG1, &buf[0], 5);
#else
	buf[0] = 0x0F;	//100Hz 12.5 cut off
	buf[1] = 0x00;	//default.
	buf[2] = 0x80;	//int1+Push Pull
	buf[3] = 0xB0;	//BDU + 2000dps
	buf[4] = 0x18;	//00011000,out data no filter at all//LPF and HPF on Interrupt.
    res = l3g4200d_i2c_write(CTRL_REG1, &buf[0], 5);

	i2c_smbus_write_byte_data(gyro->client, INT1_CFG, 0x6A);     
    i2c_smbus_read_byte_data(gyro->client, INT1_SRC);        
	i2c_smbus_write_byte_data(gyro->client, INT1_THS_XL, 50);    
	i2c_smbus_write_byte_data(gyro->client, INT1_THS_XH, 10);    
	i2c_smbus_write_byte_data(gyro->client, INT1_THS_YL, 50);    
	i2c_smbus_write_byte_data(gyro->client, INT1_THS_YH, 10);    
	i2c_smbus_write_byte_data(gyro->client, INT1_THS_YL, 50);    
	i2c_smbus_write_byte_data(gyro->client, INT1_THS_YH, 10);    
	i2c_smbus_write_byte_data(gyro->client, INT1_DURATION, 0);    
#endif
	return res;
}

#if !POLL_MODE
static int gyro_config_pin(void)
{
	
	int rc;  

	rc = gpio_request(GPIO_INT1, "l3g4200d_irq_gpio");
	if (rc) {
	    pr_err("%s: unable to request gpio %d\n",
	        __func__, GPIO_INT1);
	    goto error_irq_gpio;
	}

	rc = gpio_direction_input(GPIO_INT1);
	if (rc) {
	    pr_err("%s: unable to set direction for gpio %d\n",
	        __func__, GPIO_INT1);
	    goto error_irq_gpio;
	}

	return 0;

error_irq_gpio:
	gpio_free(GPIO_INT1);

    return -1;
}
#endif

static char l3g4200d_i2c_write(unsigned char reg_addr,
				    unsigned char *data,
				    unsigned char len)
{
	int dummy;
	int i;

	if (gyro->client == NULL)  
		return -1;
    
	for (i = 0; i < len; i++) {        
		dummy = i2c_smbus_write_byte_data(gyro->client,
						  reg_addr++, data[i]);        
		if (dummy) {
			printk(KERN_ERR "i2c write error\n");
			return dummy;
		}		
    }
	return 0;
}

static char l3g4200d_i2c_read(unsigned char reg_addr,
				   unsigned char *data,
				   unsigned char len)
{
	int dummy = 0;
	int i = 0;

	if (gyro->client == NULL)  
		return -1;
	while (i < len) {
        
		dummy = i2c_smbus_read_word_data(gyro->client, reg_addr++);        
		if (dummy >= 0) {
            
			data[i] = dummy & 0x00ff;            
			i++;
		} else {
			printk(KERN_ERR" i2c read error\n ");
			return dummy;
		}
		dummy = len;		
    }
	return dummy;
}

static int l3g4200d_enable(struct l3g4200d_data *gyro)
{
	if (!atomic_cmpxchg(&gy_flag, 0, 1)) {

        i2c_smbus_write_byte_data(gyro->client, CTRL_REG1, 0x0F);
#if !POLL_MODE	
        enable_irq(gyro->client->irq);
#else
        hrtimer_start(&gyro->timer, ktime_set(0, gy_delay * NSEC_PER_MSEC), HRTIMER_MODE_REL);
#endif
	}

	return 0;
}

static int l3g4200d_disable(struct l3g4200d_data *gyro)
{
	if (atomic_cmpxchg(&gy_flag, 1, 0)) {
    	i2c_smbus_write_byte_data(gyro->client, CTRL_REG1, 0x00);
#if !POLL_MODE
        disable_irq(gyro->client->irq);
#else	
        hrtimer_cancel(&gyro->timer);
#endif
        cancel_work_sync(&gyro->work);
	}

	return 0;
}


static int l3g4200d_open(struct inode *inode, struct file *file)
{
	if (gyro->client == NULL) {
		printk(KERN_ERR "I2C driver not install\n");
		return -1;
	}

	device_init();
#if !POLL_MODE
	enable_irq(gyro->client->irq);
#else 
	hrtimer_start(&gyro->timer, ktime_set(0, gy_delay * NSEC_PER_MSEC), HRTIMER_MODE_REL);
#endif

    atomic_inc(&open_num);   

	return 0;
}

static int l3g4200d_close(struct inode *inode, struct file *file)
{
    atomic_dec(&open_num);   
    if ((atomic_read(&open_num) == 0) || (atomic_read(&open_num) < 0)) {
		l3g4200d_disable(gyro);	
    }    

	return 0;
}

/* Begin: leyihua modify ioctl for sensor report, 2011/12/11 */
static long l3g4200d_ioctl(struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	int err = 0;
    void __user *argp = (void __user *)arg;
	unsigned char data[6];
	short flag;
    int delay;

	if (gyro->client == NULL) {
		printk(KERN_ERR "I2C driver not install\n");
		return -EFAULT;
	}

	switch (cmd) {
		case L3G4200D_SET_RANGE:
			if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
				printk(KERN_ERR "copy_from_user error\n");
				return -EFAULT;
			}
			err = l3g4200d_set_range(*data);
			return err;

		case L3G4200D_SET_MODE:
			if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
				printk(KERN_ERR "copy_to_user error\n");
				return -EFAULT;
			}
			err = l3g4200d_set_mode(*data);
			return err;

		case L3G4200D_SET_BANDWIDTH:
			if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
				printk(KERN_ERR "copy_from_user error\n");
				return -EFAULT;
			}
			err = l3g4200d_set_bandwidth(*data);
			return err;

		case L3G4200D_READ_GYRO_VALUES:
			err = l3g4200d_read_gyro_values(
					(struct l3g4200d_t *)data);

			if (copy_to_user((struct l3g4200d_t *)arg,
					 (struct l3g4200d_t *)data, 6) != 0) {
				printk(KERN_ERR "copy_to error\n");
				return -EFAULT;
			}
			return err;

		case ECS_IOCTL_APP_SET_GFLAG:
	        if (copy_from_user(&flag, argp, sizeof(flag))) {                 
	            printk(KERN_ERR "copy_from error\n");
	            return -EFAULT;
            }
            
            if (flag == 1) {
                atomic_inc(&use_num);   
            }  else if (atomic_read(&use_num) > 0) {
                atomic_dec(&use_num);
            }    
            
            if (atomic_read(&use_num)) {
                printk(KERN_ERR "[%s], enable, use_num %d",__func__ ,atomic_read(&use_num));
                l3g4200d_enable(gyro);
            } else {
                printk(KERN_ERR "[%s], disable, use_num %d",__func__ ,atomic_read(&use_num));
                l3g4200d_disable(gyro);
            }
			return err;
            
        case ECS_IOCTL_APP_GET_GFLAG:  
	        flag = atomic_read(&gy_flag);       
	        if (copy_to_user(argp, &flag, sizeof(flag))) {
	            printk(KERN_ERR "copy_to error\n");
	            return -EFAULT;
	        }
			return err;
            
        case ECS_IOCTL_APP_SET_GDELAY:	
			if (copy_from_user(&delay, argp, sizeof(delay))) {                
	            printk(KERN_ERR "copy_to error\n");
				return -EFAULT;
        	}
            
            gy_delay = delay;
            
			return err;

        case ECS_IOCTL_APP_GET_GDELAY:   
            if (copy_to_user(argp, &gy_delay, sizeof(gy_delay)))
                return -EFAULT;
			return err;
            
		default:
			return 0;
	}
}
/* End: leyihua modify ioctl for sensor report, 2011/12/11 */

static void gyro_work_func(struct work_struct *work)
{
	struct l3g4200d_t data;
	int ret;
    
	if (!atomic_read(&gy_flag))
        return ;

    ret = l3g4200d_read_gyro_values(&data);
    if (!ret) {
	    input_report_abs(gyro->input_dev, ABS_RX, data.x);
	    input_report_abs(gyro->input_dev, ABS_RY, data.y);
	    input_report_abs(gyro->input_dev, ABS_RZ, data.z);
	    input_sync(gyro->input_dev);
	}
    
#if POLL_MODE    
    hrtimer_start(&gyro->timer, ktime_set(0, gy_delay * NSEC_PER_MSEC), HRTIMER_MODE_REL);
#else
   enable_irq(gyro->client->irq);
#endif
}

#if POLL_MODE
static enum hrtimer_restart gyro_timer_func(struct hrtimer *timer)
{
	schedule_work(&gyro->work);
	return HRTIMER_NORESTART;
}
#endif

#if !POLL_MODE
static irqreturn_t gyro_irq_handler(int irq, void *dev_id)
{
	struct l3g4200d_data *pdata = dev_id;
    disable_irq_nosync(pdata->client->irq);
	schedule_work(&pdata->work);

    return IRQ_HANDLED;
}
#endif

static const struct file_operations l3g4200d_fops = {
	.owner = THIS_MODULE,
	.open = l3g4200d_open,
	.release = l3g4200d_close,
	/* Begin: leyihua modify ioctl for sensor report, 2011/12/11 */
	.unlocked_ioctl = l3g4200d_ioctl,
	/* End: leyihua modify ioctl for sensor report, 2011/12/11 */
};

static struct miscdevice gyro_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gyro",
    .fops = &l3g4200d_fops,
};

static int l3g4200d_probe(struct i2c_client *client,
			       const struct i2c_device_id *devid)
{
	struct l3g4200d_data *data;
	int err = -1;

	int tempvalue;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		goto exit;

#if !POLL_MODE
    err = gyro_config_pin();
    if (err) {
        goto exit;
    }
#endif

	data = kzalloc(sizeof(struct l3g4200d_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit_kfree1;
	}

	data->pdata = kmalloc(sizeof(*data->pdata), GFP_KERNEL);
	if (data->pdata == NULL)
		goto exit_kfree;

	memcpy(data->pdata, client->dev.platform_data, sizeof(*data->pdata));

	INIT_WORK(&data->work, gyro_work_func);

	i2c_set_clientdata(client, data);
	data->client = client;

	err = i2c_smbus_read_byte(client);
    
	if (err < 0) {       
		printk(KERN_ERR "i2c_smbus_read_byte error!!\n");
		goto exit_kfree;
	} else {
		printk(KERN_INFO "L3G4200D Device detected!\n");
	}

	/* read chip id */
	tempvalue = i2c_smbus_read_byte_data(client, WHO_AM_I);

	if ((tempvalue & 0xFF) == L3G4200D_ID) {
		printk(KERN_INFO "I2C driver registered!\n");
	} else {
		data->client = NULL;
		goto exit_kfree;
	}

	gyro = data;

	gyro->input_dev = sensor_dev;
	if ((gyro->input_dev == NULL)||((gyro->input_dev->id.vendor != GS_ADIX345)&&(gyro->input_dev->id.vendor != GS_ST35DE))) {
	  err = -ENOMEM;
	  printk(KERN_ERR "l3g4200d_probe: Failed to allocate input device\n");
	  goto exit;
	}

	set_bit(EV_ABS,gyro->input_dev->evbit);
	set_bit(ABS_RX, gyro->input_dev->absbit);
	set_bit(ABS_RY, gyro->input_dev->absbit);
	set_bit(ABS_RZ, gyro->input_dev->absbit);
	set_bit(EV_SYN,gyro->input_dev->evbit);

	gyro->input_dev->id.bustype = BUS_I2C; 


	err = misc_register(&gyro_device);
	if (err) {
		printk(KERN_ERR "gyro_probe: gyro_device register failed\n");
		goto err_misc_device_register_failed;
	}

#if !POLL_MODE    
    err = request_irq(client->irq, gyro_irq_handler, IRQF_TRIGGER_RISING, client->name, gyro);
	if (err) {
		printk(KERN_ERR "[%s] request irq failed", __func__);
        goto err_misc_device_register_failed;
    }
#else 
	hrtimer_init(&gyro->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    gyro->timer.function = gyro_timer_func;
#endif

#if DEBUG
    err = device_create_file(&gyro->input_dev->dev, &dev_attr_delay);
	if (err) {
        printk(KERN_ERR "Failed to device_create_file\n");
		goto exit; 
    }

    err = device_create_file(&gyro->input_dev->dev, &dev_attr_ctrl1);
	if (err) {
        printk(KERN_ERR "Failed to device_create_file\n");
		goto exit; 
    }

	err = device_create_file(&gyro->input_dev->dev, &dev_attr_ctrl2);
	if (err) {
	    printk(KERN_ERR "Failed to device_create_file\n");
	    goto exit; 
	}

	err = device_create_file(&gyro->input_dev->dev, &dev_attr_ctrl3);
	if (err) {
	    printk(KERN_ERR "Failed to device_create_file\n");
	    goto exit; 
	}

	err = device_create_file(&gyro->input_dev->dev, &dev_attr_ctrl4);
	if (err) {
	    printk(KERN_ERR "Failed to device_create_file\n");
	    goto exit; 
	}

	err = device_create_file(&gyro->input_dev->dev, &dev_attr_ctrl5);
	if (err) {
	    printk(KERN_ERR "Failed to device_create_file\n");
	    goto exit; 
	}

	err = device_create_file(&gyro->input_dev->dev, &dev_attr_fifo);
	if (err) {
	    printk(KERN_ERR "Failed to device_create_file\n");
	    goto exit; 
	}
#endif

    err = driver_create_file(&l3g4200d_driver.driver, &driver_attr_coordinate_gyro);
    if (err < 0) {
        printk(KERN_ERR "[%s %d] create l3g4200d coordinate sys failed", __func__, __LINE__);
        return err;
    }    

	atomic_set(&gy_flag, 0);   
	atomic_set(&use_num, 0);   
    atomic_set(&open_num, 0);

	#ifdef CONFIG_HAS_EARLYSUSPEND
	l3g4200d_earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	l3g4200d_earlysuspend.suspend = l3g4200d_early_suspend;
	l3g4200d_earlysuspend.resume = l3g4200d_late_resume;
    	register_early_suspend(&l3g4200d_earlysuspend);
	#endif
	

	printk(KERN_INFO "L3G4200D device probe successfully\n");

	return 0;
    
err_misc_device_register_failed:
	misc_deregister(&gyro_device);
exit_kfree:
    kfree(data->pdata);
exit_kfree1:
	kfree(data);
exit:
	return -1;
}

static int l3g4200d_remove(struct i2c_client *client)
{  
	printk(KERN_INFO "L3G4200D driver removing\n");

	#ifdef CONFIG_HAS_EARLYSUSPEND
    	unregister_early_suspend(&l3g4200d_earlysuspend);
	#endif
#if !POLL_MODE
    free_irq(client->irq, gyro);
#else
	hrtimer_cancel(&gyro->timer);
#endif
    cancel_work_sync(&gyro->work);
	misc_deregister(&gyro_device);
	input_unregister_device(gyro->input_dev);

	kfree(gyro);
    
	return 0;
}

#ifndef CONFIG_HAS_EARLYSUSPEND
#ifdef CONFIG_PM
static int l3g4200d_suspend(struct i2c_client *client, pm_message_t state)
{
	i2c_smbus_write_byte_data(gyro->client, CTRL_REG1, 0x00);
#if !POLL_MODE
	disable_irq(client->irq);
#else	
	hrtimer_cancel(&gyro->timer);
#endif
	cancel_work_sync(&gyro->work);
	return 0;
}

static int l3g4200d_resume(struct i2c_client *client)
{
	i2c_smbus_write_byte_data(gyro->client, CTRL_REG1, 0x0F);
#if !POLL_MODE	
	enable_irq(client->irq);
#else
	hrtimer_start(&gyro->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#endif
	return 0;
}
#endif
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void l3g4200d_early_suspend(struct early_suspend *h)
{
	printk(KERN_ERR "%s enter! \n",__func__);
	i2c_smbus_write_byte_data(gyro->client, CTRL_REG1, 0x00);
#if !POLL_MODE
	disable_irq(client->irq);
#else	
	hrtimer_cancel(&gyro->timer);
#endif
	cancel_work_sync(&gyro->work);
}

static void l3g4200d_late_resume(struct early_suspend *h)
{
	printk(KERN_ERR "%s enter !\n",__func__);
	i2c_smbus_write_byte_data(gyro->client, CTRL_REG1, 0x0F);
#if !POLL_MODE	
	enable_irq(client->irq);
#else
	hrtimer_start(&gyro->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#endif
}
#endif

static const struct i2c_device_id l3g4200d_id[] = {
	{ "l3g4200d", 0 },
	{},
};

MODULE_DEVICE_TABLE(i2c, l3g4200d_id);

static struct i2c_driver l3g4200d_driver = {
	.class = I2C_CLASS_HWMON,
	.probe = l3g4200d_probe,
	.remove = __devexit_p(l3g4200d_remove),
	.id_table = l3g4200d_id,
	#ifndef CONFIG_HAS_EARLYSUSPEND
	#ifdef CONFIG_PM
	.suspend = l3g4200d_suspend,
	.resume = l3g4200d_resume,
	#endif
	#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "l3g4200d",
	},
};

static int __init l3g4200d_init(void)
{
	return i2c_add_driver(&l3g4200d_driver);
}

static void __exit l3g4200d_exit(void)
{
	i2c_del_driver(&l3g4200d_driver);
	return;
}

__define_initcall("7s",l3g4200d_init,7s);
module_exit(l3g4200d_exit);

MODULE_DESCRIPTION("l3g4200d driver");
MODULE_LICENSE("GPL");

