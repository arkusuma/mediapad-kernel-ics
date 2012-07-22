/* drivers/i2c/busses/gpio-i2c-adpt.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <linux/remote_spinlock.h>
#include <linux/pm_qos_params.h>
#include <mach/gpio-i2c-adpt.h>
#include <linux/slab.h>

#define I2C_ADPT_DELAY  1

static enum s7_gpio_i2c_xfer_states g_S7GpioI2cXferSt = STATE_MIC_NOISE_REDUCE;

static enum s7_gpio_i2c_xfer_states gpio_i2c_adpt_get_state(void)
{
	return g_S7GpioI2cXferSt;
}

void gpio_i2c_adpt_set_state(enum s7_gpio_i2c_xfer_states state)
{
	g_S7GpioI2cXferSt = state;
}
EXPORT_SYMBOL(gpio_i2c_adpt_set_state);

static void 
bs300_gpio_i2c_start(struct gpio_i2c_s7_dev *dev)
{
	int dely_usec = dev->pdata->dely_usec;
	unsigned int gpio_i2c_dat = dev->pdata->gpio_i2c_dat;
	unsigned int gpio_i2c_clk = dev->pdata->gpio_i2c_clk;	

	gpio_direction_output(gpio_i2c_dat,1);
	gpio_direction_output(gpio_i2c_clk,1);
	udelay(dely_usec);

	gpio_set_value(gpio_i2c_dat,0);
	udelay(dely_usec);
	gpio_set_value(gpio_i2c_clk,0);
	udelay(dely_usec);
}

static void 
bs300_gpio_i2c_stop(struct gpio_i2c_s7_dev *dev)
{
	int dely_usec = dev->pdata->dely_usec;
	unsigned int gpio_i2c_dat = dev->pdata->gpio_i2c_dat;
	unsigned int gpio_i2c_clk = dev->pdata->gpio_i2c_clk;

	gpio_direction_output(gpio_i2c_dat,0);
	udelay(dely_usec);
	gpio_direction_output(gpio_i2c_clk,1);
	udelay(dely_usec);
	gpio_set_value(gpio_i2c_dat,1);
	udelay(dely_usec);	
}

static unsigned int 
bs300_gpio_i2c_byte_write(struct gpio_i2c_s7_dev	*dev, unsigned char data)
{
	unsigned char i = 0;
	unsigned int retAck = 0;
	int dely_usec = dev->pdata->dely_usec;
	unsigned int gpio_i2c_dat = dev->pdata->gpio_i2c_dat;
	unsigned int gpio_i2c_clk = dev->pdata->gpio_i2c_clk;

	for(i=0; i<8; i++)
	{
		if((0x80 >> i) & data)
		{
			gpio_set_value(gpio_i2c_dat,1);
		}
		else
		{
			gpio_set_value(gpio_i2c_dat,0);
		}
		udelay(dely_usec);
		gpio_set_value(gpio_i2c_clk,1);
		udelay(dely_usec);
		gpio_set_value(gpio_i2c_clk,0);
		udelay(dely_usec);
	}

	udelay(dely_usec);
	gpio_set_value(gpio_i2c_clk,1);
	gpio_direction_input(gpio_i2c_dat);				/*set GPIO input, get ACK*/ 
	udelay(dely_usec);

	i=0;
	retAck=gpio_get_value(gpio_i2c_dat);
//	udelay(dely_usec);
	while(retAck)
	{
		udelay(dely_usec/4); 
		i++;
		if(i>20)	
			break;
		retAck=gpio_get_value(gpio_i2c_dat);
	}

	gpio_set_value(gpio_i2c_clk,0);
	udelay(I2C_ADPT_DELAY);

	gpio_direction_output(gpio_i2c_dat,1);			/*set GPIO output, release SDA*/
	udelay(dely_usec);
	retAck = ((retAck == 0) ? S7_GPIO_I2C_SUCCESS:S7_GPIO_I2C_FAIL);

	return retAck;
}

static unsigned char 
bs300_gpio_i2c_byte_read(struct gpio_i2c_s7_dev *dev, unsigned char ack)
{
	unsigned char i = 0;
	unsigned char retVal = 0x00;
	int dely_usec = dev->pdata->dely_usec;
	unsigned int gpio_i2c_dat = dev->pdata->gpio_i2c_dat;
	unsigned int gpio_i2c_clk = dev->pdata->gpio_i2c_clk;

	gpio_direction_input(gpio_i2c_dat);				/*set GPIO input, get 8-bit DATA*/ 
//	udelay(dely_usec);

	for(i=0; i<8; i++)
	{
    	udelay(dely_usec);
		gpio_set_value(gpio_i2c_clk,1);
		udelay(dely_usec);

		retVal <<= 1;
		if(gpio_get_value(gpio_i2c_dat))
		{
			retVal  |= 0x01;
		}

		gpio_set_value(gpio_i2c_clk,0);
		udelay(dely_usec);
	}

	if(ack)
	{
		gpio_direction_output(gpio_i2c_dat,0);		/*set GPIO output, send ACK*/
	}
	else
	{
		gpio_direction_output(gpio_i2c_dat,1);		/*set GPIO output, send NACK*/
	}
	udelay(dely_usec);

	gpio_set_value(gpio_i2c_clk,1);
	udelay(dely_usec);

	gpio_set_value(gpio_i2c_clk,0);
	udelay(I2C_ADPT_DELAY);

	gpio_set_value(gpio_i2c_dat,1);				/*release SDA*/
	udelay(dely_usec);

	return retVal;
}

static int
gpio_i2c_adpt_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	unsigned char  addr;
	unsigned int i=0;
	int rem = num;
	unsigned int retVal=0;
	struct gpio_i2c_s7_dev	*dev = i2c_get_adapdata(adap);
	enum s7_gpio_i2c_xfer_states gpio_i2c_op_st;
	
	addr = msgs->addr << 1;

	mutex_lock(&dev->mlock);
	gpio_i2c_op_st = gpio_i2c_adpt_get_state();

	while(rem)
	{
		if (msgs->flags & I2C_M_RD)
		{
			addr |=1;
			
			switch(gpio_i2c_op_st)
			{
				case STATE_MIC_NOISE_REDUCE:
					bs300_gpio_i2c_start(dev);

					retVal = bs300_gpio_i2c_byte_write(dev,addr);
					if(!retVal)						/*write read address fail*/
					{
						dev_err(dev->dev,"[%s,%d]i2c write byte fail, addr = %x, err = %d\n"
									,__FUNCTION__,__LINE__,addr,retVal);
						goto out_err;
					}
					
					for(i=0;i<msgs->len;i++)
					{
						if(i == msgs->len-1) 				/*read last byte send NACK*/
						{
							msgs->buf[i] = bs300_gpio_i2c_byte_read(dev,S7_GPIO_I2C_NACK);
							dev_dbg(dev->dev,"[%s,%d] i2c read return NACK\n",__FUNCTION__,__LINE__);
						}
						else							/*else send ACK*/
						{
							msgs->buf[i] = bs300_gpio_i2c_byte_read(dev,S7_GPIO_I2C_ACK);
							dev_dbg(dev->dev,"[%s,%d] i2c read return ACK\n",__FUNCTION__,__LINE__);
						}
					}

					bs300_gpio_i2c_stop(dev);
					break;
				case STATE_HDMI:
					break;
				case STATE_CAMERA:
					break;
				default:
					break;
			}
		}
		else
		{
			switch(gpio_i2c_op_st)
			{
				case STATE_MIC_NOISE_REDUCE:
					bs300_gpio_i2c_start(dev);

					retVal = bs300_gpio_i2c_byte_write(dev,addr);
					if(!retVal)						/*write write address fail*/
					{
						dev_err(dev->dev,"[%s,%d]i2c write byte fail, addr = %x, err = %d\n"
									,__FUNCTION__,__LINE__,addr,retVal);
						goto out_err;
					}

					for(i=0;i<msgs->len;i++)
					{
						retVal = bs300_gpio_i2c_byte_write(dev,msgs->buf[i]);
						if(!retVal)
						{
							dev_err(dev->dev,"[%s,%d]i2c write byte fail, data[%d] = %x, err = %d\n"
									,__FUNCTION__,__LINE__,i,msgs->buf[i],retVal);	
							goto out_err;
						}
					}

					bs300_gpio_i2c_stop(dev);
					break;
				case STATE_HDMI:
					break;
				case STATE_CAMERA:
					break;
				default:
					break;
			}
		}
		msgs++;
		rem--;
	}
	mutex_unlock(&dev->mlock);
	return 0;
out_err:
	switch(gpio_i2c_op_st)
	{
		case STATE_MIC_NOISE_REDUCE:
			bs300_gpio_i2c_stop(dev);
			break;
		case STATE_HDMI:
			break;
		case STATE_CAMERA:
			break;
		default:
			break;
	}
	mutex_unlock(&dev->mlock);
	return -1;
}

static u32
gpio_i2c_adpt_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm gpio_i2c_adpt_algo = {
	.master_xfer	= gpio_i2c_adpt_xfer,
	.functionality	= gpio_i2c_adpt_func,
};

static int
gpio_i2c_adpt_probe(struct platform_device *pdev)
{
	int ret;
	struct gpio_i2c_s7_dev	*dev;
	struct gpio_i2c_adpt_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	if(!pdata)
	{
		dev_err(&pdev->dev, "[%s,%d]: platform data not initialized\n",__FUNCTION__,__LINE__);
		return -ENOSYS;
	}
	if(!pdata->gpio_i2c_adpt_config_gpio)
	{
		dev_err(&pdev->dev, "[%s,%d]: config_gpio function not initialized\n",__FUNCTION__,__LINE__);
		return -ENOSYS;
	}

	/* We support frequencies from 10KHz upto FAST Mode(250KHz) */
	if(pdata->dely_usec <2 ||pdata->dely_usec >50) 
	{
		dev_err(&pdev->dev, "[%s,%d]: clock frequency not supported\n",__FUNCTION__,__LINE__);
		return  -EIO;
	}
	
	dev = kzalloc(sizeof(struct gpio_i2c_s7_dev),GFP_KERNEL);
	if (!dev) 
	{
		dev_err(&pdev->dev, "[%s,%d]: not enough memory\n",__FUNCTION__,__LINE__);
		return -ENOMEM;
	}

	dev->dev = &pdev->dev;
	dev->pdata = pdata;
	platform_set_drvdata(pdev,dev);

	i2c_set_adapdata(&dev->adap_gpio_s7,dev);
	dev->adap_gpio_s7.algo = &gpio_i2c_adpt_algo;
	strlcpy(dev->adap_gpio_s7.name,
		"S7 GPIO I2C adapter",
		sizeof(dev->adap_gpio_s7.name));

	dev->adap_gpio_s7.nr = pdev->id;
	ret = i2c_add_numbered_adapter(&dev->adap_gpio_s7);
	if(ret) 
	{
		dev_err(&pdev->dev, "[%s,%d]: Add S7 GPIO I2C adapter failed, err = %d\n",__FUNCTION__,__LINE__,ret);
		kfree(dev);
		return -EIO;
	}

	mutex_init(&dev->mlock);
	
	/*Config GPIOs*/
	pdata->gpio_i2c_adpt_config_gpio(pdata->gpio_i2c_clk,pdata->gpio_i2c_dat);

	gpio_request(pdata->gpio_i2c_clk, "gpio_i2c_clk");
	if (ret) {
		dev_err(&pdev->dev, "%s: unable to request gpio %d\n",
			__FUNCTION__,  pdata->gpio_i2c_clk);
		goto out;
	}

	gpio_request(pdata->gpio_i2c_dat, "gpio_i2c_dat");
	if (ret) {
		dev_err(&pdev->dev, "%s: unable to request gpio %d\n",
			__FUNCTION__, pdata->gpio_i2c_dat);
		goto reg_disable;
	}

	//set bus to idel
	gpio_direction_output(pdata->gpio_i2c_clk,1);
	mdelay(1);
	gpio_direction_output(pdata->gpio_i2c_dat,1);
	mdelay(1);
		

	goto out;

reg_disable:
	gpio_free(pdata->gpio_i2c_clk);

out:	
	printk(KERN_INFO "i2c_gpio return=%d \n", ret);
	return ret;
}

static int
gpio_i2c_adpt_remove(struct platform_device *pdev)
{
	struct gpio_i2c_s7_dev	*dev = platform_get_drvdata(pdev);

	struct gpio_i2c_adpt_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	if(pdata)
	{
		gpio_free(pdata->gpio_i2c_clk);
		gpio_free(pdata->gpio_i2c_dat);
	}

	mutex_destroy(&dev->mlock);
	i2c_del_adapter(&dev->adap_gpio_s7);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int bs300_gpio_i2c_suspend(struct device *dev)
{
    struct gpio_i2c_s7_dev	*pdev  = dev_get_drvdata(dev);
    struct gpio_i2c_adpt_platform_data *pdata = pdev->dev->platform_data;

    gpio_free(pdata->gpio_i2c_clk);
    gpio_free(pdata->gpio_i2c_dat);
	return 0;
}

static int bs300_gpio_i2c_resume(struct device *dev)
{
    int rc;
    struct gpio_i2c_s7_dev	*pdev  = dev_get_drvdata(dev);
    struct gpio_i2c_adpt_platform_data *pdata = pdev->dev->platform_data;
    
    rc = gpio_request(pdata->gpio_i2c_clk, "gpio_i2c_clk");
	if (rc) {
		pr_err("%s: gpio_request failed for HEADSET_DETECT_GPIO\n",
								__func__);
	}

	rc = gpio_request(pdata->gpio_i2c_dat, "gpio_i2c_dat");
	if (rc) {
		pr_err("%s: gpio_request failed for MIC_CTRL_GPIO\n",
							__func__);
	}

	return 0;
}

static struct dev_pm_ops bs300_gpio_i2c_pm_ops = {
	.suspend = bs300_gpio_i2c_suspend,
	.resume = bs300_gpio_i2c_resume,
};
#endif

static struct platform_driver gpio_i2c_adpt_driver = {
	.probe = gpio_i2c_adpt_probe,
	.remove = gpio_i2c_adpt_remove,
	.driver		= {
		.name	= "adpt_gpio_i2c",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &bs300_gpio_i2c_pm_ops,
#endif
	},	
};

static int __init
gpio_i2c_adpt_init_driver(void)
{
	return platform_driver_register(&gpio_i2c_adpt_driver);
}
subsys_initcall(gpio_i2c_adpt_init_driver);

static void __exit 
gpio_i2c_adpt_exit_driver(void)
{
	platform_driver_unregister(&gpio_i2c_adpt_driver);
}
module_exit(gpio_i2c_adpt_exit_driver);
