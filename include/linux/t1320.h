/*
 * include/linux/t1320.h - platform data structure for f75375s sensor
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
 
#ifndef _LINUX_T1320_H 
#define _LINUX_T1320_H

#include <linux/interrupt.h>  
#include <linux/earlysuspend.h>

typedef __u8 u4;
typedef __u16 u12;

struct rmi_function_info {

	/** This is the number of data points supported - for example, for
	 *  function $11 (2D sensor) the number of data points is equal to the number
	 *  of fingers - for function $19 (buttons)it is eqaul to the number of buttons
	 */
	__u8 points_supported;

	/** This is the interrupt register and mask - needed for enabling the interrupts
	 *  and for checking what source had caused the attention line interrupt.
	 */
	__u8 interrupt_offset;
	__u8 interrupt_mask;

	__u8 data_offset;
	__u8 data_length;
};

enum f11_finger_status {
	f11_finger_none = 0,
	f11_finger_accurate = 1,
	f11_finger_inaccurate = 2,
};

#define MAX_SAMPLE 32

struct f11_finger_data {
	enum f11_finger_status status;

	unsigned int speed;
	bool active;

	u12 x[MAX_SAMPLE];
	u12 y[MAX_SAMPLE];
	u8 z[MAX_SAMPLE];

	int x_sum, y_sum, z_sum;
	int x_avg, y_avg, z_avg;
	int x_last, y_last, z_last;

	int sample_index;
	int sample_count;
	int report_count;

	int dirty;
};

struct t1320 {
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
#ifdef CONFIG_UPDATE_T1320_FIRMWARE
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
	int	(*chip_poweron)(void);
	int	(*chip_poweroff)(void);
	int (*config_tp_5v)(int enable);
};
#ifdef CONFIG_UPDATE_T1320_FIRMWARE
/* start: modify by liyaobing 00169718 for firmware update download 20110118 */
#define firmware_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0664,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}
/* end: modify by liyaobing 00169718 for firmware update download 20110118 */
#endif

#endif /* _LINUX_T1320_H */
