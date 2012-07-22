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
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/mfd/pmic8058.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/msm-charger.h>
#include <linux/slab.h>

static enum power_supply_property msm_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *msm_power_supplied_to[] = {
	"battery",
};

static int charger_get_property_s7pro(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct msm_hardware_charger *pchg = 
		container_of(psy, struct msm_hardware_charger, charger);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = pchg->get_charger_status(pchg->gpio_num);	
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static irqreturn_t pm8058_chg_handler(int irq, void *dev_id)
{
	struct msm_hardware_charger *pchg = dev_id;
	//if (pchg->get_charger_status(pchg->gpio_num)) {	/*this debounces it */
		msm_charger_notify_event(pchg, CHG_STAT_EVENT);
	//} else {
		//msm_charger_notify_event(pchg, CHG_REMOVED_EVENT);
	//}
	return IRQ_HANDLED;
}


static void free_irqs(struct msm_hardware_charger *pchg)
{
	free_irq(pchg->irq, NULL);
}

static int request_irqs(struct msm_hardware_charger *pchg)
{
	int ret;
	int irq;

	irq = platform_get_irq(pchg->pdev, 0);

	if (irq < 0) {
		pr_err("%s:couldnt find irq resource. \n", __func__);
		return -EINVAL;
	} else {		
		ret = request_threaded_irq(irq, NULL,
				  pm8058_chg_handler,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				  pchg->pdev->name, pchg);
		if (ret < 0) {
			pr_err("%s:couldnt request %d %d\n", __func__, irq, ret);
		} else {
			pchg->irq = irq;			
			disable_irq_nosync(irq);
		}
	}

	return ret;
}

static void  pm8058_chg_determine_initial_state(struct msm_hardware_charger *pchg)
{

	if (pchg->get_charger_status(pchg->gpio_num)) {
		msm_charger_notify_event(pchg, CHG_INSERTED_EVENT);
	}
	enable_irq(pchg->irq);
}

//
//#ifdef CONFIG_HAS_EARLYSUSPEND
//static void s7pro_charger_early_suspend(struct early_suspend *h)
//{
//	struct msm_hardware_charger *pchg = 
//		container_of(h, struct msm_hardware_charger, early_suspend);
//
//	if(pchg->rating == 2)
//		enable_irq_wake(pchg->irq);
//}
//
//static void s7pro_charger_late_resume(struct early_suspend *h)
//{
//	struct msm_hardware_charger *pchg = 
//		container_of(h, struct msm_hardware_charger, early_suspend);
//
//	if(pchg->rating == 2)
//		disable_irq_wake(pchg->irq);
//}
//#endif
//

static int __devinit pm8058_charger_probe(struct platform_device *pdev)
{
	int ret;	
	struct pm8058_charger_platform_data *pdata;
	struct msm_hardware_charger *pchg;

	pdata = pdev->dev.platform_data;

	if(!pdata) {
		pr_err("%s: no platform_data passed in. \n", __func__);
		return -EFAULT;
	}

	pchg = kzalloc(sizeof *pchg, GFP_KERNEL);
	if (pchg == NULL) {
		pr_err("%s kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	pchg->chg_detection_config = pdata->chg_detection_config;
	pchg->get_charger_status = pdata->get_charger_status;
	pchg->pdev = pdev;
	pchg->gpio_num = pdata->gpio_num;
	pchg->rating = pdata->rating;

	pchg->charger.name = pdata->name;
	if(pdata->rating == 2) {
		pchg->charger.type = POWER_SUPPLY_TYPE_MAINS;
	} else if(pdata->rating == 1) {
		pchg->charger.type = POWER_SUPPLY_TYPE_USB;
	} else {
		goto error_type;
	}

	pchg->charger.supplied_to = msm_power_supplied_to;
	pchg->charger.num_supplicants = ARRAY_SIZE(msm_power_supplied_to);
	pchg->charger.properties = msm_power_props;
	pchg->charger.num_properties = ARRAY_SIZE(msm_power_props);
	pchg->charger.get_property = charger_get_property_s7pro;

//
//#ifdef CONFIG_HAS_EARLYSUSPEND
//	pchg->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
//	pchg->early_suspend.suspend = s7pro_charger_early_suspend;
//	pchg->early_suspend.resume = s7pro_charger_late_resume;
//	register_early_suspend(&pchg->early_suspend);
//#endif
//

	dev_set_drvdata(&pdev->dev, pchg);

	if(pchg->chg_detection_config(1, pchg->gpio_num)) {
		pr_err("%s, init config fail. \n", __func__);
		goto error_type;
	}

	ret = request_irqs(pchg);
	if (ret) {
		pr_err("%s: couldnt register interrupts\n", __func__);
		goto error_type;
	}

	ret = msm_charger_register(pchg);
	if(ret) {
		pr_err("%s register charger failed ret=%d\n",
			__func__, ret);
		goto register_fail;
	}

	/* determine what state the charger is in */
	pm8058_chg_determine_initial_state(pchg);

	return 0;
	
register_fail:
	free_irqs(pchg);
error_type:
	kfree(pchg);
	return -EFAULT;
 
}

static int __devexit pm8058_charger_remove(struct platform_device *pdev)
{
	struct msm_hardware_charger *pchg = dev_get_drvdata(&pdev->dev);
//		container_of(&pdev, struct msm_hardware_charger, pdev);

	pchg->chg_detection_config(0, pchg->gpio_num);	
	msm_charger_notify_event(pchg, CHG_REMOVED_EVENT);
	msm_charger_unregister(pchg);
//
//#ifdef CONFIG_HAS_EARLYSUSPEND
//	unregister_early_suspend(&pchg->early_suspend);
//#endif
//
	free_irqs(pchg);
	kfree(pchg);

	return 0;
}

const struct platform_device_id charger_table[] = {
	{ "wall-charger-s7pro",	 0 },
	{ "usb-charger-s7pro",	 0 },		
};


static int s7pro_charger_suspend(struct platform_device * pdev, pm_message_t state)
{
	struct msm_hardware_charger *pchg = dev_get_drvdata(&pdev->dev);

	pchg->chg_detection_config(0, pchg->gpio_num);

	enable_irq_wake(pchg->irq);

	return 0;
}

static int s7pro_charger_resume(struct platform_device * pdev)
{
	struct msm_hardware_charger *pchg = dev_get_drvdata(&pdev->dev);

	pchg->chg_detection_config(1, pchg->gpio_num);

	disable_irq_wake(pchg->irq);

	return 0;
}


static struct platform_driver pm8058_charger_driver = {
	.probe = pm8058_charger_probe,
	.remove = __devexit_p(pm8058_charger_remove),
	.id_table = charger_table,
	.suspend = s7pro_charger_suspend,
	.resume = s7pro_charger_resume,
	.driver = {
		   .name = "pm8058-charger-s7pro",
		   .owner = THIS_MODULE,
	},
};

static int __init pm8058_charger_init(void)
{
	return platform_driver_register(&pm8058_charger_driver);
}

static void __exit pm8058_charger_exit(void)
{
	platform_driver_unregister(&pm8058_charger_driver);
}

late_initcall(pm8058_charger_init);
module_exit(pm8058_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 BATTERY driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8058_charger");
