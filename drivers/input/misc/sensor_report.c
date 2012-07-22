#include <linux/uaccess.h>
#include <linux/init.h>
#include <asm/irq.h> 
#include <asm/io.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <mach/board.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/input/sensor_report.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)		(sizeof(a)/sizeof(a[0]))
#endif


static struct input_dev *sensor_daemon_dev;

#ifdef CONFIG_HAS_EARLYSUSPEND
unsigned int sensor_report_dev_alive_status = 1;
struct early_suspend snr_report_early_suspend;
#endif

DECLARE_WAIT_QUEUE_HEAD(sensor_queue);
sensor_cal_data sensor_data;

void report_data(sensor_cal_data sensor_data)
{
	input_report_abs(sensor_daemon_dev, ABS_THROTTLE, sensor_data.rotation_x);	 //ROTATION
	input_report_abs(sensor_daemon_dev, ABS_RUDDER, sensor_data.rotation_y);
	input_report_abs(sensor_daemon_dev, ABS_WHEEL, sensor_data.rotation_z);

	input_report_abs(sensor_daemon_dev, ABS_HAT0X, sensor_data.mag_x); 		//MagneticField
	input_report_abs(sensor_daemon_dev, ABS_HAT0Y, sensor_data.mag_y);
	input_report_abs(sensor_daemon_dev, ABS_BRAKE, sensor_data.mag_z);

	input_report_abs(sensor_daemon_dev, ABS_RX, sensor_data.ori_x); 	//Orientation
	input_report_abs(sensor_daemon_dev, ABS_RY, sensor_data.ori_y);
	input_report_abs(sensor_daemon_dev, ABS_RZ, sensor_data.ori_z);
  
	input_report_abs(sensor_daemon_dev, ABS_X, sensor_data.linear_x);		//LINEAR_ACC
	input_report_abs(sensor_daemon_dev, ABS_Y, sensor_data.linear_y);
	input_report_abs(sensor_daemon_dev, ABS_Z, sensor_data.linear_z);
    
	input_report_abs(sensor_daemon_dev, ABS_HAT1X, sensor_data.gravity_x);	//GRAVITY
	input_report_abs(sensor_daemon_dev, ABS_HAT1Y, sensor_data.gravity_y);
	input_report_abs(sensor_daemon_dev, ABS_GAS, sensor_data.gravity_z);

	input_report_abs(sensor_daemon_dev, ABS_MISC, sensor_data.status);   	
    
	input_sync(sensor_daemon_dev);

}

/* Begin: leyihua modify ioctl for sensor report, 2011/12/11 */
static long sensor_daemon_misc_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	switch (cmd) {
		case SENSOR_IOCTL_SEND_DATA:
			#ifdef CONFIG_HAS_EARLYSUSPEND
			if(sensor_report_dev_alive_status)
			{
			#endif
			if (copy_from_user(&sensor_data, argp, sizeof(sensor_data))) {
				return -EFAULT;
            		}
			report_data(sensor_data);
			#ifdef CONFIG_HAS_EARLYSUSPEND
			}
			#endif
			break;

		default:
			return -EINVAL;
	}

	return 0;
}


static const struct file_operations sensor_daemon_misc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = sensor_daemon_misc_ioctl,
};
/* End: leyihua modify ioctl for sensor report, 2011/12/11 */

static struct miscdevice sensor_daemon_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sensor_report",
	.fops = &sensor_daemon_misc_fops,
};


#ifdef CONFIG_HAS_EARLYSUSPEND
static void report_early_suspend(struct early_suspend *h)
{
	printk(KERN_ERR "%s enter! \n",__func__);
	sensor_report_dev_alive_status = 0;
}

static void report_late_resume(struct early_suspend *h)
{
	printk(KERN_ERR "%s enter !\n",__func__);
	sensor_report_dev_alive_status = 1;
}
#endif


static int __init sensor_daemon_init(void) 
{
    int error;
   
	sensor_daemon_dev = input_allocate_device();
	if (!sensor_daemon_dev) { 
        printk(KERN_ERR "[%s] input_allocate_device failed\n", __func__);
		error = -ENOMEM;
    }

	sensor_daemon_dev->name = "sensor_report";
    
	set_bit(EV_ABS, sensor_daemon_dev->evbit);

	/* Begin: leyihua modify set bits for sensor report, 2011/12/11 */
	/* set Orientation bits */
	input_set_abs_params(sensor_daemon_dev, ABS_RX, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_RY, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_RZ, 0, 0, 0, 0);
	
	/* set MagneticField bits */
	input_set_abs_params(sensor_daemon_dev, ABS_HAT0X, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_HAT0Y, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_BRAKE, 0, 0, 0, 0);

	/* set ROTATION bits */
	input_set_abs_params(sensor_daemon_dev, ABS_THROTTLE, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_RUDDER, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_WHEEL, 0, 0, 0, 0);

	/* set LINEAR_ACC bits */
	input_set_abs_params(sensor_daemon_dev, ABS_X, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_Y, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_Z, 0, 0, 0, 0);

	/* set GRAVITY bits */
	input_set_abs_params(sensor_daemon_dev, ABS_HAT1X, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_HAT1Y, 0, 0, 0, 0);
	input_set_abs_params(sensor_daemon_dev, ABS_GAS, 0, 0, 0, 0);

	/* set bits for compass calibration data */
	input_set_abs_params(sensor_daemon_dev, ABS_MISC, 0, 0, 0, 0);
	/* End: leyihua modify set bits for sensor report, 2011/12/11 */
	
	error = input_register_device(sensor_daemon_dev);
	if (error) {
        printk(KERN_ERR "[%s] Failed to register device\n", __func__);
		goto err_free_dev; 
    }

 
	error = misc_register(&sensor_daemon_misc_device);
	if (error < 0) {
		printk(KERN_ERR "[%s]  register failed\n", __func__);
		goto err_free_dev;
	}

	#ifdef CONFIG_HAS_EARLYSUSPEND
	sensor_report_dev_alive_status = 1;
	snr_report_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	snr_report_early_suspend.suspend = report_early_suspend;
	snr_report_early_suspend.resume = report_late_resume;
    	register_early_suspend(&snr_report_early_suspend);
	#endif
	return 0;
    
 err_free_dev:
	input_free_device(sensor_daemon_dev);
	return error; 
}

static void __exit sensor_daemon_exit(void) 
{ 
	#ifdef CONFIG_HAS_EARLYSUSPEND
	sensor_report_dev_alive_status = 0;
    	unregister_early_suspend(&snr_report_early_suspend);
	#endif
	
	misc_deregister(&sensor_daemon_misc_device);
    	input_unregister_device(sensor_daemon_dev);
	input_free_device(sensor_daemon_dev);
}

module_init(sensor_daemon_init);
module_exit(sensor_daemon_exit);

