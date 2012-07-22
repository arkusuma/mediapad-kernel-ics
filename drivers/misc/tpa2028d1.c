/** 
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
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
 
/**
 *  bs300-audio I2C driver.
 *  Belasigna BS300 (MIC noise reducer) driver 
 */
 
//#include <linux/module.h>
//#include <linux/param.h>
//#include <linux/delay.h>
//#include <linux/platform_device.h>
#include <linux/i2c.h>
//#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <mach/mpp.h>
#include <linux/delay.h>
#include <linux/i2c/tpa2028d1.h>
#include "tpa2028d1_data.h"


#define CONTROL_RGE (1)

struct i2c_client* tpa2028d1_i2c_client = NULL;

static struct i2c_driver tpa2028d1_driver;

#define UPDATE_STRING "updatetpa2028d1"

static DEFINE_MUTEX(tpa2028d1_mutex);

static int tpa2028d1_read_byte(unsigned char  offset, unsigned char* data)
{
	int ret;
	struct i2c_msg msg[1];
	struct i2c_client *client = tpa2028d1_i2c_client;
	if (!client->adapter)
	{
		pr_err("[%s,%d]invalid i2c client\n",__FUNCTION__,__LINE__);
		return -EFAULT;
	}

	msg->addr = client->addr;
	msg->len = 1;
	msg->flags = 0;		/*i2c write flag*/
	msg->buf = &offset;

	ret = i2c_transfer(client->adapter, msg, 1);
	if(ret<0)
	{
		pr_err("[%s,%d]i2c write byte fail, addr = %x, err = %d\n"
					,__FUNCTION__,__LINE__,msg->addr, ret);
		goto out_err;
	}


	msg->addr = client->addr;
	msg->len = 1;
	msg->flags = I2C_M_RD;
	msg->buf = data;

	ret = i2c_transfer(client->adapter, msg, 1);
	if(ret<0)
	{
		pr_err("[%s,%d]i2c read byte fail, addr = %x, err = %d\n"
					,__FUNCTION__,__LINE__,msg->addr, ret);
		goto out_err;
	}

	return 0;

out_err:

	return -ENODEV;
}

static int tpa2028d1_write_byte(unsigned char  offset, unsigned char data)
{
	int ret;
	struct i2c_msg msg[1];
	struct i2c_client *client = tpa2028d1_i2c_client;
	unsigned char dat[2];
	if (!client->adapter)
	{
		pr_err("[%s,%d]invalid i2c client\n",__FUNCTION__,__LINE__);
		return -EFAULT;
	}

	dat[0] = offset;
	dat[1] = data;

	msg->addr = client->addr;
	msg->len = 2;
	msg->flags = 0;		/*i2c write flag*/
	msg->buf = dat;

	ret = i2c_transfer(client->adapter, msg, 1);
	if(ret<0)
	{
		pr_err("[%s,%d]i2c write byte fail, addr = %x, err = %d\n"
					,__FUNCTION__,__LINE__,msg->addr, ret);
		goto out_err;
	}
	return 0;

out_err:

	return -ENODEV;
	
}


int enable_tpa2028d1_function(void)
{
	unsigned char dat;
	int ret;
	ret = tpa2028d1_read_byte(CONTROL_RGE, &dat);
	if(ret < 0) {
		pr_err("%s failed \n", __func__);
		return -EFAULT;
	}

	dat |= 1<<6;		//set en bit

	return tpa2028d1_write_byte(CONTROL_RGE, dat);
}
EXPORT_SYMBOL(enable_tpa2028d1_function);

int disable_tpa2028d1_function(void)
{
	unsigned char dat;
	int ret;
	ret = tpa2028d1_read_byte(CONTROL_RGE, &dat);
	if(ret < 0) {
		pr_err("%s failed \n", __func__);
		return -EFAULT;
	}

	dat &= ~(1<<6);		//cls en bit

	return tpa2028d1_write_byte(CONTROL_RGE, dat);
}
EXPORT_SYMBOL(disable_tpa2028d1_function);

#define PM8901_MPP_3 (2) /* PM8901 MPP starts from 0 */
/* GPIO_CLASS_D0_EN */
#define SNDDEV_GPIO_CLASS_D0_EN 227

static void config_class_d0_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = pm8901_mpp_config_digital_out(PM8901_MPP_3,
			PM8901_MPP_DIG_LEVEL_MSMIO, 1);

		if (rc) {
			pr_err("%s: CLASS_D0_EN failed\n", __func__);
			return;
		}

		rc = gpio_request(SNDDEV_GPIO_CLASS_D0_EN, "CLASSD0_EN");

		if (rc) {
			pr_err("%s: spkr pamp gpio pm8901 mpp3 request"
			"failed\n", __func__);
			pm8901_mpp_config_digital_out(PM8901_MPP_3,
			PM8901_MPP_DIG_LEVEL_MSMIO, 0);
			return;
		}

		gpio_direction_output(SNDDEV_GPIO_CLASS_D0_EN, 1);
		gpio_set_value(SNDDEV_GPIO_CLASS_D0_EN, 1);

	} else {
		pm8901_mpp_config_digital_out(PM8901_MPP_3,
		PM8901_MPP_DIG_LEVEL_MSMIO, 0);
		gpio_set_value(SNDDEV_GPIO_CLASS_D0_EN, 0);
		gpio_free(SNDDEV_GPIO_CLASS_D0_EN);
	}
}

/* begin: added by z00168965 for speaker */
#define PM8901_MPP_1 (0)
#define SNDDEV_GPIO_BOOST_5V_EN 225

static void config_boost_5v_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = pm8901_mpp_config_digital_out(PM8901_MPP_1,
			PM8901_MPP_DIG_LEVEL_MSMIO, 1);

		if (rc) {
			pr_err("%s: CLASS_D0_EN failed\n", __func__);
			return;
		}

		rc = gpio_request(SNDDEV_GPIO_BOOST_5V_EN, "CLASSD0_EN");

		if (rc) {
			pr_err("%s: spkr pamp gpio pm8901 mpp3 request"
			"failed\n", __func__);
			pm8901_mpp_config_digital_out(PM8901_MPP_1,
			PM8901_MPP_DIG_LEVEL_MSMIO, 0);
			return;
		}

		gpio_direction_output(SNDDEV_GPIO_BOOST_5V_EN, 1);
		gpio_set_value(SNDDEV_GPIO_CLASS_D0_EN, 1);

	} else {
		pm8901_mpp_config_digital_out(PM8901_MPP_1,
		PM8901_MPP_DIG_LEVEL_MSMIO, 0);
		gpio_set_value(SNDDEV_GPIO_BOOST_5V_EN, 0);
		gpio_free(SNDDEV_GPIO_BOOST_5V_EN);
	}
}

/* end: added by z00168965 for speaker */

static void show_tpa2028_regs(void)
{
	int i;
	unsigned char data;

	pr_info("show tpa2028 registers... \n");
	
	for(i=1; i<8; i++) {
		if(tpa2028d1_read_byte(i, &data) < 0) {
			break;
		}
		pr_info("reg%d = %d", i, data);
	}

	return;
}

static void write_style_regs(int style_)
{
	int len;

	mutex_lock(&tpa2028d1_mutex);

	for(len = 1; len < 7; len++) {
		pr_info("reg%d = %d \n", len+1, tpa2028_datas[style_][len]);
		tpa2028d1_write_byte(len+1, tpa2028_datas[style_][len]);
	}		

	mutex_unlock(&tpa2028d1_mutex);

}

static ssize_t tpa2028d1_attr_store(struct device_driver *driver, const char *buf, size_t count)
{
	int iRet = 0;
	int len;

	pr_info("setting tpa2028d1 parameters... \n");   

	len = strlen(UPDATE_STRING);
	if(0 == strncmp(buf, UPDATE_STRING, len)) {

		write_style_regs(buf[len] - 0x30);

	}
	iRet = len + 1;

	return iRet;
}


static ssize_t tpa2028d1_attr_show(struct device_driver *driver, char *buf)
{

	show_tpa2028_regs();

	return sprintf(buf, "show tpa2028d1 registers.\n");
}

static DRIVER_ATTR(state, S_IRUGO|S_IWUGO, tpa2028d1_attr_show, tpa2028d1_attr_store);


static ssize_t tpa2028d1_attr_control_store(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned char value = buf[0] - 0x30;

	pr_info("%s , control tpa2028d1 enable/disable. \n", __func__);

	if(value) {
		enable_tpa2028d1_function();
	} else {
		disable_tpa2028d1_function();
	}

	return count;
;
}


static ssize_t tpa2028d1_attr_control_show(struct device_driver *driver, char *buf)
{

	return sprintf(buf, "Control tpa2028d1 enable/disable.\n");

}

static DRIVER_ATTR(control, S_IRUGO|S_IWUGO, tpa2028d1_attr_control_show, tpa2028d1_attr_control_store);

/**
 * I2C kernel driver module
 */
static int tpa2028d1_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int retval = 0;
//	unsigned int uiRet=0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[%s,%d]: need I2C_FUNC_I2C\n",__FUNCTION__,__LINE__);
		return -ENODEV;
	}

	tpa2028d1_i2c_client = client;

	config_boost_5v_gpio(1);
	config_class_d0_gpio(1);
	mdelay(5);

	write_style_regs(0);

	disable_tpa2028d1_function();

	retval = driver_create_file(&(tpa2028d1_driver.driver), &driver_attr_state);
	if (retval < 0)
	{
		pr_err("failed to create sysfs entry(state): %d\n", retval);
		return -1;
	}

	retval = driver_create_file(&(tpa2028d1_driver.driver), &driver_attr_control);
	if (retval < 0)
	{
		driver_remove_file(&(tpa2028d1_driver.driver), &driver_attr_state);
		pr_err("failed to create sysfs entry(state): %d\n", retval);
		return -1;
	}

	return retval;
}

static int tpa2028d1_remove(struct i2c_client *client)
{
	tpa2028d1_i2c_client = NULL;
	config_class_d0_gpio(0);
	config_boost_5v_gpio(0);

	driver_remove_file(&(tpa2028d1_driver.driver), &driver_attr_state);
	driver_remove_file(&(tpa2028d1_driver.driver), &driver_attr_control);
	
	return 0;
}

static const struct i2c_device_id tpa2028d1_id[] = {
	{"tpa2028d1-spk",0},
	{},
};

static struct i2c_driver tpa2028d1_driver = {
	.driver = {
		.name = "tpa2028d1-spk",
	},
	.probe = tpa2028d1_probe,
	.remove = tpa2028d1_remove,
	.id_table = tpa2028d1_id,
};

static int __init tpa2028d1_init(void)
{
	int ret;

	ret = i2c_add_driver(&tpa2028d1_driver);
	if (ret)
		pr_err("Unable to register tpa2028d1 driver\n");

	return ret;
}

late_initcall(tpa2028d1_init);

static void __exit tpa2028d1_exit(void)
{
	i2c_del_driver(&tpa2028d1_driver);
}
module_exit(tpa2028d1_exit);

MODULE_AUTHOR("S7 Tech. Co., Ltd. Qiu Hanning(q00170317)");
MODULE_DESCRIPTION("Belasigna BS300 (MIC noise reducer) driver");
MODULE_LICENSE("GPL");
