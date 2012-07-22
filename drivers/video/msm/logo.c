/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>

#include "msm_fb.h"
#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 4)
#define FB_DATA_TMP 0x38600000 //0x58000000

void memset32(void *_ptr, uint32_t val, unsigned count)
{
	uint32_t *ptr = _ptr;
    count >>= 2;
    while (count--) {
        *ptr++ = val;
    }
}

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename)
{
	struct fb_info *info;
	int ret;
	struct msm_fb_data_type *mfd ;
	struct msm_fb_panel_data *pdata = NULL;
	char* vaddr;
	info = registered_fb[0];
	mfd = (struct msm_fb_data_type *)info->par;
	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
	vaddr = ioremap(FB_DATA_TMP, fb_size(info));
	memcpy((void*)info->screen_base, (void*)vaddr, fb_size(info));
	iounmap(vaddr);
	if (!mfd->panel_power_on) 
	{
		ret = pdata->on(mfd->pdev);
		if (ret == 0) 
		{
			printk(KERN_INFO "~~~~~~msm_fb: panel_power_on~~~~~~");
			mfd->panel_power_on = TRUE;
		}
	}
	return 0;
}
EXPORT_SYMBOL(load_565rle_image);
