/*
 * include/linux/s7020.h - platform data structure for f75375s sensor
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 */
 
#ifndef _LINUX_S7020_H 
#define _LINUX_S7020_H

#include <linux/interrupt.h>  
#include <linux/earlysuspend.h>
#include "t1320.h"

struct s7020 {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	struct hrtimer timer;
	struct hrtimer timer_reset;
	struct work_struct  work_reset;
    struct work_struct  work;
	struct early_suspend early_suspend;

	__u8 data_reg;
	__u8 data_length;
	__u8 *data;
	struct i2c_msg data_i2c_msg[2];

	struct rmi_function_info f01;

	int hasF11;
	struct rmi_function_info f11;
	int f11_has_gestures;
	int f11_has_relative;
	int f11_max_x, f11_max_y;
	__u8 *f11_egr;
	bool hasEgrPinch;
	bool hasEgrPress;
	bool hasEgrFlick;
	bool hasEgrEarlyTap;
	bool hasEgrDoubleTap;
	bool hasEgrTapAndHold;
	bool hasEgrSingleTap;
	bool hasEgrPalmDetect;
	struct f11_finger_data *f11_fingers;
	
	int hasF19;
	struct rmi_function_info f19;

	int hasF30;
	struct rmi_function_info f30;
#ifdef CONFIG_UPDATE_S7020_FIRMWARE
    int hasF34;
	struct rmi_function_info f34;
#endif 

	int enable;

	unsigned int x_max;
	unsigned int y_max;
	struct timer_list timerlist;
    struct timer_list timerlistkey;    
    int	(*init_platform_hw)(void);
	void (*exit_platform_hw)(void);
	
	int	(*interrupts_pin_status)(void);
	int	(*chip_reset)(void);
	int	(*chip_poweron_reset)(void);
	int	(*chip_poweroff)(void);
	int (*config_tp_5v)(int enable);
};

#ifdef CONFIG_UPDATE_S7020_FIRMWARE
#define firmware_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0664,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}
#endif

#endif /* _LINUX_S7020_H */
