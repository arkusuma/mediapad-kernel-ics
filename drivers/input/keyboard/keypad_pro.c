#include <linux/input.h> 
#include <linux/module.h> 
#include <linux/init.h>
#include <asm/irq.h> 
#include <asm/io.h>

#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <mach/board.h>

#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

extern int msm_gpiomux_put(unsigned gpio);
extern int msm_gpiomux_get(unsigned gpio);

static void LED_TimerHandler(unsigned long data);
static DEFINE_TIMER(gLED_Timer, LED_TimerHandler, 0, 0);

#if S7_HWID_L3L(S7, S7301, T0)   
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio + 173)
#define VOLUMEUP_PRO	PM8058_GPIO_PM_TO_SYS(8)
#define VOLUMEDOWN_PRO	PM8058_GPIO_PM_TO_SYS(9)
#else
#define VOLUMEUP_PRO	   (123)
#define VOLUMEDOWN_PRO	   (124)
#define VOLUMEUP_PRO_IRQ   MSM_GPIO_TO_INT(VOLUMEUP_PRO)
#define VOLUMEDOWN_PRO_IRQ MSM_GPIO_TO_INT(VOLUMEDOWN_PRO)
#endif

#define LED				(153)
#define ON				(1)
#define OFF				(0)

#define SCAN_TIME	(40) //ms
#ifndef ARRAYSIZE
#define ARRAYSIZE(a)		(sizeof(a)/sizeof(a[0]))
#endif

static struct platform_driver button_driver;
static int volkey_reverse = 0;

static struct input_dev *button_dev;
static struct hrtimer timer;
int vol_up, vol_down;
int vol_up_old = -1;
int vol_down_old = -1;


static char key_v[] = "mbhudlreas";
static int key[] = {KEY_MENU, KEY_BACK, KEY_HOME, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER,KEY_VOLUMEUP, KEY_VOLUMEDOWN};

static irqreturn_t volume_irq_handler(int irq, void *dev_id)
{
    disable_irq_nosync(VOLUMEUP_PRO_IRQ);
    disable_irq_nosync(VOLUMEDOWN_PRO_IRQ);
    hrtimer_start(&timer, ktime_set(0, SCAN_TIME * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    return IRQ_HANDLED;
}

static enum hrtimer_restart keypad_timer_func(struct hrtimer *hrtimer)
{
    
    if (volkey_reverse == 1) {
    	vol_down = gpio_get_value_cansleep(VOLUMEUP_PRO);
		vol_up = gpio_get_value_cansleep(VOLUMEDOWN_PRO);
    } else {
    	vol_up = gpio_get_value_cansleep(VOLUMEUP_PRO);
		vol_down = gpio_get_value_cansleep(VOLUMEDOWN_PRO);
    }

	if (vol_up < 0)
        vol_up = vol_up_old;
    if (vol_down < 0)
        vol_down = vol_down_old;

	if(vol_up_old != vol_up) {
	    input_report_key(button_dev, KEY_VOLUMEUP, !vol_up);       
        input_sync(button_dev);
    }

    if(vol_down_old != vol_down) {
 	   input_report_key(button_dev, KEY_VOLUMEDOWN, !vol_down);  
       input_sync(button_dev);
    }
    
    vol_up_old = vol_up;
    vol_down_old = vol_down; 

    if ((vol_up == 0) || (vol_down == 0)) {
	    hrtimer_start(&timer, ktime_set(0, SCAN_TIME * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    } else {
		enable_irq(VOLUMEUP_PRO_IRQ);
		enable_irq(VOLUMEDOWN_PRO_IRQ);
    }
    
	return HRTIMER_NORESTART;
}

static ssize_t key_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
    int key_up, key_down;

    key_up = gpio_get_value_cansleep(VOLUMEUP_PRO);
	key_down = gpio_get_value_cansleep(VOLUMEDOWN_PRO);

	return sprintf(buf, "key_up = %d\n key_down = %d\n",key_up, key_down);
}

static ssize_t key_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int num;
    int i;
	for (i = 0; i < ARRAYSIZE(key); i++) {
		if (key_v[i] == buf[0]) {
			num = i;
            break;
        }
    }
    
    input_report_key(button_dev, key[i], 1);
    printk(KERN_ERR "%c pressed\n", key_v[i]);
	input_sync(button_dev);
    mdelay(150);      
    input_report_key(button_dev, key[i], 0);  
    printk(KERN_ERR "%c released\n", key_v[i]);
	input_sync(button_dev);
    
	return count;
}
static DEVICE_ATTR(key, 0644, key_show, key_store);

static unsigned int gulTime = 0, m_status = 0;
#define BSP_MAX_HZ    100   /*大于等于100HZ则认为是常亮*/
static void Led_Ctrl_func(struct work_struct *work)
{    
    if (m_status)
    {
        gpio_set_value_cansleep(LED, 1);
        mod_timer(&gLED_Timer, jiffies + msecs_to_jiffies(gulTime));
    }
    else
    {
    	gpio_set_value_cansleep(LED, 0);
        mod_timer(&gLED_Timer, jiffies + msecs_to_jiffies(gulTime));
    }

    m_status = ~m_status;    
}

static DECLARE_WORK(led_event, Led_Ctrl_func);
static void LED_TimerHandler(unsigned long data)
{
    schedule_work(&led_event);    
    return;
}

static ssize_t led_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
    int on = 0;  /*led flash HZ number*/
    on = simple_strtoul(buf, NULL, 0);

    if(0 == on)
    {
        gpio_set_value_cansleep(LED, 0);
        del_timer(&gLED_Timer);
    }
    else if (on >= BSP_MAX_HZ)
    {
        gpio_set_value_cansleep(LED, 1);
        del_timer(&gLED_Timer);
    }
    else
    {
        gulTime = 1000/2/on;   /* ms */
        mod_timer(&gLED_Timer, jiffies + msecs_to_jiffies(gulTime));
    }
   
	return count;
}
static DEVICE_ATTR(led, 0600, NULL, led_store);

static ssize_t volkey_reverse_show(struct device_driver *drv, char *buf)
{
	return sprintf(buf, "volkey_reverse = %d\n",volkey_reverse);
}

static ssize_t volkey_reverse_store(struct device_driver *drv, const char *buf, size_t count)
{
	int status = 0;
    if (buf == NULL) {
		return count;
    }
	status = simple_strtoul(buf, NULL, 0);
	if ((status != 0) && (status != 1)) {
		printk(KERN_ERR "[%s %d] Must be 1 or 0\n", __func__, __LINE__);
    } else {
		volkey_reverse = status;
    }    
	return count;
}

static DRIVER_ATTR(volkey_reverse, 0644, volkey_reverse_show, volkey_reverse_store);

static int __devinit button_probe(struct platform_device *pdev)
{
    int error;
	int rc;
    int i;
    
#if S7_HWID_L3L(S7, S7301, T0)   
    struct pm8058_gpio kypd_sns = {
        .direction  = PM_GPIO_DIR_IN,
        .pull       = PM_GPIO_PULL_UP_31P5,
        .vin_sel    = 2,
        .out_strength   = PM_GPIO_STRENGTH_NO,
        .function   = PM_GPIO_FUNC_NORMAL,
        .inv_int_pol    = 1,
    };

    rc = pm8058_gpio_config(8, &kypd_sns);
    if (rc) {
        pr_err("%s: FAIL pm8058_gpio_config(): rc=%d.\n",
            __func__, rc);
        return rc;
    }

	rc = pm8058_gpio_config(9, &kypd_sns);
	if (rc) {
	    pr_err("%s: FAIL pm8058_gpio_config(): rc=%d.\n",
	        __func__, rc);
	    return rc;
	}
#endif

	error = gpio_request(VOLUMEUP_PRO, "VOLUMEUP_PRO");
	if (error) {
		pr_err("%s: gpio_request failed for VOLUMEUP_PRO\n",
								__func__);
		return error;
	}
    
	error = gpio_request(VOLUMEDOWN_PRO, "VOLUMEDOWN_PRO");
	if (error) {
		pr_err("%s: gpio_request failed for VOLUMEDOWN_PRO\n",
								__func__);
		return error;
	}
    
#if S7_HWID_L3H(S7, S7301, B)  
	rc = gpio_direction_input(VOLUMEUP_PRO);
	if (rc) {
	    pr_err("%s: unable to set direction for VOLUMEUP_PRO\n", __func__);
	    goto error_volumeup;
	}
	rc = gpio_direction_input(VOLUMEDOWN_PRO);
	if (rc) {
	    pr_err("%s: unable to set direction for VOLUMEDOWN_PRO\n", __func__);
	    goto error_volumedown;
	}
#endif
   
    error = gpio_request(LED, "led");
    if (error) {
        pr_err("%s: gpio_request failed for led\n",
                                __func__);
        return error;
    }

	rc = gpio_direction_output(LED, ON);
	if (rc) {
	    pr_err("%s: unable to set direction for LED\n", __func__);
	}

	button_dev = input_allocate_device();
    
	if (!button_dev) { 
        printk(KERN_ERR "button.c: Not enough memory\n");
		error = -ENOMEM;
        
		goto error_volumedown; 
    }

	button_dev->name = "pro_keypad";
    
    set_bit(EV_KEY, button_dev->evbit);    
	
	for (i = 0; i < ARRAYSIZE(key); i++) {
		set_bit(key[i],button_dev->keybit );
    }    
    
	error = input_register_device(button_dev);
	if (error) {
        printk(KERN_ERR "keypad_pro.c: Failed to register device\n");
		goto err_free_dev; 
    }

    error = device_create_file(&button_dev->dev, &dev_attr_key);
	if (error) {
        printk(KERN_ERR "keypad_pro.c: Failed to device_create_file\n");
		goto err_free_dev; 
    }

    error = device_create_file(&button_dev->dev, &dev_attr_led);
	if (error) {
        printk(KERN_ERR "keypad_pro.c: Failed to device_create_file\n");
		goto err_free_dev; 
    }

    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);   
    timer.function = keypad_timer_func;
    
    error = request_irq(VOLUMEUP_PRO_IRQ, volume_irq_handler, IRQF_TRIGGER_FALLING, "volume_up", &pdev->dev);
    if (error) {
        printk(KERN_ERR "[%s] request irq %d failed", __func__, VOLUMEUP_PRO);
        goto err_free_irq;
    }
    
    error = request_irq(VOLUMEDOWN_PRO_IRQ, volume_irq_handler, IRQF_TRIGGER_FALLING, "volume_down", &pdev->dev);
    if (error) {
        printk(KERN_ERR "[%s] request irq  %d failed", __func__, VOLUMEDOWN_PRO);
        goto err_free_irq1;
    }
    
   
    error = driver_create_file(&button_driver.driver, &driver_attr_volkey_reverse);
    if (error < 0) {
		printk(KERN_ERR "[%s %d] create volkey reverse sys failed", __func__, __LINE__);
    }

	return 0;
    
 err_free_irq1:
    free_irq(VOLUMEDOWN_PRO_IRQ, &pdev->dev);
 err_free_irq:   
    free_irq(VOLUMEUP_PRO_IRQ, &pdev->dev);
 err_free_dev:
    input_free_device(button_dev);
 error_volumedown:    
    gpio_free(VOLUMEDOWN_PRO);
 error_volumeup:      
    gpio_free(VOLUMEUP_PRO);
    
    return error; 
}

#ifdef CONFIG_PM
static int button_suspend(struct device *dev)
{
    msm_gpiomux_put(VOLUMEUP_PRO);
    msm_gpiomux_put(VOLUMEDOWN_PRO);    
    msm_gpiomux_put(LED);
	return 0;
}

static int button_resume(struct device *dev)
{
	int ret = -1;
    ret = msm_gpiomux_get(VOLUMEUP_PRO);
    if (ret < 0) {
        printk(KERN_ERR "get VOLUMEUP_PRO failed\n");
        return ret;
	}
    ret = msm_gpiomux_get(VOLUMEDOWN_PRO);    
    if (ret < 0) {
        printk(KERN_ERR "get VOLUMEDOWN_PRO failed\n");
        return ret;
	}
    ret = msm_gpiomux_get(LED);
    if (ret < 0) {
        printk(KERN_ERR "get LED failed\n");
        return ret;
	}
    
	return 0;
}

static struct dev_pm_ops button_pm_ops = {
	.suspend = button_suspend,
	.resume = button_resume,
};
#endif

static struct platform_driver button_driver = {
	.probe		= button_probe,
	.driver	= {
		.name	= "button-pro",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &button_pm_ops,
#endif
	},
};


static int __init button_init(void) 
{
	return platform_driver_register(&button_driver);
}

static void __exit button_exit(void) 
{ 
    input_unregister_device(button_dev);
    hrtimer_cancel(&timer);
    platform_driver_unregister(&button_driver);
}

module_init(button_init);
module_exit(button_exit);

