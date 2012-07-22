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
#include <linux/err.h>
#include <linux/mfd/pmic8058.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/msm-charger.h>
#include <linux/time.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include <asm/atomic.h>

#include <mach/msm_hsusb.h>
#include <mach/gpio.h>

#ifdef CONFIG_BATTERY_BQ275X0
#include <linux/i2c/bq275x0_battery.h>
#endif

#include <linux/jiffies.h>

#define MSM_CHG_MAX_EVENTS		16

#define MAX8903_CEN_N 131
#define USB_CHARGING_CTRL 145

/**
 * enum msm_battery_status
 * @BATT_STATUS_ABSENT: battery not present
 * @BATT_STATUS_ID_INVALID: battery present but the id is invalid
 * @BATT_STATUS_DISCHARGING: battery is present and is discharging
 * @BATT_STATUS_TRKL_CHARGING: battery is being trickle charged
 * @BATT_STATUS_FAST_CHARGING: battery is being fast charged
 * @BATT_STATUS_JUST_FINISHED_CHARGING: just finished charging,
 *		battery is fully charged. Do not begin charging untill the
 *		voltage falls below a threshold to avoid overcharging
 * @BATT_STATUS_TEMPERATURE_OUT_OF_RANGE: battery present,
					no charging, temp is hot/cold
 */
enum msm_battery_status {
	BATT_STATUS_ABSENT,
	BATT_STATUS_ID_INVALID,
	BATT_STATUS_DISCHARGING,
	BATT_STATUS_TRKL_CHARGING,
	BATT_STATUS_FAST_CHARGING,
	BATT_STATUS_CHARGING,
	BATT_STATUS_JUST_FINISHED_CHARGING,
	BATT_STATUS_TEMPERATURE_OUT_OF_RANGE,
};

struct msm_hardware_charger_priv {
	struct list_head list;
	struct msm_hardware_charger *hw_chg;
	enum msm_hardware_charger_state hw_chg_state;
#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock wl;
#endif
};


struct msm_charger_event {
	enum msm_hardware_charger_event event;
	struct msm_hardware_charger *hw_chg;
};

struct msm_charger_mux {
	int inited;
	struct list_head msm_hardware_chargers;
	int count_chargers;
	struct mutex msm_hardware_chargers_lock;

	struct device *dev;

	unsigned int update_time;

	struct delayed_work update_heartbeat_work;

	struct mutex status_lock;
	enum msm_battery_status batt_status;

//    
//	int usb_charging_en;
//	int usb_charging_control;
//    
	u32 start_charging_jiffies;

	int (*setup_charging_gpio_status)(int flag, unsigned gpio_num, int status);	
	struct msm_charger_event *queue;
	int tail;
	int head;
	spinlock_t queue_lock;
	int queue_count;
	struct delayed_work queue_work;
	struct workqueue_struct *event_wq_thread;

	int  batt_present;
	int  batt_vol;	
	int  batt_cur;
	int  batt_cap;
	int  batt_temp;	
	int  batt_tte;
};

static struct msm_charger_mux msm_chg;

extern struct i2c_client* g_battery_measure_by_bq275x0_i2c_client;
static struct bq275x0_device_info di;

static struct platform_driver msm_charger_driver;


#define MAX_UP_POINT		4
#define MAX_DOWN_POINT	4
static int old_battery_capacity = -1;
static int old_charging = 0;
static int charging = 0;
#define SMOOTH_ADJUST	1
#define MAX_SMOOTH_STEP	4
static int smooth_step = 0;

static int smooth_capacity(int new_cap)
{
	int difference;
	int result = 0;

	if ((10 > new_cap)&&(2 <= new_cap)) {
		new_cap = 2;
	}

	if (charging == 0) {
		if ((0 == smooth_step)&&(9 == new_cap%10)&&(90 > new_cap)) {
			smooth_step = 1;
		}
	} else {
		if ((0 == smooth_step)&&(0 == new_cap%10)&&(91 > new_cap)) {
			smooth_step = 1;
		}
	}
		
	if ((20 > new_cap)&&(10 <= new_cap)) {
		new_cap = new_cap - (9 - 20/10);
	}

	if ((90 > new_cap)&&(20 <= new_cap)) { 
		new_cap = new_cap - (9 - new_cap/10);
	}
	
	//pr_err("old_battery_capacity %d \n smooth_step %d  middle_num is %d\n",old_battery_capacity,smooth_step,new_cap);

	if (-1 == old_battery_capacity) {
		old_battery_capacity = new_cap;
	}
		
	if (charging == 0) {		//NO_charging
		difference = old_battery_capacity - new_cap;
		if (0 > difference) {
			return old_battery_capacity;
		}

		if (0 != smooth_step) {
			if (0 != difference) {
				result = old_battery_capacity - SMOOTH_ADJUST;
				old_battery_capacity = result;
			}
			return old_battery_capacity;
		}
		
		if (MAX_DOWN_POINT <= difference) {
			result = old_battery_capacity - MAX_DOWN_POINT;
			old_battery_capacity = result;
			return result;
		}
	} else {		//charging
		difference = new_cap - old_battery_capacity;
		if (0 > difference) {
			return old_battery_capacity;
		}
		
		if (0 != smooth_step) {
			if (0 != difference) {
				result = old_battery_capacity + SMOOTH_ADJUST;
				old_battery_capacity = result;
			}
			return old_battery_capacity;
		}

		if (MAX_UP_POINT <= difference) {
			result = old_battery_capacity + MAX_DOWN_POINT;
			old_battery_capacity = result;
			return result;
		}
	}
	
	return new_cap;
}


static void sort_data(int* data, int len)
{  
	int i = 0;
	int j = 0;
	int temp =0;  

	for (i = 0; i < len; i++) {  
		for (j = i+1; j < len; j++) {  
			if(data[i]>data[j]){  
				temp = data[j];  
				data[j] = data[i];  
				data[i] = temp;  
			}  
		}  
	}  
}

void get_batt_status(void) 
{
	int cap[5];
	int loop = 0;
	int value;
	int tmp = 0;

	for(loop = 0; loop < 5; loop++) {
		cap[loop] = bq275x0_battery_capacity(&di);
		//pr_err("WXX_DEBUG cap[%d] = %d\n", loop, cap[loop]);
		msleep(10);
	}
	sort_data(cap, 5);

	value = bq275x0_battery_voltage(&di);
	if(value >= 0)
		msm_chg.batt_vol = value;

	value = bq275x0_battery_current(&di);
//	if(value >= 0)		
	msm_chg.batt_cur = value;
	old_charging = charging;
	if(0 > smooth_step){
		smooth_step = 0;
	}
	if(0 > value){ //No_charging
	charging = 0;
	}
	else{		//charging
	charging = 1;
	}
	
	if(0 == smooth_step){
		if(old_charging != charging)
			smooth_step = MAX_SMOOTH_STEP;
	}
	value = bq275x0_battery_temperature(&di);
	if(value >= 0)
		msm_chg.batt_temp = value;

	value = cap[2];
	if (value >= 0) {
		tmp = value;
		
		old_battery_capacity = smooth_capacity(value);
		if(0 != smooth_step){
			smooth_step--;
		}
		//pr_err("smooth_capacity ver 2.2 charging is %d  new_cap is %d  modified_cap is %d\n",charging,tmp,old_battery_capacity);
		msm_chg.batt_cap = old_battery_capacity;
	}

	value = bq275x0_battery_tte(&di);
	if(value >= 0)
		msm_chg.batt_tte = value;

	value = is_bq275x0_battery_exist(&di);
	if(value >= 0)
		msm_chg.batt_present = value;

}

static int is_battery_present(void)
{
	return msm_chg.batt_present;
}

static int is_battery_temp_within_range(void)
{
	return 1;
}


static int is_battery_charging_full(void)
{
	return (msm_chg.batt_cap == 100);
}

/*check whether the capacity is low to charge */
#define BATT_CHARGING_LEVEL 99	
static int is_battery_capacity_lower_setting(void)
{

	return (msm_chg.batt_cap  <=  BATT_CHARGING_LEVEL);	
}

static int is_batt_status_charging(void)
{
	if (msm_chg.batt_status == BATT_STATUS_CHARGING)
		return 1;
	return 0;
}

void set_max8903_cen(int flag)
{
	gpio_direction_output(MAX8903_CEN_N, !flag);
}
static ssize_t disable(struct device_driver *driver,const char *buf,size_t count)
{
	int flag=(int)buf[0] -0x30;
	if(flag)
	{
	    set_max8903_cen(0);
		dev_info(msm_chg.dev, "[%s %d] close charging...... \n", __func__, __LINE__ );
	}
	else
	{
		set_max8903_cen(1);
		dev_info(msm_chg.dev, "[%s %d] open  charging...... \n", __func__, __LINE__ );
	}
	return count;
}
static DRIVER_ATTR(chg_enable, S_IWUSR|S_IWGRP, NULL, disable);

//static void set_usb_charging_ctrl(int flag)
//{
//	gpio_direction_output(USB_CHARGING_CTRL, !!flag);
//}
//
//
//static int open_close_charging(int flag)
//{
//    
//	if(flag)
//	{
//		set_max8903_cen(1);
//		dev_dbg(msm_chg.dev, "[%s %d] open charging...... \n", __func__, __LINE__ );
//	}
//	else
//	{
//		set_max8903_cen(0);
//		dev_dbg(msm_chg.dev, "[%s %d] close charging...... \n", __func__, __LINE__ );
//	}
//    
//
//	return 0;
//
//}

static int is_wall_charger_exist(void)		
{
	int ret = 0;
	struct msm_hardware_charger_priv *hw_chg_priv;

	list_for_each_entry(hw_chg_priv, &msm_chg.msm_hardware_chargers, list) {
			if (hw_chg_priv->hw_chg_state != CHG_ABSENT_STATE && hw_chg_priv->hw_chg->rating == 2) {
				ret = 1;
				break;
			}
	}
	return ret;
}

//static struct msm_hardware_charger * get_usb_charger(void)
//{
//	struct msm_hardware_charger_priv *hw_chg_priv;
//
//	list_for_each_entry(hw_chg_priv, &msm_chg.msm_hardware_chargers, list) {
//			if (hw_chg_priv->hw_chg->rating == 1) {
//				return hw_chg_priv->hw_chg;
//			}
//	}
//	return NULL;
//}

/* This function should only be called within handle_event or resume*/
static void update_batt_status(void)
{
	enum msm_battery_status last_status = msm_chg.batt_status;

	dev_dbg(msm_chg.dev, "[%s %d] update battery status now...... \n", __func__, __LINE__ );

	get_batt_status();	

	if(is_battery_present())
	{
		if(is_battery_temp_within_range())
		{
			if(is_wall_charger_exist()) {			
				if(is_battery_capacity_lower_setting()) {
					msm_chg.batt_status = BATT_STATUS_CHARGING;
					dev_dbg(msm_chg.dev, "[%s %d] battery need charging...... \n", __func__, __LINE__ );
					goto open_charging;
				}
				else
				{
					if(last_status != BATT_STATUS_CHARGING)
					{
						msm_chg.batt_status = BATT_STATUS_JUST_FINISHED_CHARGING;
					dev_dbg(msm_chg.dev, "[%s %d] battery needn't charge...... \n", __func__, __LINE__ );
						goto close_charging;
					}
					else
					{
						if(is_battery_charging_full())  
						{						
							msm_chg.batt_status = BATT_STATUS_JUST_FINISHED_CHARGING;
						dev_dbg(msm_chg.dev, "[%s %d] battery charge to full...... \n", __func__, __LINE__ );
							goto close_charging;
						}
					}
				}
			}
			else 
			{
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
				goto close_charging;				
			}
		}
		else
		{
			msm_chg.batt_status = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
			dev_dbg(msm_chg.dev, "[%s %d] battery temperature is too high...... \n", __func__, __LINE__ );
			goto close_charging;
		}
	}
	else
	{
		msm_chg.batt_status = BATT_STATUS_ABSENT;
		dev_dbg(msm_chg.dev, "[%s %d] battery is absent...... \n", __func__, __LINE__ );
		goto close_charging;
	}

open_charging:

	if(last_status != BATT_STATUS_CHARGING)
	{
		dev_dbg(msm_chg.dev, "[%s %d] battery turn to charging...... \n", __func__, __LINE__ );
		//open_close_charging(1);	
		msm_chg.start_charging_jiffies = jiffies;
	}	
	return;	

close_charging:

	if(last_status == BATT_STATUS_CHARGING)
	{
		dev_dbg(msm_chg.dev, "[%s %d] battery turn to discharging...... \n", __func__, __LINE__ );
		//open_close_charging(0);	
	}	
	return;
}

static enum power_supply_property msm_batt_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
};

static enum power_supply_property gauge_batt_power_props[] = {
//	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,	
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
};

static int msm_batt_property_filter(enum power_supply_property psp)
{
	int iLoop;
	for(iLoop=0; iLoop < ARRAY_SIZE(gauge_batt_power_props); iLoop++)
	{
		if(psp == gauge_batt_power_props[iLoop])
		{
			return 1;
		}
	}
	return 0;
}

static int msm_gauge_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = msm_chg.batt_cur;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = msm_chg.batt_vol * 1000;			//bq27x50
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = msm_chg.batt_cap;	
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = msm_chg.batt_temp; 
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = 0;
		if(is_batt_status_charging()) {
			val->intval = (jiffies - msm_chg.start_charging_jiffies)/HZ;
		}
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = msm_chg.batt_tte;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int msm_batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (is_batt_status_charging() && is_wall_charger_exist())   
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if ((msm_chg.batt_status == BATT_STATUS_JUST_FINISHED_CHARGING)
                    && is_wall_charger_exist())   
			val->intval = POWER_SUPPLY_STATUS_FULL;		//
		else if (msm_chg.batt_status != BATT_STATUS_ABSENT)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (is_batt_status_charging())
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = msm_chg.batt_present;	
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 4200;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 3600;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static int msm_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	if(msm_batt_property_filter(psp)) {
		return msm_gauge_power_get_property(psy, psp, val);
	}
	else {
		return msm_batt_power_get_property(psy, psp, val);	
	}
}

static struct power_supply msm_psy_batt = {
	.name = "battery_gauge",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = msm_batt_power_props,
	.num_properties = ARRAY_SIZE(msm_batt_power_props),
/* Begin: wuxinxian w00176579 20110123 modify for bq275x0	*/
	.get_property = msm_power_get_property,
/* End: wuxinxian w00176579 20110123 modify for bq275x0	*/	
};

static void update_heartbeat(struct work_struct *work)
{

	mutex_lock(&msm_chg.status_lock);
	update_batt_status();
	mutex_unlock(&msm_chg.status_lock);

	/* notify that the voltage has changed
	 * the read of the capacity will trigger a
	 * voltage read*/
	power_supply_changed(&msm_psy_batt);

	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
}

static void handle_charger_inserted(struct msm_hardware_charger_priv *hw_chg_priv)
{
	hw_chg_priv->hw_chg_state = CHG_PRESENT_STATE;	
}

static void handle_charger_removed(struct msm_hardware_charger_priv *hw_chg_priv)
{
	hw_chg_priv->hw_chg_state = CHG_ABSENT_STATE;	
}

//
//static void check_usb_charging(struct msm_hardware_charger *hw_chg)
//{
//    
//	struct msm_hardware_charger_priv *hw_chg_priv;
//
//	if(hw_chg)
//		hw_chg_priv = hw_chg->charger_private;
//	else
//		goto exit_check;
//
//	if(hw_chg->rating != 1)
//		goto exit_check;
//
//	if(hw_chg_priv->hw_chg_state != CHG_ABSENT_STATE && msm_chg.usb_charging_control)   
//		msm_chg.usb_charging_en = 1;
//	else
//		msm_chg.usb_charging_en = 0;
//   
//
//exit_check:
//	pr_debug("usb charging en = %d\n", msm_chg.usb_charging_en);
//
//
//}
//

static void handle_event(struct msm_hardware_charger *hw_chg, int event)
{
	struct msm_hardware_charger_priv *hw_chg_priv;
	
	hw_chg_priv = hw_chg->charger_private;
	dev_dbg(msm_chg.dev, "%s %d from %s\n", __func__, event, hw_chg->charger.name);
	//msleep(200);		
	if (event == CHG_STAT_EVENT) {
		if(hw_chg->get_charger_status(hw_chg->gpio_num)) {	/*this debounces it */
			event = CHG_INSERTED_EVENT;
		} else {
			event = CHG_REMOVED_EVENT;
		}
	}
	//msleep(200);
	mutex_lock(&msm_chg.status_lock);
	
	switch (event) {
	case CHG_INSERTED_EVENT:	
		handle_charger_inserted(hw_chg_priv);
		//#ifdef CONFIG_HAS_WAKELOCK
			//wake_lock_timeout(&hw_chg_priv->wl, 500);
 		//#endif		
		break;
	case CHG_REMOVED_EVENT:
		handle_charger_removed(hw_chg_priv);
		break;
//	case CHG_USB_ON:
//		msm_chg.usb_charging_control = 1;
//		set_usb_charging_ctrl(0);   
//		break;
//	case CHG_USB_OFF:
//		msm_chg.usb_charging_control = 0;
//		set_usb_charging_ctrl(1);        
//		break;
	default:
		pr_err("error event...\n");
		break;
	}	

//	check_usb_charging(hw_chg);

	update_batt_status();

//	power_supply_changed(&msm_psy_batt);
	power_supply_changed(&hw_chg->charger);

	mutex_unlock(&msm_chg.status_lock);

}

static int msm_chg_dequeue_event(struct msm_charger_event **event)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_chg.queue_lock, flags);
	if (msm_chg.queue_count == 0) {
		spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
		return -EINVAL;
	}
	*event = &msm_chg.queue[msm_chg.head];
	msm_chg.head = (msm_chg.head + 1) % MSM_CHG_MAX_EVENTS;
	pr_debug("%s dequeueing %d from %s\n", __func__,
				(*event)->event, (*event)->hw_chg->charger.name);
	msm_chg.queue_count--;
	spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
	return 0;
}

static int msm_chg_enqueue_event(struct msm_hardware_charger *hw_chg,
			enum msm_hardware_charger_event event)
{
	unsigned long flags;
	unsigned char full = 0;


	spin_lock_irqsave(&msm_chg.queue_lock, flags);
	if (msm_chg.queue_count == MSM_CHG_MAX_EVENTS) {
#if 0
		spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
		pr_err("%s: queue full cannot enqueue %d from %s\n",
				__func__, event, hw_chg->charger.name);
#endif
		full = 1;
	}
	pr_debug("%s queueing %d from %s\n", __func__, event, hw_chg->charger.name);
	msm_chg.queue[msm_chg.tail].event = event;
	msm_chg.queue[msm_chg.tail].hw_chg = hw_chg;
	if(!full) {
		msm_chg.tail = (msm_chg.tail + 1)%MSM_CHG_MAX_EVENTS;
		msm_chg.queue_count++;		
	}
	spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
	return 0;
}

static void process_events(struct work_struct *work)
{
	struct msm_charger_event *event;
	int rc;

	do {
		rc = msm_chg_dequeue_event(&event);
		if (!rc)
			handle_event(event->hw_chg, event->event);
	} while (!rc);
}

//
//static ssize_t msm_charger_attr_store(struct device_driver *driver, const char *buf, size_t count)
//{
//	struct msm_hardware_charger * chg = get_usb_charger();
//
//	pr_debug("%s, %d: state = %d", __func__, __LINE__, buf[0]);
//
//	if(chg) {
//		if(buf[0] - 0x30)
//			msm_charger_notify_event(chg, CHG_USB_ON);
//		else
//			msm_charger_notify_event(chg, CHG_USB_OFF);
//	}
//
//	return count;
//}
//
//static DRIVER_ATTR(state, S_IWUGO, NULL, msm_charger_attr_store);
//

static int __init determine_initial_batt_status(void)
{
	int rc;

	/* get init state of charging */
	msm_chg.batt_present = 0;
	msm_chg.batt_vol = 0;	
	msm_chg.batt_cur = 0;
	msm_chg.batt_cap = 0;
	msm_chg.batt_temp = 0;	
	msm_chg.batt_tte = 0;
	
	get_batt_status();	
	if (is_battery_present()) {
			if (is_battery_temp_within_range())
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			else
				msm_chg.batt_status
				    = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
	} else {
		msm_chg.batt_status = BATT_STATUS_ABSENT;
	}

	rc = power_supply_register(msm_chg.dev, &msm_psy_batt);
	
	if (rc < 0) {
		dev_err(msm_chg.dev, "%s: power_supply_register failed"
			" rc=%d\n", __func__, rc);
		return rc;
	}

//	msm_chg.usb_charging_en = 0;
//	msm_chg.usb_charging_control = 1;


	/* start updaing the battery powersupply every msm_chg.update_time
	 * milliseconds */
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
		
	return 0;
}

static u8 charging_level = 100;

static ssize_t show(struct device_driver *driver,char *buf)
{
    if(NULL == buf)
    {
        return -1;
    }

	return sprintf(buf, "%d", charging_level);

}

static ssize_t store(struct device_driver *driver,const char *buf,size_t count)
{
	if(count <=0)
		return -1;

	charging_level = (u8)buf[0];	
	
	return 0;
}
static DRIVER_ATTR(cap_limit, S_IRUSR | S_IWUSR, show, store);

static int __devinit msm_charger_probe(struct platform_device *pdev)
{
	int rc;
	
	msm_chg.dev = &pdev->dev;

	if (pdev->dev.platform_data) {
		unsigned int milli_secs;

		struct msm_charger_platform_data *pdata =
		    (struct msm_charger_platform_data *)pdev->dev.platform_data;

		milli_secs = pdata->update_time * 30 * MSEC_PER_SEC;    
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}
		msm_chg.update_time = milli_secs;
		msm_chg.setup_charging_gpio_status = pdata->setup_charging_gpio_status;	
	}

	if(g_battery_measure_by_bq275x0_i2c_client)
	{
		di.client = g_battery_measure_by_bq275x0_i2c_client;
	}
	else
	{
		pr_err("%s failed, check bq275x0 module installed...\n", __func__);
		return -EINVAL;
	}
    rc = driver_create_file(&(msm_charger_driver.driver), &driver_attr_chg_enable);
	if (rc < 0)
	{
		pr_err("failed to create sysfs entry(CEN_N): %d\n", rc);
		return -1;
	}	

    rc = driver_create_file(&(msm_charger_driver.driver), &driver_attr_cap_limit);
    if (rc < 0)
    {
        printk("failed to create sysfs entry(replenish): %d\n", rc);
        return -1;
    }

//    
//	rc = driver_create_file(&(msm_charger_driver.driver), &driver_attr_state);
//	if (rc < 0)
//	{
//		pr_err("failed to create sysfs entry(state): %d\n", rc);
//		return -1;
//	}
//
//    

	mutex_init(&msm_chg.status_lock);
	INIT_DELAYED_WORK(&msm_chg.update_heartbeat_work, update_heartbeat);

	rc = determine_initial_batt_status();
	if(rc < 0) {
		pr_err("initial batt status fail...\n");
		mutex_destroy(&msm_chg.status_lock);
		return rc;
	}

	return 0;
}

static int __devexit msm_charger_remove(struct platform_device *pdev)
{
	mutex_destroy(&msm_chg.status_lock);
	power_supply_unregister(&msm_psy_batt);
//    
//	driver_remove_file(&(msm_charger_driver.driver), &driver_attr_state);
//    
	return 0;
}

int msm_charger_notify_event(struct msm_hardware_charger *hw_chg,
			     enum msm_hardware_charger_event event)
{
	struct msm_hardware_charger_priv *hw_chg_priv;
	
	hw_chg_priv = hw_chg->charger_private;

	msm_chg_enqueue_event(hw_chg, event);
	queue_delayed_work(msm_chg.event_wq_thread, &msm_chg.queue_work, round_jiffies_relative(msecs_to_jiffies
						     (10)));
	wake_lock_timeout(&hw_chg_priv->wl, 200);
	return 0;
}
EXPORT_SYMBOL(msm_charger_notify_event);

int msm_charger_register(struct msm_hardware_charger *hw_chg)
{
	struct msm_hardware_charger_priv *priv;
	int rc = 0;

	if (!msm_chg.inited) {
		pr_err("%s: msm_chg is NULL,Too early to register\n", __func__);
		return -EAGAIN;
	}

	priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (priv == NULL) {
		dev_err(msm_chg.dev, "%s kzalloc failed\n", __func__);
		return -ENOMEM;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&priv->wl, WAKE_LOCK_SUSPEND, hw_chg->charger.name);
#endif

	rc = power_supply_register(NULL, &hw_chg->charger);
	if (rc) {
		dev_err(msm_chg.dev, "%s power_supply_register failed\n",
			__func__);
		goto out;
	}

	priv->hw_chg = hw_chg;
	priv->hw_chg_state = CHG_ABSENT_STATE;
	INIT_LIST_HEAD(&priv->list);
	mutex_lock(&msm_chg.msm_hardware_chargers_lock);
	list_add_tail(&priv->list, &msm_chg.msm_hardware_chargers);
	mutex_unlock(&msm_chg.msm_hardware_chargers_lock);
	hw_chg->charger_private = (void *)priv;

	return 0;

out:
#ifdef CONFIG_HAS_WAKELOCK	
	wake_lock_destroy(&priv->wl);
#endif
	kfree(priv);
	return rc;
}
EXPORT_SYMBOL(msm_charger_register);

int msm_charger_unregister(struct msm_hardware_charger *hw_chg)
{
	struct msm_hardware_charger_priv *priv;

	priv = (struct msm_hardware_charger_priv *)(hw_chg->charger_private);
	mutex_lock(&msm_chg.msm_hardware_chargers_lock);
	list_del(&priv->list);
	mutex_unlock(&msm_chg.msm_hardware_chargers_lock);
#ifdef CONFIG_HAS_WAKELOCK	
	wake_lock_destroy(&priv->wl);
#endif
	power_supply_unregister(&hw_chg->charger);
	kfree(priv);
	return 0;
}
EXPORT_SYMBOL(msm_charger_unregister);

void cancel_update_battery_status(void)
{
	cancel_delayed_work(&msm_chg.update_heartbeat_work);
	old_battery_capacity = -1;
	smooth_step = 0;
}

static int msm_charger_suspend(struct platform_device * dev, pm_message_t state)
{


	cancel_update_battery_status();

	return 0;
}

void update_battery_status_now(void)
{
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      200);
}

static int msm_charger_resume(struct platform_device * dev)
{

	update_battery_status_now();
	return 0;
}





static struct platform_driver msm_charger_driver = {
	.probe = msm_charger_probe,
	.remove = __devexit_p(msm_charger_remove),
	.suspend = msm_charger_suspend,
	.resume = msm_charger_resume,	
	.driver = {
		   .name = "msm-charger",
		   .owner = THIS_MODULE,
	},
};

static int __init msm_charger_init(void)
{
	int rc;

	INIT_LIST_HEAD(&msm_chg.msm_hardware_chargers);
	msm_chg.count_chargers = 0;
	mutex_init(&msm_chg.msm_hardware_chargers_lock);

	msm_chg.queue = kzalloc(sizeof(struct msm_charger_event)
				* MSM_CHG_MAX_EVENTS,
				GFP_KERNEL);
	if (!msm_chg.queue) {
		rc = -ENOMEM;
		goto out;
	}
	msm_chg.tail = 0;
	msm_chg.head = 0;
	spin_lock_init(&msm_chg.queue_lock);
	msm_chg.queue_count = 0;
	INIT_DELAYED_WORK(&msm_chg.queue_work, process_events);
	msm_chg.event_wq_thread = create_workqueue("msm_charger_eventd");

	pr_info("work_queue_init_OK......");
	
	if (!msm_chg.event_wq_thread) {
		rc = -ENOMEM;
		goto free_queue;
	}
	rc = platform_driver_register(&msm_charger_driver);
	if (rc < 0) {
		pr_err("%s: FAIL: platform_driver_register. rc = %d\n",
		       __func__, rc);
		goto destroy_wq_thread;
	}

	msm_chg.inited = 1;
	return 0;

destroy_wq_thread:
	destroy_workqueue(msm_chg.event_wq_thread);
free_queue:
	kfree(msm_chg.queue);
out:
	return rc;
}

static void __exit msm_charger_exit(void)
{
	flush_workqueue(msm_chg.event_wq_thread);
	destroy_workqueue(msm_chg.event_wq_thread);
	kfree(msm_chg.queue);
	platform_driver_unregister(&msm_charger_driver);
}

module_init(msm_charger_init);
module_exit(msm_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Battery driver for Qualcomm MSM chipsets.");
MODULE_VERSION("1.0");
