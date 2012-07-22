/*
 * drivers/input/touchscreen/mxt422_generic.c
 *
 * Copyright (c) 2011 Huawei Device Co., Ltd.
 *	Li Yaobing <liyaobing@S7.com>
 *
 * Using code from:
 *  The code is originated from Atmel Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __LINUX_I2C_MXT224_H
#define __LINUX_I2C_MXT224_H

#include <linux/i2c.h>
#include <linux/earlysuspend.h>

#define MAX_FINGERS		5

struct ts_event {
	uint8_t touch_number_id;//liyaobing214
    uint8_t tchstatus; 
	u16	x;
	u16	y;
    uint8_t tcharea;
    uint8_t tchamp;
	uint8_t tchvector;
};

struct mxt224 {
	struct input_dev	*input;
	char phys[32];
	struct hrtimer hr_timer;
   	struct timer_list timer;
    struct early_suspend early_suspend;
	struct ts_event tc;
	uint32_t x_max;
	uint32_t y_max;
	struct i2c_client *client;
	spinlock_t lock;
	int irq; 
};

extern int mxt224_generic_probe(struct mxt224 *tsc);
extern void mxt224_generic_remove(struct mxt224 *tsc);
extern void mxt224_get_message(struct mxt224 *tsc);
extern void mxt224_update_pen_state(void *tsc);
extern uint8_t config_disable_mxt244(void);
extern uint8_t config_enable_mxt244(void);
#ifdef CONFIG_UPDATE_MXT224_FIRMWARE  
extern int mxt224_update_firmware(void);
#define MAX_OBJECT_NUM						15
#define MAX_OBJECT_CONFIGDATA_NUM		50
extern unsigned char config_data[MAX_OBJECT_NUM][MAX_OBJECT_CONFIGDATA_NUM];
#endif
extern uint8_t backup_config(void);
extern uint8_t reset_chip(void);
extern uint8_t get_object_size(uint8_t object_type);
extern uint16_t get_object_address(uint8_t object_type, uint8_t instance);
extern uint8_t calibrate_chip(void) ;
 extern uint8_t write_mem(uint16_t Address, uint8_t ByteCount, uint8_t *Data);
 extern uint8_t read_mem(uint16_t Address, uint8_t ByteCount, uint8_t *Data);
 extern  uint8_t report_id_to_type(uint8_t report_id, uint8_t *instance);
#ifdef CONFIG_DEBUG_MXT224_FIRMWARE 
extern int ts_debug_X ;
extern int ts_debug_Y ;
extern int min_multitouch_report_id ;
#endif
extern int tp_is_calibrating   ;
extern int tp_is_facesuppression   ;
extern int tp_config_err;
struct mxt224_platform_data {
	u16	model;				/* 2007. */
	u16	x_plate_ohms;

	int	(*get_pendown_state)(void);
	void	(*clear_penirq)(void);		/* If needed, clear 2nd level
						   interrupt source */
	int	(*init_platform_hw)(void);
	void	(*exit_platform_hw)(void);
	int	(*interrupts_pin_status)(void);
	int	(*chip_reset)(void);
	int	(*chip_poweron_reset)(void);
	int	(*chip_poweron)(void);
	int	(*chip_poweroff)(void);
	int (*config_tp_5v)(int enable);
};

#ifdef CONFIG_UPDATE_MXT224_FIRMWARE 
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
#endif
