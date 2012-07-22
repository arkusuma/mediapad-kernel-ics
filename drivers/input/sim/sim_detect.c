/* Driver for SIM CARD dector

 *  Copyright 2010 S7 Tech. Co., Ltd.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 */

#include <linux/interrupt.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <linux/input/sim_detect.h>
#include <linux/err.h>
#include <linux/slab.h>

#define SIM_ONDUTY		1
#define SIM_OFFDUTY	      0

struct sim_detect_data
{	
	u32	irq;
	u32	det_gpio;
};

static struct sim_detect_data *data = NULL;
static int g_sim_state = SIM_OFFDUTY; 
static int g_sim_pluge_times = 0; 

/* sim card initial status, only record the first status after power on */
static u16 sim_init_state = 10;
static int sim_get_init_status(char *buffer, struct kernel_param *kp);
module_param_call(sim_init_state, NULL, sim_get_init_status,
					&sim_init_state, 0644);
MODULE_PARM_DESC(sim_init_state, "sim card initial status");
static int sim_get_init_status(char *buffer, struct kernel_param *kp)
{
    int ret;

    switch( sim_init_state )
    {
        case SIM_OFFDUTY:
            ret = sprintf(buffer, "%s", "off");

            break;
        case SIM_ONDUTY:
            ret = sprintf(buffer, "%s", "on");

            break;
        default:
            ret = sprintf(buffer, "%s", "unknow");

            break;
    }

    return ret;
}

static ssize_t sim_attr_show(struct device_driver *driver, char *buf)
{
    int buf_len = 0;

    if( (g_sim_state == SIM_ONDUTY) || (g_sim_state == SIM_OFFDUTY) )
    {
        *((int *)buf) = g_sim_pluge_times;
        buf_len += sizeof(g_sim_pluge_times);
        *((bool *)(buf + buf_len)) = (SIM_ONDUTY == g_sim_state) ? true : false;
        buf_len += sizeof(bool);
        
        return buf_len;
    }
    else 
    {
        return -1;
    }
}

static void sim_detect_delay_func(struct work_struct *work);
static DECLARE_DELAYED_WORK(sim_detect_event, sim_detect_delay_func);
static void sim_detect_delay_func(struct work_struct *work)
{
    int temp_sim_state = g_sim_state;
	
    temp_sim_state = gpio_get_value(data->det_gpio);

    printk(KERN_INFO "sim card status change from %s to %s\n", 
                                g_sim_state == SIM_ONDUTY ? "on duty":"off duty",
                                temp_sim_state == SIM_ONDUTY ? "on duty":"off duty" );  
    
    if( temp_sim_state != g_sim_state )
    {
        g_sim_state = temp_sim_state;
        g_sim_pluge_times++;

        sim_init_state = g_sim_state;
   }

    return;
}


static DRIVER_ATTR(state, S_IRUGO, sim_attr_show, NULL);

static irqreturn_t sim_detect_interrupt(int irq, void *dev_id)
{
    schedule_delayed_work(&sim_detect_event, msecs_to_jiffies(200));

    return IRQ_HANDLED;
}

static int sim_detect_probe(struct platform_device *pdev)
{
    int rc; 
    struct sim_detect_platform_data *pdata = NULL;

    data = kzalloc(sizeof(struct sim_detect_data), GFP_KERNEL);
    if (!data) 
    {
        return -ENOMEM;
    }

    pdata = pdev->dev.platform_data;
    data->det_gpio = pdata->det_gpio;
    data->irq = pdata->det_irq_gpio;

    rc = request_irq(data->irq,
                    sim_detect_interrupt,
                    IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                    "sim_detect",
                    NULL);  
    if (rc < 0) 
    {       
        printk(KERN_ERR "request sim_detect_interrupt error. rc=%d\n", rc);      
        kfree(data);
        return rc;
    } 
	
    g_sim_state = gpio_get_value(data->det_gpio);
    printk(KERN_INFO "sim card %s\n", g_sim_state == SIM_ONDUTY ? "on duty":"off duty");      

    sim_init_state = g_sim_state;

    return 0;
}

static int sim_detect_remove(struct platform_device *pdev)
{
    if( NULL != data )
    {
        kfree(data);
        data = NULL;
    }
    
    return 0;
}


static struct platform_driver sim_detect_driver =
{
    .probe     = sim_detect_probe,       
    .remove    = sim_detect_remove,
    .driver    = 
    {
        .name  = "sim_detect",
        .owner = THIS_MODULE,
    },
};


static int __init sim_detect_init(void)
{
    int rc = 0;	

    rc = platform_driver_register(&sim_detect_driver); 
    if (rc < 0) 
    {
        printk(KERN_ERR "register sim detect driver error: %d\n", rc);
        return rc;
    }	 

    if ( (rc = driver_create_file(&sim_detect_driver.driver, &driver_attr_state)) )
    {
        platform_driver_unregister(&sim_detect_driver);
        printk(KERN_ERR "failed to create sysfs entry(state): %d\n", rc); 
    }	    

    return rc;
}

static void __exit sim_detect_exit(void)
{
    return platform_driver_unregister(&sim_detect_driver);

}

module_init(sim_detect_init);
module_exit(sim_detect_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Sim Card Detect Driver");
