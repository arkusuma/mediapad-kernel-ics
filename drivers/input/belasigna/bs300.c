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
 
#include <linux/module.h>
#include <linux/param.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio-i2c-adpt.h>
#include <linux/input/bs300.h>
#include "belasigna300_debug_protocol.h"

#ifndef CONFIG_WITH_ECHO_CANCELLATION
#include "download_data.h"
#else 
#include "download_data_bs300.h"
#endif

#include <mach/msm_xo.h>
struct i2c_client* bs300_i2c_client = NULL;



static unsigned int bs300EnFlag = 0;


#define MAXRETRYCOUNT  10



static struct msm_xo_voter *bs300_clock;
static struct msm_xo_voter *bs300_clock_digital;
int bs300_clk_control(int on)
{
	int rc = 0;

	if (on) {
		printk(KERN_INFO "open bs300_clk \n");
		bs300_clock_digital = msm_xo_get(MSM_XO_TCXO_D0, "bs300_clk_d");
        bs300_clock = msm_xo_get(MSM_XO_TCXO_A1, "bs300_clk");

        if (IS_ERR(bs300_clock_digital)) {
            pr_err("Couldn't get TCXO_D0 voter\n");
			goto out;
		}
        
		if (IS_ERR(bs300_clock)) {
			pr_err("Couldn't get TCXO_A1 voter\n");
			goto out;
		}
     
		rc = msm_xo_mode_vote(bs300_clock_digital, MSM_XO_MODE_ON);		
		if (rc < 0) {
			pr_err("Failed to vote for TCXO_D0 pin control\n");
			goto fail_vote;
		}
        
		//rc = msm_xo_mode_vote(bs300_clock, MSM_XO_MODE_PIN_CTRL);
        rc = msm_xo_mode_vote(bs300_clock, MSM_XO_MODE_ON);		
		if (rc < 0) {
			pr_err("Failed to vote for TCXO_A1 pin control\n");
			goto fail_vote;
		}
	
	} else {
		rc = msm_xo_mode_vote(bs300_clock_digital, MSM_XO_MODE_OFF);
        rc = msm_xo_mode_vote(bs300_clock, MSM_XO_MODE_OFF);
		printk(KERN_INFO "close bs300_clk \n");

fail_vote:
		msm_xo_put(bs300_clock);
        msm_xo_put(bs300_clock_digital);
	}

out:

	return rc;
}

EXPORT_SYMBOL(bs300_clk_control);

#define WAKEUP_BS300_GPIO (144)
void set_wakeup_bs300(int flag)
{
	int rc;

	if (flag) {
		rc = gpio_request(WAKEUP_BS300_GPIO, "wakeup_bs300");
		if (rc) {
			pr_err("%s: wakeup bs300 gpio %d request"
			"failed\n", __func__, WAKEUP_BS300_GPIO);
			return;
		}
		gpio_direction_output(WAKEUP_BS300_GPIO, 1);
		gpio_set_value_cansleep(WAKEUP_BS300_GPIO, 1);
	} else {
		gpio_set_value_cansleep(WAKEUP_BS300_GPIO, 0);
		gpio_free(WAKEUP_BS300_GPIO);
	}
}
EXPORT_SYMBOL(set_wakeup_bs300);

static unsigned int 
bs300_send_receive( int sendCount, int receiveCount,
        unsigned char *pSendData, unsigned char *pReceiveData )
{
	int err;
	struct i2c_msg msg[1];
	struct i2c_client *client = bs300_i2c_client;

	
	if (!client->adapter)
	{
		BS300_ERR("[%s,%d]invalid i2c client\n",__FUNCTION__,__LINE__);
		return BS300_FAIL;
	}
	gpio_i2c_adpt_set_state(STATE_MIC_NOISE_REDUCE);
	if(sendCount != 0)
	{
		msg->addr = client->addr;
		msg->len = sendCount;
		msg->flags = 0;		/*i2c write flag*/
		msg->buf = pSendData;
		
		err = i2c_transfer(client->adapter, msg, 1);
		if(err<0)
		{
			BS300_ERR("[%s,%d]i2c write byte fail, addr = %x, err = %d\n"
						,__FUNCTION__,__LINE__,msg->addr,err);
			goto out_err;
		}
	}

	if(receiveCount != 0)
	{
		msg->addr = client->addr;
		msg->len = receiveCount;
		msg->flags = I2C_M_RD;
		msg->buf = pReceiveData;

		err = i2c_transfer(client->adapter, msg, 1);
		if(err<0)
		{
			BS300_ERR("[%s,%d]i2c read byte fail, addr = %x, err = %d\n"
						,__FUNCTION__,__LINE__,msg->addr,err);
			goto out_err;
		}
	}

	return BS300_SUCCESS;

out_err:

	return BS300_FAIL;
}

static unsigned int bs300_get_status(unsigned short *pStatusWord)
{
	unsigned char command[] = {CMD_GET_STATUS};
	unsigned char statusbytes[2] = {0};

	if(pStatusWord == NULL)
	{	
		BS300_ERR("[%s,%d] fail with null pointer\n",__FUNCTION__,__LINE__);
		return BS300_FAIL;
	}
	/*read status info*/ 
	if(bs300_send_receive(1,2, command,statusbytes))
	{						
		if( ((statusbytes[0] & 0x80) == 0x80) && ((statusbytes[1] & 0x80) == 0x00) ) 
		{			
			*pStatusWord = (unsigned short)(statusbytes[0] << 8) | statusbytes[1];
			return BS300_SUCCESS;
		}
	}
	return BS300_FAIL;
}

static unsigned int bs300_is_exist(void)
{
  unsigned short statusWord = 0;
	unsigned int i = 0;

	for(i=0;i<MAXRETRYCOUNT;i++)
	{
        printk(KERN_INFO "[%d] trying get bs300 status!\n", i);		//added for debug
		if((bs300_get_status(&statusWord) )&& 
			((statusWord & STATUS_SECURITY_MODE) == STATUS_SECURITY_UNRESTRICTED))
		{
			
			bs300EnFlag = 1;
			break;
		}
		msleep(15);	/* sleep 15 ms, do not use mdelay(15) */ 
	}
	/*max retry get_status fail*/
	if(i == MAXRETRYCOUNT)		
	{
		BS300_ERR("[%s,%d] maximum retry exceeds\n",__FUNCTION__,__LINE__);
		bs300EnFlag = 0;
		return BS300_FAIL;
		
	}
	return BS300_SUCCESS;
}


static unsigned int bs300_connect(void)
{
	unsigned char stop_command[] = {CMD_STOP_CORE};
       unsigned char Cmd_Reset_Monitor[] = {0x43 /* 'C' */};
	unsigned char Cmd_reset_lp[] = {CMD_WRITE_SPECIAL_REGISTERS/* '2' */
										, SPECIAL_REGISTER_LP/*0x0c*/, 0x00, 0x00};
	unsigned char Cmd_reset_sr[] = {CMD_WRITE_NORMAL_REGISTERS/* 'F' */
										, NORMAL_REGISTER_SR/*0x32*/, 0x00, 0x00, 0x00};
										
  if(!bs300_send_receive(1,0,stop_command,NULL))	
	{
	  BS300_ERR("[%s,%d] stop DSP failed\n",__FUNCTION__,__LINE__);
	  return BS300_FAIL;
	}

  if(!bs300_send_receive(1,0,Cmd_Reset_Monitor,NULL))	
	{
		BS300_ERR("[%s,%d] reset monitor failed\n",__FUNCTION__,__LINE__);
	  return BS300_FAIL;
	}	

	if(!bs300_send_receive(sizeof(Cmd_reset_lp), 0, Cmd_reset_lp, NULL))	
	{
	 BS300_ERR("[%s,%d] reset lp failed\n",__FUNCTION__,__LINE__);
	 return BS300_FAIL;
	}	

	if(!bs300_send_receive(sizeof(Cmd_reset_sr), 0, Cmd_reset_sr, NULL))	
	{
	 BS300_ERR("[%s,%d] reset sr failed\n",__FUNCTION__,__LINE__);
	 return BS300_FAIL;
	}	

	return BS300_SUCCESS;
	
}


static unsigned int bs300_download(void)
{
	unsigned int i = 0;
	unsigned char crcBuffer[2] = {0};
	unsigned short receivedCrc = 0;
	unsigned char crc_command[] = {CMD_READ_AND_RESET_CRC};	

	for( i=0; i < DOWNLOAD_BLOCK_COUNT; i ++ )
	{
		/*set crc to 0xFFFF before data transfer*/
		if(!bs300_send_receive(1, 0, crc_command, NULL))	
		{
			BS300_ERR("[%s,%d] send CRC CMD failed\n",__FUNCTION__,__LINE__);
			return BS300_FAIL;
		}
		/*write data transfer*/
		if(!bs300_send_receive(downloadBlocks[i].byteCount, 0,downloadBlocks[i].formattedData, NULL))	
		{
			BS300_ERR("[%s,%d] send DOWNLOAD BLOCKS failed\n",__FUNCTION__,__LINE__);
			return BS300_FAIL;
		}
	  	/*read crc after data transfer*/
		if(!bs300_send_receive(1, 2, crc_command, crcBuffer))
		{
			BS300_ERR("[%s,%d] send & read CRC CMD failed\n",__FUNCTION__,__LINE__);
			return BS300_FAIL;
		}
							
		receivedCrc = (unsigned short)((crcBuffer[0] << 8)|crcBuffer[1]);
		/*check crc*/
		if( downloadBlocks[i].crc != receivedCrc ) 
		{
			BS300_ERR("[%s,%d] crc not match i= %d, downloadBlocks[i].crc = %x, receivedCrc = %x\n"
							,__FUNCTION__,__LINE__,i,downloadBlocks[i].crc,receivedCrc);
			return BS300_FAIL;
		}	
		usleep(1);		
	}

	return BS300_SUCCESS;
}

static unsigned int bs300_run(void)
{
	unsigned char reset_lp_command[] ={CMD_WRITE_SPECIAL_REGISTERS
											, SPECIAL_REGISTER_LP, 0x00, 0x00};
	unsigned char reset_sr_command[] = {CMD_WRITE_NORMAL_REGISTERS
											, NORMAL_REGISTER_SR, 0x00, 0x00, 0x00};
	unsigned char set_pc_command[] = {CMD_EXECUTE_INSTRUCTION
											, 0x3B, 0x20, 0x10, 0x00 /* start at 0x1000 */};
	unsigned char go_command[] = {CMD_START_CORE};

	/*write special register(LP)*/
	if(!bs300_send_receive(sizeof(reset_lp_command), 0, reset_lp_command, NULL))	
	{
		BS300_ERR("[%s,%d] reset lp CMD failed\n",__FUNCTION__,__LINE__);
		return BS300_FAIL;
	}
	/*write normal register(SR)*/
	if(!bs300_send_receive(sizeof(reset_sr_command), 0, reset_sr_command, NULL))	
	{
		BS300_ERR("[%s,%d] reset sr CMD failed\n",__FUNCTION__,__LINE__);
		return BS300_FAIL;
	}
	/*write execute*/
	if( !bs300_send_receive( sizeof(set_pc_command), 0, set_pc_command, NULL ) )  
	{
		BS300_ERR("[%s,%d] set pc CMD failed\n",__FUNCTION__,__LINE__);
		return BS300_FAIL;
	}
	/*start DSP core*/
	if( !bs300_send_receive( sizeof(go_command), 0, go_command, NULL ) )		
	{
		BS300_ERR("[%s,%d] go CMD failed\n",__FUNCTION__,__LINE__);
		return BS300_FAIL;
	}	

	return BS300_SUCCESS;
}

/**
 * I2C kernel driver module
 */


static ssize_t bs300_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {

//#ifndef CONFIG_WITH_ECHO_CANCELLATION
	//	bs300EnFlag = 0;
//#endif

     return sprintf(buf, "%d", bs300EnFlag);
 }




static struct regulator *reg_8058_l10;
static void bs300_power_set(int power_min, int power_max)
{	
    int rc;	
    reg_8058_l10 = regulator_get(NULL, "8058_l10");	
    if(IS_ERR(reg_8058_l10)){		
        pr_err("%s regulator not found, rc = %ld\n", "8058_l10",PTR_ERR(reg_8058_l10));		
        return;	
    }	

    rc = regulator_set_voltage(reg_8058_l10, power_min, power_max);	
    if(rc)		
        goto out_bs300_reg;	

    rc = regulator_enable(reg_8058_l10);	
    if(rc)		
        goto out_bs300_reg;	

    printk("set power success : %d\n", power_min);	
    if(regulator_is_enabled(reg_8058_l10))		
        regulator_disable(reg_8058_l10);
    
    regulator_put(reg_8058_l10);	
    return;
    
out_bs300_reg:	
    regulator_put(reg_8058_l10);	
    reg_8058_l10 = NULL;	
}

static int bs300_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int retval = 0;
	unsigned int uiRet=0;

    unsigned int i = 0;

	/* open clk of bs300 */
	retval = bs300_clk_control(1);
	if(retval < 0) {
		dev_err(&client->dev, "bs300 xo_out_a1 control fail! \n");
		goto bs300_failed_1;
	}
	msleep(15);
	/*wakeup bs300*/
	set_wakeup_bs300(1);
	mdelay(1);
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		BS300_ERR("[%s,%d]: need I2C_FUNC_I2C\n",__FUNCTION__,__LINE__);
		return -ENODEV;
	}

	bs300_i2c_client = client;
  
	uiRet = bs300_is_exist();
	if(!uiRet)
	{
		BS300_ERR("[%s,%d]: bs300_connect return fail\n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto bs300_failed_0;
	}
  
    for(i = 0; i<MAXRETRYCOUNT; i++)
    {
    uiRet = bs300_connect();
	  if(uiRet)
	  {
		  uiRet = bs300_download();
		  if(uiRet)
		  {
		    uiRet = bs300_run();  
		    if(uiRet)
		    {
    		    break;
		    }
		  }
	  } 
	  msleep(15);
	}  
	if(i == MAXRETRYCOUNT)	
	{	  
		BS300_ERR("[%s,%d]: bs300_connect return fail\n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto bs300_failed_0;
	} 

	
	BS300_DBG("[%s,%d]: bs300 Connect, Download, Run SUCCESS\n",__FUNCTION__,__LINE__);

	retval = 0;

bs300_failed_0:	
	set_wakeup_bs300(0);
	msleep(15);					
	bs300_clk_control(0);		

bs300_failed_1:

    bs300_power_set(2600000, 2600000);
	return retval;
}

static int bs300_remove(struct i2c_client *client)
{
	bs300_i2c_client = NULL;
	bs300_clk_control(0);
	return 0;
}


static const struct i2c_device_id bs300_id[] = {
	{"bs300-audio",0},
	{},
};

static struct i2c_driver bs300_driver = {
	.driver = {
		.name = "bs300-audio",
	},
	.probe = bs300_probe,
	.remove = bs300_remove,
	.id_table = bs300_id,
};



 static struct kobj_attribute bs300_attribute =
         __ATTR(switch, 0664, bs300_attr_show, NULL);

 static struct attribute* bs300_attributes[] =
 {
         &bs300_attribute.attr,
         NULL
 };

 static struct attribute_group bs300_defattr_group =
 {
         .attrs = bs300_attributes,
 };

static int __init bs300_init(void)
{
	int ret;
	struct kobject *kobj = NULL;

    bs300_power_set(1800000, 1800000);
    
	ret = i2c_add_driver(&bs300_driver);
	if (ret)
		BS300_ERR("Unable to register BS300 driver\n");

	
	kobj = kobject_create_and_add("bs300_en", NULL);
    if (kobj == NULL) 
    {
        return ret;
    }
    if (sysfs_create_group(kobj, &bs300_defattr_group)) 
    {
        kobject_put(kobj);
        return ret;
    }
	

	return ret;
}



late_initcall(bs300_init);

static void __exit bs300_exit(void)
{
	i2c_del_driver(&bs300_driver);
}
module_exit(bs300_exit);

MODULE_AUTHOR("S7 Tech. Co., Ltd. Qiu Hanning(q00170317)");
MODULE_DESCRIPTION("Belasigna BS300 (MIC noise reducer) driver");
MODULE_LICENSE("GPL");
