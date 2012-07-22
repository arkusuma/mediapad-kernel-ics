
/**************************************************
 Copyright (C), 2009-2010, Huawei Tech. Co., Ltd.
 File Name: kernel/drivers/power/po188.c
 Author:    libao     Version:  1   Date:2009
 Description:po188 driver
 Version:
 Function List:
 History:
        libao    2009    1   light sensor driver
**************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <asm/atomic.h>
#include <linux/timer.h>
#include <linux/hwmon-sysfs.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/pm.h>
#include <linux/kobject.h>
#include <linux/input.h>

#include <linux/msm_adc.h>

/* Macro definition */
#define PO188DE_DRIVER_NAME "po188"
#define PO188_NAME "po188"
#define PO188_DEV_NAME "autolight"

#define SENSOR_POLLING_JIFFIES 1*HZ   //modify the time to 1 second
#define START_SENSOR_POLLING_JIFFIES 5*HZ    //modify the time to 5 second
#define MOD_SENSOR_POLLING_JIFFIES 5*HZ   // modify the time to 5 second

#define SAMPLE_LUX_MIN            0
#define SAMPLE_LUX_MAX           256
#define TIMES    3

/* begin: added by z00168965 for light sensor */
#define PO188_IOC_MAGIC 'l'
#define LIGHTSENSOR_IOCTL_GET_ENABLED _IOR(PO188_IOC_MAGIC, 1, int *)
#define LIGHTSENSOR_IOCTL_ENABLE _IOW(PO188_IOC_MAGIC, 2, int *)
/* end: added by z00168965 for light sensor */

#define IOCTL_SEND _IO(PO188_IOC_MAGIC, 0) 
#define IOCTL_RESET_PO188 _IO(PO188_IOC_MAGIC, 1)
#define IOCTL_CLOSE_PO188 _IO(PO188_IOC_MAGIC, 2)
#define SENSORS_GET_LUX_FIR _IO(PO188_IOC_MAGIC, 3)
#define IOCTL_PO188_SLEEP_TIME _IO(PO188_IOC_MAGIC, 4)

/*Macro switch*/
//#define DEBUG_PO188
#ifdef DEBUG_PO188
#define PO188_DMSG(format, args...) printk(KERN_INFO "[%s] (line: %u) " format "\n", \
__FUNCTION__, __LINE__, ##args)
#else
#define PO188_DMSG(format, args...)
#endif

#define PO188_ERRMSG(format, args...) printk(KERN_ERR "[%s] (line: %u) " format "\n", \
__FUNCTION__, __LINE__, ##args)


/* Type definition */
struct po188de_driver_struct
{

	struct miscdevice dev;
	struct file_operations fops;
	struct workqueue_struct *po188_wq;
	struct workqueue_struct *monitor_wqueue;
	struct semaphore run_sem;
	struct task_struct *cb_thread;

	wait_queue_head_t wait_q;
	struct delayed_work monitor_work;

	bool status_on;
	struct input_dev *input_dev;
	atomic_t cb_thread_wakeup;
	atomic_t stop_cb_thread;
	atomic_t cb_thread_stopped;
	struct timer_list timer;
	struct mutex     mux_lock;
	u32 voltage_now;
       
	s32 sleep_time;
	//struct hrtimer timer;
	bool active_flag;
	spinlock_t lock;
   	int last_value;
	struct early_suspend early_suspend;
	int last_voltage;
	int current_voltage;
	bool vol_flag;
};

/* Static variable definition*/
static struct po188de_driver_struct po188_driver;

/* Function declaration */	
static void po188_wait_for_event(struct work_struct *work);
static DECLARE_WORK(po188_cb_work, po188_wait_for_event);

/* G-sensor & Light share input dev. suchangyu. 20100513. begin. */
extern struct input_dev *sensor_dev;
/* G-sensor & Light share input dev. suchangyu. 20100513. end. */

/* following are the sysfs callback functions */
static ssize_t po188_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
     return sprintf(buf, "%d\n", po188_driver.voltage_now);
 }

/* Define the device attributes */
static struct kobj_attribute po188_attribute =
     __ATTR(light, 0644, po188_attr_show, NULL);

static struct attribute* po188_attributes[] =
{
     &po188_attribute.attr,
     NULL
};

static struct attribute_group po188_defattr_group =
{
     .attrs = po188_attributes,
};


static int 
po188_open(struct inode* inode, struct file* file)
{
	
	return 0;
}


static int 
po188_release(struct inode* i, struct file* f)
{
	return 0;
}

static inline int
po188_get_fsr(s32 fsr)
{
    /* begin: added by z00168965 for light sensor only temporarily */
    return fsr;
	/* end: added by z00168965 for light sensor only temporarily */
	
	// modify the value	
	if(fsr <=3)
		return 75;
	else if( fsr<=4 )
		return 90;
	else if( fsr<=5 )
		return 100;
	else if( fsr<=6 )
		return 110;
	else if( fsr<=7 )
		return 120;
	else if( fsr<=8 )
		return 130;
	else if( fsr<=9 )
		return 140;
	else if( fsr<=10 )
		return 150;
	else if( fsr<=11 )
		return 160;
	else if( fsr<=12 )
		return 170;	
	else if(fsr <= 13)
		return 180;
	else if(fsr <= 14)
		return 190;
	else if(fsr <= 15)
		return 200;
	else if(fsr <= 16)
		return 210;
	else 
		return 225;
}

static inline int
po188_get_pro_level(s32 fsr)
{
	int max_voltage = 1300;
	int app_range = 255;
	int value;

	value = (fsr * app_range) / max_voltage;
	if(value <= 59)
		value = 40;
	else if(value <= 253)
		value = 130;
	else
		value = 210;
    return value;
}

 static inline int
po188_get_level(s32 light)
{
	if(light <=80)
		return 1;
	else if( light <=100 )
		return 2;
	else if( light <=120 )
		return 3;
	else if( light <=140)
		return 4;
	else if( light <=160 )
		return 5;
	else if(light <=180)
		return 6;
	else if( light <=200 )
		return 7;
	else if( light <=220 )
		return 8;
	else if( light <=245)
		return 9 ;
}

/* begin: added by z00168965 for light sensor */
static int po188_read_adc(int channel, int *mv_reading)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
					__func__, channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv(h, &conv_complete_evt);
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = wait_for_completion_interruptible(&conv_complete_evt);
	if (ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret)
		pr_err("%s: couldnt close channel %d ret=%d\n",
					__func__, channel, ret);
	if (mv_reading)
		*mv_reading = (int)adc_chan_result.measurement;

	return adc_chan_result.physical;
out:
	*mv_reading = 0;
	return -EINVAL;
}
/* end: added by z00168965 for light sensor */

static void
po188_get_sensor_status(void)
{
	
	/* begin: added by z00168965 for light sensor */    
    po188_driver.voltage_now = po188_read_adc(CHANNEL_ADC_LIGHT, NULL);
    return ;
    /* end: added by z00168965 for light sensor */ 
}
					
static void
po188_wait_for_event(struct work_struct *work)
{     
	static int voltage[TIMES];
	static int i=0;
	static int j=0;

	//static int status=0;
	//static bool autodect=0;	
	int k;
	int sum=0;
    mod_timer(&po188_driver.timer, jiffies + SENSOR_POLLING_JIFFIES);
    po188_get_sensor_status();
    /*po188_driver.voltage_now = po188_get_fsr(po188_driver.voltage_now);*/
    po188_driver.voltage_now = po188_get_pro_level(po188_driver.voltage_now);
	voltage[i++%TIMES]=po188_driver.voltage_now;
	if(j<TIMES)
	{
		j++;
	}
	if( j<TIMES )
	{
		for(k=0;k<j;k++)	
		{
			sum += voltage[k];
		}
		po188_driver.current_voltage = sum / k;
	}
	else
	{
		for(k=0;k<TIMES;k++)	
		{
			sum += voltage[k];
		}
		po188_driver.current_voltage = sum / TIMES;
	}

	if( (po188_driver.last_voltage - po188_driver.current_voltage >= 60) ||
        (po188_driver.current_voltage - po188_driver.last_voltage >= 60) )
	{
	
	    input_report_abs(po188_driver.input_dev, ABS_MISC,  po188_driver.voltage_now);			
		input_sync(po188_driver.input_dev);

		/*status++;
		if(status == TIMES)
		{
			autodect = 0;
			status = 0;
		}
		else
		{
			autodect = 1;
		}*/
        po188_driver.last_voltage = po188_driver.voltage_now;
        PO188_DMSG(" *************inner we can report light info to HAL (%d)\n", po188_driver.voltage_now);
	}
	if( po188_driver.vol_flag == true )
	{
		//input_report_rel(po188_driver.input_dev, REL_LIGHT,  10);			
		//input_sync(po188_driver.input_dev);
        input_report_abs(po188_driver.input_dev, ABS_MISC,  po188_driver.voltage_now);			
		input_sync(po188_driver.input_dev);

        /* begin: z00168965 for debug temporarily */
        //po188_driver.vol_flag = false; 
		/* end: z00168965 for debug temporarily */
        
        po188_driver.last_voltage = po188_driver.voltage_now;
	}
	//po188_driver.last_voltage = po188_driver.current_voltage;
	PO188_DMSG(" *************outer we can report light info to HAL (%d)\n", po188_driver.voltage_now);		
}


static void 
po188_start_cb_thread(unsigned long data)
{

	
	atomic_set(&po188_driver.cb_thread_wakeup,0);

	queue_work(po188_driver.po188_wq, &po188_cb_work);
	
}


/* Begin: leyihua modified for Linux Kernel 3.0, begin 2011/11/26 */
static long po188_ioctl(struct file* file, unsigned int cmd, 
	unsigned long param)
/* End: leyihua modified for Linux Kernel 3.0, end 2011/11/26 */
{
	//PO188_DMSG("!!!!!!!!!!!!!!!!!!!!!!!!%s Entry ...\n",__FUNCTION__);
	int ret = 0;
	//int mv;
    static int start_flag = 0;
    
	switch (cmd)
	{	
		
		case IOCTL_CLOSE_PO188:
			destroy_workqueue(po188_driver.po188_wq);
			break;

		/* begin: added by z00168965 for light sensor HAL */
        case LIGHTSENSOR_IOCTL_ENABLE:
        /* end: added by z00168965 for light sensor HAL */    
		case SENSORS_GET_LUX_FIR:
			if(1 == param)
			{
				PO188_DMSG(" ************* active enable,	close_finished_flag =  1\n");

                po188_driver.status_on = true;
				po188_driver.vol_flag = true;
                if (start_flag)
                {
                    mod_timer(&po188_driver.timer, jiffies + SENSOR_POLLING_JIFFIES);
                }
                else
                {
                    mod_timer(&po188_driver.timer, jiffies + START_SENSOR_POLLING_JIFFIES);
                }
			}
			if( 0==param )
			{	
				PO188_DMSG(" ************* active disable close_finished_flag =  0\n");
				del_timer(&po188_driver.timer);		
				po188_driver.status_on = false;
                po188_driver.vol_flag = false;
			}
			ret = 1;
			break;

		case IOCTL_RESET_PO188:
			if (down_trylock(&po188_driver.run_sem))
			{
				ret =  -EBUSY;
			}
			up(&po188_driver.run_sem);
			break;

		case IOCTL_PO188_SLEEP_TIME:
			if (down_trylock(&po188_driver.run_sem))
			{
				ret = -EBUSY;
			}

			if (param == 0)
			{
				up(&po188_driver.run_sem);
				ret = -EINVAL;
			}	
			po188_driver.sleep_time = param;
			printk("set count=%d\n", po188_driver.sleep_time);

			up(&po188_driver.run_sem);
			break;

        /* begin: added by z00168965 for light sensor HAL */
        case LIGHTSENSOR_IOCTL_GET_ENABLED:
            put_user(1, (unsigned long __user *)param);
            break;
		/* end: added by z00168965 for light sensor HAL */
            
		default:
			printk("%s CMD INVALID.\n", __FUNCTION__);
			ret = -EINVAL;
			break;
			
	}

    start_flag = 1;
    
	return ret;

}


static void Po188_suspend(struct early_suspend *h)
{
    if( po188_driver.status_on )
	{
		destroy_workqueue(po188_driver.po188_wq);
		del_timer(&po188_driver.timer);
    }
}

static void Po188_resume(struct early_suspend *h)
{
	if( po188_driver.status_on )
	{
	    po188_driver.vol_flag = true;
        mod_timer(&po188_driver.timer, jiffies + 3*SENSOR_POLLING_JIFFIES);
        po188_driver.po188_wq =   create_singlethread_workqueue("po188_wq");
    }
}
  
static int __init
po188_init(void)
{
	int err;
	int ret;
	struct kobject *kobj = NULL;
	po188_driver.dev.name = PO188_DEV_NAME;
	po188_driver.dev.minor = MISC_DYNAMIC_MINOR;
	po188_driver.fops.open = po188_open;
	po188_driver.fops.release = po188_release;
	/* Begin: leyihua modified for Linux Kernel 3.0, begin 2011/11/26 */
	po188_driver.fops.unlocked_ioctl = po188_ioctl;
	/* End: leyihua modified for Linux Kernel 3.0, end 2011/11/26 */
	po188_driver.dev.fops = &po188_driver.fops;
	po188_driver.last_voltage = 0;
	po188_driver.current_voltage = 0;
	po188_driver.vol_flag = false;
    
	if ((err = misc_register(&po188_driver.dev)))
	{
		PO188_ERRMSG("misc_register() failed");
		goto EXIT_ERROR;
	}

	/* Begin: leyihua modified for Linux Kernel 3.0, begin 2011/11/26 */
	sema_init(&(po188_driver.run_sem), 1);
	/* End: leyihua modified for Linux Kernel 3.0, begin 2011/11/26 */
	
	po188_driver.po188_wq =
	    create_singlethread_workqueue("po188_wq");
	
	if (!po188_driver.po188_wq) 
	{
		printk(KERN_ERR "***********************%s: create workque failed \n", __func__);
		return -ENOMEM;
	}	
	init_timer(&po188_driver.timer);
	
	po188_driver.timer.expires = jiffies + SENSOR_POLLING_JIFFIES;
	po188_driver.timer.data = 0;
	po188_driver.timer.function = po188_start_cb_thread;
	add_timer(&po188_driver.timer);

  /* G-sensor & Light share input dev. suchangyu. 20100513. begin. */
  if (sensor_dev == NULL) {
	po188_driver.input_dev = input_allocate_device();
	if (po188_driver.input_dev == NULL) {
      printk(KERN_ERR "po188_init : Failed to allocate input device\n");
		return -1;
	}
    /* Modified by suchangyu. 20100513. begin. */
    po188_driver.input_dev->name = "sensors";  // "light_sensor"
    /* Modified by suchangyu. 20100513. end. */
	input_set_drvdata(po188_driver.input_dev, &po188_driver);
	
	ret = input_register_device(po188_driver.input_dev);
	if (ret) {
		printk(KERN_ERR "[%s]Unable to register %s input device\n", __func__,po188_driver.input_dev->name);
		return -1;
	}
    sensor_dev = po188_driver.input_dev;
  } else {
    po188_driver.input_dev = sensor_dev;
  }
  /* G-sensor & Light share input dev. suchangyu. 20100513. end. */
  /* Modified by suchangyu. 20100513. begin. */
  set_bit(EV_ABS,po188_driver.input_dev->evbit);
  set_bit(ABS_MISC, po188_driver.input_dev->absbit);
  set_bit(EV_SYN,po188_driver.input_dev->evbit);

  po188_driver.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
  po188_driver.early_suspend.suspend = Po188_suspend;
  po188_driver.early_suspend.resume = Po188_resume;
  register_early_suspend(&po188_driver.early_suspend);
    
  kobj = kobject_create_and_add("po188", NULL);
  if (kobj == NULL) {
		return -1;
	}
  if (sysfs_create_group(kobj, &po188_defattr_group)) {
		kobject_put(kobj);
		return -1;
	}


	return 0;

EXIT_ERROR:
  /* Modified by suchangyu. 20100514. begin */	
  kobject_put(kobj);
  sysfs_remove_group(kobj, &po188_defattr_group);
  /* Modified by suchangyu. 20100514. end */
	return err;
}


static void __exit
po188_exit (void)
{

	sysfs_remove_group(&po188_driver.dev.parent->kobj, 
		&po188_defattr_group);
	input_unregister_device(po188_driver.input_dev);
	misc_deregister(&po188_driver.dev);
}

/* G-sensor & Light share input dev. suchangyu. 20100513. begin. */
//module_init(po188_init);
device_initcall_sync(po188_init);
/* G-sensor & Light share input dev. suchangyu. 20100513. end. */
module_exit(po188_exit);


MODULE_DESCRIPTION("po188 light sensor driver");
MODULE_AUTHOR("libao");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");

