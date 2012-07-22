/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-othc.h>
#include <linux/msm_adc.h>	

#define PM8058_OTHC_LOW_CURR_MASK	0xF0
#define PM8058_OTHC_HIGH_CURR_MASK	0x0F
#define PM8058_OTHC_EN_SIG_MASK		0x3F
#define PM8058_OTHC_HYST_PREDIV_MASK	0xC7
#define PM8058_OTHC_CLK_PREDIV_MASK	0xF8
#define PM8058_OTHC_HYST_CLK_MASK	0x0F
#define PM8058_OTHC_PERIOD_CLK_MASK	0xF0

#define PM8058_OTHC_LOW_CURR_SHIFT	0x4
#define PM8058_OTHC_EN_SIG_SHIFT	0x6
#define PM8058_OTHC_HYST_PREDIV_SHIFT	0x3
#define PM8058_OTHC_HYST_CLK_SHIFT	0x4

#define OTHC_GPIO_MAX_LEN		25

enum othc_headset_status {
	OTHC_HEADSET_REMOVE,
	OTHC_HEADSET_INSERT,
};

enum othc_headset_switch_status {
	OTHC_HEADSET_SWITCH_ON,
	OTHC_HEADSET_SWITCH_OFF,
};

struct pm8058_othc {
	bool othc_sw_state;
	bool switch_reject;
	bool othc_support_n_switch;
	bool accessory_support;
	int othc_base;
	int othc_irq_sw;
	int othc_irq_ir;
	int othc_irq_det;			
	int othc_ir_state;
	int num_accessories;
	int curr_accessory_code;
	int curr_accessory;
    int mic_ctrl_pin;
    int headset_detect_pin;
	u32 sw_key_code;
	unsigned long switch_debounce_ms;
	unsigned long detection_delay_ms;
	void *adc_handle;
	spinlock_t lock;
	struct device *dev;
	struct regulator *othc_vreg;
	struct input_dev *othc_ipd;
	struct switch_dev othc_sdev;
	struct pmic8058_othc_config_pdata *othc_pdata;
	struct othc_accessory_info *accessory_info;
	struct hrtimer timer;
	struct othc_n_switch_config *switch_config;
	struct work_struct switch_work;
	struct delayed_work detect_work;

	enum othc_headset_status  headset_status;
	enum othc_headset_switch_status  switch_status;
	
};

static struct pm8058_othc *config[OTHC_MICBIAS_MAX];
static int is_init_pmic8058 = 0 ;

/*
 * The API pm8058_micbias_enable() allows to configure
 * the MIC_BIAS. Only the lines which are not used for
 * headset detection can be configured using this API.
 * The API returns an error code if it fails to configure
 * the specified MIC_BIAS line, else it returns 0.
 */
int pm8058_micbias_enable(enum othc_micbias micbias,
		enum othc_micbias_enable enable)
{
	int rc;
	u8 reg;
	struct pm8058_othc *dd = config[micbias];

	if (dd == NULL) {
		pr_err("MIC_BIAS not registered, cannot enable\n");
		return -ENODEV;
	}

	if (dd->othc_pdata->micbias_capability != OTHC_MICBIAS) {
		pr_err("MIC_BIAS enable capability not supported\n");
		return -EINVAL;
	}

	rc = pm8xxx_readb(dd->dev->parent, dd->othc_base + 1, &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	reg &= PM8058_OTHC_EN_SIG_MASK;
	reg |= (enable << PM8058_OTHC_EN_SIG_SHIFT);

	rc = pm8xxx_writeb(dd->dev->parent, dd->othc_base + 1, reg);
	if (rc < 0) {
		pr_err("PM8058 write failed\n");
		return rc;
	}

	return rc;
}
EXPORT_SYMBOL(pm8058_micbias_enable);

#ifdef CONFIG_PM
static int pm8058_othc_suspend(struct device *dev)
{
	struct pm8058_othc *dd = dev_get_drvdata(dev);
    if (dd->headset_detect_pin != 0){
        gpio_free(dd->headset_detect_pin);
        gpio_free(dd->mic_ctrl_pin);
    }

	if (dd->othc_pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		if (device_may_wakeup(dev)) {
			enable_irq_wake(dd->othc_irq_sw);
//			enable_irq_wake(dd->othc_irq_ir);
			enable_irq_wake(dd->othc_irq_det);
		}
	}

	return 0;
}

static int pm8058_othc_resume(struct device *dev)
{
	struct pm8058_othc *dd = dev_get_drvdata(dev);

	if (dd->othc_pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		if (device_may_wakeup(dev)) {
			disable_irq_wake(dd->othc_irq_sw);
			disable_irq_wake(dd->othc_irq_det);
		}
	}

    if ( dd->headset_detect_pin != 0 ){
        gpio_request(dd->headset_detect_pin, "headset-detect");
    	gpio_request(dd->mic_ctrl_pin, "headset-ctrl");
        if (dd->switch_status == OTHC_HEADSET_SWITCH_ON){
            gpio_direction_output(dd->mic_ctrl_pin, 0);
        }
    }

	return 0;
}

static struct dev_pm_ops pm8058_othc_pm_ops = {
	.suspend = pm8058_othc_suspend,
	.resume = pm8058_othc_resume,
};
#endif

static int __devexit pm8058_othc_remove(struct platform_device *pd)
{
	struct pm8058_othc *dd = platform_get_drvdata(pd);

	pm_runtime_set_suspended(&pd->dev);
	pm_runtime_disable(&pd->dev);

	if (dd->othc_pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		device_init_wakeup(&pd->dev, 0);
		if (dd->othc_support_n_switch == true) {
			adc_channel_close(dd->adc_handle);
			cancel_work_sync(&dd->switch_work);
		}

		if (dd->accessory_support == true) {
			int i;
			for (i = 0; i < dd->num_accessories; i++) {
				if (dd->accessory_info[i].detect_flags &
							OTHC_GPIO_DETECT)
					gpio_free(dd->accessory_info[i].gpio);
			}
		}
		cancel_delayed_work_sync(&dd->detect_work);
		free_irq(dd->othc_irq_sw, dd);
		input_unregister_device(dd->othc_ipd);
	}
	regulator_disable(dd->othc_vreg);
	regulator_put(dd->othc_vreg);
	kfree(dd);

	return 0;
}


/* begin: wuxinxian 00176579 add for headset */
static int switch_read_adc(int channel, int *mv_reading)
{
	int rc;
	void* h;
	struct adc_chan_result adc_result;
	DECLARE_COMPLETION_ONSTACK(adc_wait);
	
	rc = adc_channel_open(channel, &h);
	if (rc) {
		pr_err("Unable to open ADC channel\n");
		return -ENODEV;
	}
	
	rc = adc_channel_request_conv(h, &adc_wait);
	if (rc) {
		pr_err("adc_channel_request_conv failed\n");
		goto bail_out;
	}

	rc = wait_for_completion_interruptible(&adc_wait);
	if (rc) {
		pr_err("wait_for_completion_interruptible failed\n");
		goto bail_out;
	}

	rc = adc_channel_read_result(h, &adc_result);
	if (rc) {
		pr_err("adc_channel_read_result failed\n");
		goto bail_out;
	}

	if (mv_reading)
		*mv_reading = adc_result.measurement;
	return adc_result.physical;
bail_out:
	return -EINVAL;

}
/* end: wuxinxian 00176579 add for headset */

#define HEADSET_DETECT_GPIO 143
#define MIC_CTRL_GPIO 134
//#define CHECK_SWITCH_TIMES 3	
#define UPPER_SWITCH_VOLTAGE 1100
#define LOWER_SWITCH_VOLTAGE 100

/*===========================================================================
  Function:       get_switch_voltage
  Description:    get the switch voltage in mv by adc
  Calls:         
  Called By:      check_headset_status
  Input:          none
  Output:         none
  Return:         voltage in mv
  Others:         none
===========================================================================*/
static int get_switch_voltage(void)
{
	int value;
	switch_read_adc(CHANNEL_ADC_HDSET,  &value);
	return value;
}

#define KEEP_STATUS_TIMES 5
#define CHECK_VALUE_TIMES ((1 << KEEP_STATUS_TIMES) -1)
/*===========================================================================
  Function:       get_det_status
  Description:    return the detect pin status if 5 times of the gpio status keep in the same at interval 10ms
  Calls:         
  Called By:      check_headset_status
  Input:          none
  Output:         none
  Return:         detect pin status
  Others:         none
===========================================================================*/
static int get_det_status(void)
{
	unsigned int temp;
	unsigned int value = 0;
	unsigned int times = 0;

	while(1)
	{
		value = value << 1;
		value |= (!!gpio_get_value(HEADSET_DETECT_GPIO));
		printk(KERN_INFO "value=%d \n", value);		
		if(++times > KEEP_STATUS_TIMES) {
			temp = value & CHECK_VALUE_TIMES;

			printk(KERN_INFO "temp=%d, check=%d\n", temp, CHECK_VALUE_TIMES);
			
			if((temp == 0x00) || (temp == CHECK_VALUE_TIMES))
			{
				return !!temp;
			}
		}
		msleep(10);
	}
}

/*===========================================================================
  Function:       check_headset_status
  Description:   update headset status according to the detect pin status
  Calls:         
  Called By:      detect_work_f
  Input:          pointer of pm8058_othc
  Output:         none
  Return:         none
  Others:         none
===========================================================================*/
static void check_headset_status(struct pm8058_othc *dd)
{
	int gpio_status;
	int avg;
	int switch_ = 0;

	gpio_status = get_det_status();		//gpio_get_value(HEADSET_DETECT_GPIO);

	printk(KERN_INFO "gpio_143_status = %d \n", gpio_status);

	if(gpio_status) {

		/* get switch voltage */
		avg = get_switch_voltage();

		printk(KERN_INFO "average voltage=%d \n", avg);

		if (avg < UPPER_SWITCH_VOLTAGE)  {
//		if((avg > LOWER_SWITCH_VOLTAGE) && (avg < UPPER_SWITCH_VOLTAGE))  {
			switch_ = 1;
		}

		if(dd->headset_status == OTHC_HEADSET_REMOVE) {
			// insert
			dd->headset_status = OTHC_HEADSET_INSERT;
			printk(KERN_INFO "headset insert... \n");
			if(switch_ && (dd->switch_status == OTHC_HEADSET_SWITCH_OFF)) {
				gpio_direction_output(MIC_CTRL_GPIO, 0);
				dd->switch_status = OTHC_HEADSET_SWITCH_ON;
				printk(KERN_INFO "switch status...... \n");
			}

			switch_set_state(&dd->othc_sdev, OTHC_HEADSET);
			input_report_switch(dd->othc_ipd, SW_HEADPHONE_INSERT, 1);
			input_sync(dd->othc_ipd);

       if(0 ==  is_init_pmic8058)
            msleep(600);
			/* enable irq of key press */
			enable_irq(dd->othc_irq_sw);
			
		} else {
		//check status
			if((!switch_) && (dd->switch_status == OTHC_HEADSET_SWITCH_ON)) {
				dd->switch_status = OTHC_HEADSET_SWITCH_OFF;
				gpio_direction_output(MIC_CTRL_GPIO, 0);
				printk(KERN_INFO "headset update... \n");
			}
		}
	} else {
		if(dd->headset_status == OTHC_HEADSET_INSERT)
		{
		// remove
		    input_report_key(dd->othc_ipd, KEY_MEDIA, 0);
            input_sync(dd->othc_ipd);
			/* disable irq of key press */
			disable_irq_nosync(dd->othc_irq_sw);
			gpio_direction_output(MIC_CTRL_GPIO, 1);
			dd->headset_status = OTHC_HEADSET_REMOVE;
			dd->switch_status = OTHC_HEADSET_SWITCH_OFF;
			printk(KERN_INFO "headset remove... \n");

			switch_set_state(&dd->othc_sdev, 0);
			input_report_switch(dd->othc_ipd, SW_HEADPHONE_INSERT, 0);
			input_sync(dd->othc_ipd);

			/* disable irq of key press */
//			disable_irq_nosync(dd->othc_irq_sw);
		} else {
		//check status
			printk(KERN_INFO "error status... \n");
		}
	}
}

static void detect_work_f(struct work_struct *work)
{
	struct pm8058_othc *dd =
		container_of(work, struct pm8058_othc, detect_work.work);

	check_headset_status(dd);

	enable_irq(dd->othc_irq_det);

}

static irqreturn_t headset_det_irq(int irq, void *dev_id)
{
	struct pm8058_othc *dd = dev_id;


	/* disable irq, this gets enabled in the workqueue */
	disable_irq_nosync(dd->othc_irq_det);

	schedule_delayed_work(&dd->detect_work,
				msecs_to_jiffies(dd->detection_delay_ms));

	return IRQ_HANDLED;
}

/*
 * The pm8058_no_sw detects the switch press and release operation.
 * The odd number call is press and even number call is release.
 * The current state of the button is maintained in othc_sw_state variable.
 * This isr gets called only for NO type headsets.
 */
static irqreturn_t pm8058_no_sw(int irq, void *dev_id)
{
	int level;
	struct pm8058_othc *dd = dev_id;

	level = pm8xxx_read_irq_stat(dd->dev->parent, dd->othc_irq_sw);
	if (level < 0) {
		pr_err("Unable to read IRQ status register\n");
		return IRQ_HANDLED;
	}

	/*
	 * It is necessary to check the software state and the hardware state
	 * to make sure that the residual interrupt after the debounce time does
	 * not disturb the software state machine.
	 */
	if (level == 1 && dd->othc_sw_state == false) {
		/*  Switch has been pressed */

		printk(KERN_INFO "Switch has been pressed...");
		
		dd->othc_sw_state = true;
		input_report_key(dd->othc_ipd, KEY_MEDIA, 1);
	} else if (level == 0 && dd->othc_sw_state == true) {
		/* Switch has been released */

		printk(KERN_INFO "Switch has been released...");
		
		dd->othc_sw_state = false;
		input_report_key(dd->othc_ipd, KEY_MEDIA, 0);
	}
	input_sync(dd->othc_ipd);

	return IRQ_HANDLED;
}

static int pm8058_configure_micbias(struct pm8058_othc *dd)
{
	int rc;
	u8 reg, value;
	u32 value1;
	u16 base_addr = dd->othc_base;
	struct hsed_bias_config *hsed_config =
			dd->othc_pdata->hsed_config->hsed_bias_config;

	/* Intialize the OTHC module */
	/* Control Register 1*/
	rc = pm8xxx_readb(dd->dev->parent, base_addr, &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	/* set iDAC high current threshold */
	value = (hsed_config->othc_highcurr_thresh_uA / 100) - 2;
	reg =  (reg & PM8058_OTHC_HIGH_CURR_MASK) | value;

	rc = pm8xxx_writeb(dd->dev->parent, base_addr, reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	/* Control register 2*/
	rc = pm8xxx_readb(dd->dev->parent, base_addr + 1, &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	value = dd->othc_pdata->micbias_enable;
	reg &= PM8058_OTHC_EN_SIG_MASK;
	reg |= (value << PM8058_OTHC_EN_SIG_SHIFT);

	value = 0;
	value1 = (hsed_config->othc_hyst_prediv_us << 10) / USEC_PER_SEC;
	while (value1 != 0) {
		value1 = value1 >> 1;
		value++;
	}
	if (value > 7) {
		pr_err("Invalid input argument - othc_hyst_prediv_us\n");
		return -EINVAL;
	}
	reg &= PM8058_OTHC_HYST_PREDIV_MASK;
	reg |= (value << PM8058_OTHC_HYST_PREDIV_SHIFT);

	value = 0;
	value1 = (hsed_config->othc_period_clkdiv_us << 10) / USEC_PER_SEC;
	while (value1 != 1) {
		value1 = value1 >> 1;
		value++;
	}
	if (value > 8) {
		pr_err("Invalid input argument - othc_period_clkdiv_us\n");
		return -EINVAL;
	}
	reg = (reg &  PM8058_OTHC_CLK_PREDIV_MASK) | (value - 1);

	rc = pm8xxx_writeb(dd->dev->parent, base_addr + 1, reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	/* Control register 3 */
	rc = pm8xxx_readb(dd->dev->parent, base_addr + 2 , &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	value = hsed_config->othc_hyst_clk_us /
					hsed_config->othc_hyst_prediv_us;
	if (value > 15) {
		pr_err("Invalid input argument - othc_hyst_prediv_us\n");
		return -EINVAL;
	}
	reg &= PM8058_OTHC_HYST_CLK_MASK;
	reg |= value << PM8058_OTHC_HYST_CLK_SHIFT;

	value = hsed_config->othc_period_clk_us /
					hsed_config->othc_period_clkdiv_us;
	if (value > 15) {
		pr_err("Invalid input argument - othc_hyst_prediv_us\n");
		return -EINVAL;
	}
	reg = (reg & PM8058_OTHC_PERIOD_CLK_MASK) | value;

	rc = pm8xxx_writeb(dd->dev->parent, base_addr + 2, reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	return 0;
}

static ssize_t othc_headset_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case OTHC_NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case OTHC_HEADSET:
	case OTHC_HEADPHONE:
	case OTHC_MICROPHONE:
	case OTHC_ANC_HEADSET:
	case OTHC_ANC_HEADPHONE:
	case OTHC_ANC_MICROPHONE:
		return sprintf(buf, "Headset\n");
	}
	return -EINVAL;
}

static int
othc_configure_hsed(struct pm8058_othc *dd, struct platform_device *pd)
{
	int rc;
	struct input_dev *ipd;
	struct pmic8058_othc_config_pdata *pdata = pd->dev.platform_data;
	struct othc_hsed_config *hsed_config = pdata->hsed_config;

	dd->othc_sdev.name = "h2w";
	dd->othc_sdev.print_name = othc_headset_print_name;

    dd->headset_detect_pin = HEADSET_DETECT_GPIO;
    dd->mic_ctrl_pin = MIC_CTRL_GPIO;

	rc = gpio_request(dd->headset_detect_pin, "headset-detect");
	if (rc) {
		pr_err("%s: gpio_request failed for HEADSET_DETECT_GPIO\n",
								__func__);
	}

	rc = gpio_request(dd->mic_ctrl_pin, "headset-ctrl");
	if (rc) {
		pr_err("%s: gpio_request failed for MIC_CTRL_GPIO\n",
							__func__);
	}
	gpio_direction_output(dd->mic_ctrl_pin, 1);
	gpio_direction_input(dd->headset_detect_pin);

	rc = switch_dev_register(&dd->othc_sdev);
	if (rc) {
		pr_err("Unable to register switch device\n");
		return rc;
	}

	ipd = input_allocate_device();
	if (ipd == NULL) {
		pr_err("Unable to allocate memory\n");
		rc = -ENOMEM;
		goto fail_input_alloc;
	}

	/* Get the IRQ for Headset Insert-remove and Switch-press */
	dd->othc_irq_sw = platform_get_irq(pd, 0);
	dd->othc_irq_ir = platform_get_irq(pd, 1);
	dd->othc_irq_det = platform_get_irq(pd, 2);	//wxx_debug
	if (dd->othc_irq_ir < 0 || dd->othc_irq_sw < 0) {
		pr_err("othc resource:IRQs absent\n");
		rc = -ENXIO;
		goto fail_micbias_config;
	}

	if (pdata->hsed_name != NULL)
		ipd->name = pdata->hsed_name;
	else
		ipd->name = "pmic8058_othc";

	ipd->phys = "pmic8058_othc/input0";
	ipd->dev.parent = &pd->dev;

	dd->othc_ipd = ipd;
	dd->othc_sw_state = false;
	dd->switch_debounce_ms = hsed_config->switch_debounce_ms;
	dd->othc_support_n_switch = hsed_config->othc_support_n_switch;
	dd->accessory_support = pdata->hsed_config->accessories_support;
	dd->detection_delay_ms = pdata->hsed_config->detection_delay_ms;

	if (dd->othc_support_n_switch == true)
		dd->switch_config = hsed_config->switch_config;

	if (dd->accessory_support == true) {
		dd->accessory_info = pdata->hsed_config->accessories;
		dd->num_accessories = pdata->hsed_config->othc_num_accessories;
	}

	/* Configure the MIC_BIAS line for headset detection */
	rc = pm8058_configure_micbias(dd);
	if (rc < 0)
		goto fail_micbias_config;

	input_set_capability(ipd, EV_SW, SW_HEADPHONE_INSERT);
	input_set_drvdata(ipd, dd);
	spin_lock_init(&dd->lock);

	rc = input_register_device(ipd);
	if (rc) {
		pr_err("Unable to register OTHC device\n");
		goto fail_micbias_config;
	}

	/* Request the HEADSET detect interrupt */
	rc = request_threaded_irq(dd->othc_irq_det, NULL, headset_det_irq,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_DISABLED,
				"headset_det_irq", dd);
	if (rc < 0) {
		pr_err("Unable to request headset_det_irq IRQ \n");
		goto fail_det_irq;
	}

	/* Request the  SWITCH press/release interrupt */
	rc = request_threaded_irq(dd->othc_irq_sw, NULL, pm8058_no_sw,
	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			"pm8058_othc_sw", dd);
	if (rc < 0) {
		pr_err("Unable to request pm8058_othc_sw IRQ \n");
		goto fail_sw_irq;
	}
	disable_irq(dd->othc_irq_sw);

	dd->headset_status = OTHC_HEADSET_REMOVE;
	dd->switch_status = OTHC_HEADSET_SWITCH_OFF;
	check_headset_status(dd);
	
	INIT_DELAYED_WORK(&dd->detect_work, detect_work_f);

	return 0;

fail_sw_irq:
	free_irq(dd->othc_irq_ir, dd);
fail_det_irq:
	input_unregister_device(ipd);
	dd->othc_ipd = NULL;
fail_micbias_config:
	input_free_device(ipd);
fail_input_alloc:
	return rc;
}

static int __devinit pm8058_othc_probe(struct platform_device *pd)
{
	int rc;
	struct pm8058_othc *dd;
	struct resource *res;
	struct pmic8058_othc_config_pdata *pdata = pd->dev.platform_data;

	if (pdata == NULL) {
		pr_err("Platform data not present\n");
		return -EINVAL;
	}

	dd = kzalloc(sizeof(*dd), GFP_KERNEL);
	if (dd == NULL) {
		pr_err("Unable to allocate memory\n");
		return -ENOMEM;
	}
	
	res = platform_get_resource_byname(pd, IORESOURCE_IO, "othc_base");
	if (res == NULL) {
		pr_err("othc resource:Base address absent\n");
		rc = -ENXIO;
		goto fail_get_res;
	}

	dd->dev = &pd->dev;
	dd->othc_pdata = pdata;
	dd->othc_base = res->start;
    dd->headset_detect_pin = 0;
    dd->mic_ctrl_pin = 0;

	if (pdata->micbias_regulator == NULL) {
		pr_err("OTHC regulator not specified\n");
		rc = -EFAULT;
		goto fail_get_res;
	}

	dd->othc_vreg = regulator_get(NULL,
				pdata->micbias_regulator->regulator);
	if (IS_ERR(dd->othc_vreg)) {
		pr_err("regulator get failed\n");
		rc = PTR_ERR(dd->othc_vreg);
		goto fail_get_res;
	}

	rc = regulator_set_voltage(dd->othc_vreg,
				pdata->micbias_regulator->min_uV,
				pdata->micbias_regulator->max_uV);
	if (rc) {
		pr_err("othc regulator set voltage failed\n");
		goto fail_reg_enable;
	}

	rc = regulator_enable(dd->othc_vreg);
	if (rc) {
		pr_err("othc regulator enable failed\n");
		goto fail_reg_enable;
	}

	platform_set_drvdata(pd, dd);

	if (pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		/* HSED to be supported on this MICBIAS line */
		if (pdata->hsed_config != NULL) {
			is_init_pmic8058 = 1 ;
			rc = othc_configure_hsed(dd, pd);
			is_init_pmic8058 = 0 ;
			if (rc < 0)
				goto fail_othc_hsed;
            
            set_bit(EV_KEY, dd->othc_ipd->evbit);
            set_bit(KEY_MEDIA, dd->othc_ipd->keybit);   
            
		} else {
			pr_err("HSED config data not present\n");
			rc = -EINVAL;
			goto fail_othc_hsed;
		}
	}

	/* Store the local driver data structure */
	if (dd->othc_pdata->micbias_select < OTHC_MICBIAS_MAX)
		config[dd->othc_pdata->micbias_select] = dd;

	pr_debug("Device %s:%d successfully registered\n",
					pd->name, pd->id);
	return 0;


fail_othc_hsed:
	regulator_disable(dd->othc_vreg);
fail_reg_enable:
	regulator_put(dd->othc_vreg);
fail_get_res:

	kfree(dd);
	return rc;
}

static struct platform_driver pm8058_othc_driver = {
	.driver = {
		.name = "pm8058-othc",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &pm8058_othc_pm_ops,
#endif
	},
	.probe = pm8058_othc_probe,
	.remove = __devexit_p(pm8058_othc_remove),
};

static int __init pm8058_othc_init(void)
{
	return platform_driver_register(&pm8058_othc_driver);
}

static void __exit pm8058_othc_exit(void)
{
	platform_driver_unregister(&pm8058_othc_driver);
}
/*
 * Move to late_initcall, to make sure that the ADC driver registration is
 * completed before we open a ADC channel.
 */
late_initcall(pm8058_othc_init);
module_exit(pm8058_othc_exit);

MODULE_ALIAS("platform:pmic8058_othc");
MODULE_DESCRIPTION("PMIC 8058 OTHC");
MODULE_LICENSE("GPL v2");
