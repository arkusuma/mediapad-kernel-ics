/*
 * drivers/input/touchscreen/mxt422.c
 *
 * Copyright (c) 2011 Huawei Device Co., Ltd.
 *	Li Yaobing <liyaobing@S7.com>
 *
 * Using code from:
 *  - ads7846.c
 *	Copyright (c) 2005 David Brownell
 *	Copyright (c) 2006 Nokia Corporation
 *  - corgi_ts.c
 *	Copyright (C) 2004-2005 Richard Purdie
 *  - omap_ts.[hc], ads7846.h, ts_osk.c
 *	Copyright (C) 2002 MontaVista Software
 *	Copyright (C) 2004 Texas Instruments
 *	Copyright (C) 2005 Dirk Behme
 *  - mxt224.c
 *	Copyright (c) 2010 Huang Zhikui
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/module.h>  
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/mxt224.h>
#include <linux/jiffies.h>
#include <mach/mpp.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h> 
#include <linux/vmalloc.h>


#if 0
#define DBG(fmt, args...) printk(KERN_INFO "[%s,%d] "fmt"\n", __FUNCTION__, __LINE__, ##args)
#else
#define DBG(fmt, args...) do {} while (0)
#endif

#define TS_POLL_DELAY		(10 * 1000)	/* ns delay before the first sample */
#define TS_PENUP_TIMEOUT_MS 20
#define	MAX_8BIT			((1 << 8) - 1)
#define USE_WQ

#define TOUCH_MULTITOUCHSCREEN_T9                 9u
#define USERDATA_T38                                 38u
#define VERSION_OFFSET                              0u
#define TOUCHSCREEN_OFFSET                     1u

#define	MXT_MSG_T9_STATUS				0x01
/* Status bit field */
#define		MXT_MSGB_T9_SUPPRESS		0x02
#define		MXT_MSGB_T9_AMP			0x04
#define		MXT_MSGB_T9_VECTOR		0x08
#define		MXT_MSGB_T9_MOVE		0x10
#define		MXT_MSGB_T9_RELEASE		0x20
#define		MXT_MSGB_T9_PRESS		0x40
#define		MXT_MSGB_T9_DETECT		0x80
    
#define	MXT_MSG_T9_XPOSMSB				0x02
#define	MXT_MSG_T9_YPOSMSB				0x03
#define	MXT_MSG_T9_XYPOSLSB				0x04
#define	MXT_MSG_T9_TCHAREA				0x05
#define	MXT_MSG_T9_TCHAMPLITUDE				0x06
#define	MXT_MSG_T9_TCHVECTOR				0x07

#ifdef USE_WQ
#include <linux/workqueue.h>
static void mxt224_timer(struct work_struct *work);
DECLARE_WORK(mxt224_work, mxt224_timer);
#endif

static struct mxt224 *mxt224_tsc;

struct orig_t {
    int x;
    int y;
};
 
/* upper left point */
static struct orig_t tsp_ul_50x50 = {
    50, 
    50
};

/* lower right point */
static struct orig_t tsp_dr_50x50 = {
    1230,
    750
};

static struct orig_t lcd_size = {
    1280, 
    800
};

static struct orig_t tsp_ul_50x50_convert, tsp_dr_50x50_convert;
static struct orig_t tsp_origin_convert;
/*begin: add by liyaobing l00169718 20110413 for no respond problem when system boot up*/
static void mxt224_resume(struct i2c_client *client);
/*end: add by liyaobing l00169718 20110413 for no respond problem when system boot up*/
static irqreturn_t mxt224_irq(int irq, void *handle);
static int mxt224_read_values(struct mxt224 *tsc);
#define GPIO_3V3_EN             (130)  
#define GPIO_CTP_INT   			(140)
#define GPIO_CTP_RESET       	(139)
extern int msm_gpiomux_put(unsigned gpio);
extern int msm_gpiomux_get(unsigned gpio);
static int is_upgrade_firmware = 0 ;
static void poweron_touchscreen(void);
#ifdef CONFIG_UPDATE_MXT224_FIRMWARE  
#define READ_MEM_OK                 1u
#define READ_MEM_FAILED             2u
#define WRITE_MEM_OK                1u
#define WRITE_MEM_FAILED            2u
#define OBJECT_NOT_FOUND   0u

#define FIMRWARE_VERSION 0X01
#define FIMRWARE_TYPE	 "ATMEL_CMI"

static struct i2c_client *g_client = NULL;

static int ts_firmware_file(void);
static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(update_firmware);
static ssize_t firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t firmware_version_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(firmware_version);
static ssize_t firmware_file_version_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t firmware_file_version_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(firmware_file_version);
static ssize_t firmware_tptype_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t firmware_tptype_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(firmware_tptype);

static ssize_t tp_control_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t tp_control_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(tp_control);

struct delayed_work cal_work;
int is_suspend = 0;
static void calibrate_chip_resume(struct work_struct *work);

#ifdef CONFIG_DEBUG_MXT224_FIRMWARE 
#define FLAG_RR 		0x00
#define FLAG_WR 		0x01
#define FLAG_RPX 		0x02
#define FLAG_RPY 		0x03
#define FLAG_RNIT 		0x04
#define FLAG_HRST 		0x05
#define FLAG_SRST 		0x06
#define FLAG_EINT 		0x07
#define FLAG_DINT 		0x08
#define FLAG_RTA 		0x09
#define FLAG_PRST 		0x0a 
#define FLAG_POFF 		0x0b 
#define FLAG_EFW 		0x0c 
#define FLAG_BR 		0x0d 
#define FLAG_ROA 		0x0e 
#define FLAG_ROS 		0x0f
#define FLAG_RRP       0x10
static void read_delta_data(void);
static ssize_t ts_debug_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t ts_debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(ts_debug);
#endif
static int ts_firmware_file(void)
 {
     int ret;
     struct kobject *kobject_ts;
	 
     kobject_ts = kobject_create_and_add("touchscreen", firmware_kobj);
     if (!kobject_ts) {
         printk("create kobjetct error!\n");
         return -1;
     }
	 
     ret = sysfs_create_file(kobject_ts, &update_firmware_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create file error\n");
         return -1;
     }
     ret = sysfs_create_file(kobject_ts, &firmware_version_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create file error\n");
         return -1;
     }
 	ret = sysfs_create_file(kobject_ts, &firmware_file_version_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create firmware file version error\n");
         return -1;
     }
     ret = sysfs_create_file(kobject_ts, &tp_control_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create file error\n");
         return -1;
     }
     
     ret = sysfs_create_file(kobject_ts, &firmware_tptype_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create tptype file error\n");
         return -1;
     }
#ifdef CONFIG_DEBUG_MXT224_FIRMWARE 
     ret = sysfs_create_file(kobject_ts, &ts_debug_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create debug_ts file error\n");
         return -1;
     }	  
#endif
     return 0;   
}

static ssize_t write_register(uint8_t object_type,uint8_t offset,uint8_t *data)
 {
	uint16_t object_address;
     	uint8_t object_size;
	uint8_t ret ;
	
	object_address = get_object_address(object_type, 0) ;
	object_size = get_object_size(object_type);
      if ((object_address == 0) ||(object_size == 0) || ( offset > object_size))
        	return  0;
	ret = write_mem(object_address+offset, 1, data);
	if(WRITE_MEM_FAILED == ret)
		return  0;
	 else 
		return  1;
 }

 static ssize_t read_register(uint8_t object_type,uint8_t offset)
 {
	uint16_t object_address;
     	uint8_t object_size;
	uint8_t data ;
	uint8_t ret ;

	object_address = get_object_address(object_type, 0) ;
	object_size = get_object_size(object_type);
      if ((object_address == 0) ||(object_size == 0) || ( offset > object_size))
        	return  0;
	ret = read_mem(object_address+offset, 1, &data);
	if(READ_MEM_FAILED == ret)
		return  0;
	 else 
		return  data;
 }

static void poweron_touchscreen(void){
    uint8_t data ;
    struct mxt224_platform_data * ts ;
     ts = (struct mxt224_platform_data *)g_client->dev.platform_data; 
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
      if ((get_object_address(USERDATA_T38, 0) == 0) ||(get_object_size(USERDATA_T38) == 0) 
	  	||(READ_MEM_FAILED == read_mem(get_object_address(USERDATA_T38, 0) , 1, &data))){
        printk("touchscreen has poweroff and wil poweron for do some operation\n");
        msm_gpiomux_get(GPIO_3V3_EN);
        msm_gpiomux_get(GPIO_CTP_INT);   
        msm_gpiomux_get(GPIO_CTP_RESET);  
        ts->chip_poweron_reset() ; 
    }
    /*for update_firmware ,it should delay 500ms for touchscreen poweron reset complete*/
    if(1 == is_upgrade_firmware){
		msleep(500) ;
    }
}
 #define is_valid_data_space(c)				(c == ' ' || c == '\t')
 #define is_valid_space(c)						(is_valid_data_space(c) || c == 10 || c == 13 )
 #define is_valid_alphanumeric(c)				((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
 #define is_valid_annotation(c)				(c == '#')
 #define is_valid_data(c)						(is_valid_space(c) || is_valid_alphanumeric(c) || is_valid_annotation(c))
 #define char_to_numeric(c)					((c >= '0' && c <= '9') ? (c-'0'):((c >= 'a' && c <= 'f') ? (c-'a'+10):((c >= 'A' && c <= 'F') ? (c-'A'+10) : -1)))
 unsigned char config_data[MAX_OBJECT_NUM][MAX_OBJECT_CONFIGDATA_NUM];
 static int i2c_update_firmware(const char *file, size_t count) 
 {
     unsigned char path_image[255];
     char *buf;
     struct file *filp;
     struct inode *inode = NULL;
     mm_segment_t oldfs;
     uint16_t    length,i,j,k,line_num,data_num=0;	 
   
     if(count >255 || count == 0 || strnchr(file, count, 0x20))
         return -1;     
     
     memcpy (path_image, file,  count);
     /* replace '\n' with  '\0'  */ 
     if((path_image[count-1]) == '\n')
     	path_image[count-1] = '\0'; 
     else
 		path_image[count] = '\0';
	 
     /* open file */
     oldfs = get_fs();
     set_fs(KERNEL_DS);
     filp = filp_open(path_image, O_RDONLY, S_IRUSR);
     if (IS_ERR(filp)) {
         printk("%s: file %s filp_open error\n", __FUNCTION__,path_image);
         set_fs(oldfs);
         return -1;
     }
 
     if (!filp->f_op) {
         printk("%s: File Operation Method Error\n", __FUNCTION__);
         filp_close(filp, NULL);
         set_fs(oldfs);
         return -1;
     }
 
     inode = filp->f_path.dentry->d_inode;
     if (!inode) {
         printk("%s: Get inode from filp failed\n", __FUNCTION__);
         filp_close(filp, NULL);
         set_fs(oldfs);
         return -1;
     }
 
     /* file's size */
     length = i_size_read(inode->i_mapping->host);
     printk("mxt224 firmware image file is %s and data size is %d Bytes \n",path_image,length);
     if (!( length > 0 && length < 62*1024 )){
         printk("file size error\n");
         filp_close(filp, NULL);
         set_fs(oldfs);
         return -1;
     }
 
     /* allocation buff size */
     buf = vmalloc(length+(length%2));       /* buf size if even */
     if (!buf) {
         printk("alloctation memory failed\n");
         filp_close(filp, NULL);
         set_fs(oldfs);
         return -1;
     }

 	 /* make sure the buffer memory initialized to zero */
	 memset(buf, 0, length+(length%2));
     /* read data */
     if (filp->f_op->read(filp, buf, length, &filp->f_pos) != length) {
         printk("%s: file read error\n", __FUNCTION__);
         filp_close(filp, NULL);
         set_fs(oldfs);
         vfree(buf);
         return -1;
     }

   i=0;
   j=0;
  line_num=1;
  memset(config_data,0,sizeof(config_data));
  do{
  	/*if a line is begin with annotation '#',ignore it */
	while(is_valid_annotation(buf[i])){
		do{
			i++;
		}while ((!(buf[i]==0x0d && buf[i+1] ==0x0a)) && (i<length));
		if(i<length){
			i+=2;
			line_num++ ;
		}
	}
      j=i;
      k=0;
      /*if a line is begin with non annotation '#',process it */
      while((!(buf[i]==0x0d &&buf[i+1] ==0x0a)) && (i<length)){
	  	/*a valid  space char in a line,ignore it*/
		 while((is_valid_data_space(buf[i])) && (i<length)){
			i++;
      		}
		 
	       /*valid data char in a line,process it*/		   
		 if((i>=length) || ((buf[i] == 0x0d)&&(buf[i+1] = 0x0a))){
		 	break ;
 		 }else if(is_valid_alphanumeric(buf[i]) && is_valid_alphanumeric(buf[i+1])){
		 	if(((i+1 < length-1) && is_valid_alphanumeric(buf[i]) && is_valid_alphanumeric(buf[i+1]) && (is_valid_space(buf[i+2]) ||  ((buf[i+2] == 0x0d)&&(buf[i+3] == 0x0a))))
			  || ((i+1 == length-1) && is_valid_alphanumeric(buf[i]) && is_valid_alphanumeric(buf[i+1]))){
					config_data[data_num][k++]=(char_to_numeric(buf[i])<<4) +  char_to_numeric(buf[i+1]) ;
			 	 	i+=2;
  			  }else{
			 	printk("mxt224 firmware config data [line_num=%d,colum_num=%d] format erro:buf[%d]=%x,%x,%x\n",line_num,i-j+1,i,buf[i],buf[i+1],buf[i+2]);
				filp_close(filp, NULL);
			       set_fs(oldfs);
			       vfree(buf);
			       return -1;	
			  }	      
		 }else{ /*invalid data char in a line,exit directly*/
			 	printk("mxt224 firmware config data [line_num=%d,colum_num=%d] format erro:buf[%d]=%x,%x,%x\n",line_num,i-j+1,i,buf[i],buf[i+1],buf[i+2]);
				filp_close(filp, NULL);
			       set_fs(oldfs);
			       vfree(buf);
			       return -1;	
		 }
	}
	 if(i<length){
		i+=2 ;
		line_num++;
	  }
	
	if(k>0){
		if(config_data[data_num][1] != k-2){
			printk("object[%d] config data num erro:require_num=%d but actual_num=%d!\n",config_data[data_num][0],config_data[data_num][1] ,k-2);
			filp_close(filp, NULL);
		       set_fs(oldfs);
		       vfree(buf);
		       return -1;	
		}
		data_num++;
	}
	
	}while(i<length); 

     filp_close(filp, NULL);
     set_fs(oldfs);
     vfree(buf);
     return 0;
 }
static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
     return 0 ;
 }
 static ssize_t update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
     int ret = -1;
     printk("start mx224  firmware update download\n"); 
        is_upgrade_firmware = 1 ;
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
       poweron_touchscreen();
     disable_irq(mxt224_tsc->irq);          
      ret = mxt224_update_firmware();
     enable_irq(mxt224_tsc->irq);

    msleep(100) ;
     if((WRITE_MEM_FAILED == ret ) || (1 == tp_config_err)){
         printk("Update firmware failed!\n");
	  reset_chip() ;
         ret = -1;
     } else {
         backup_config()  ;
	  ret = reset_chip() ;
	  if(WRITE_MEM_FAILED == ret ){
		printk("Update firmware failed!\n");
         	ret = -1;
	  }else{
         	printk("Update firmware success!\n");
         	//arm_pm_restart('0', (const char *)&ret);
         	ret = 1;
	  }
     } 
        is_upgrade_firmware = 0 ;
     return ret;
 }
 
static ssize_t firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
	poweron_touchscreen();
 	return sprintf(buf, "%03d", read_register(USERDATA_T38,VERSION_OFFSET)) ;
 }
 static ssize_t firmware_version_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
     return 0 ;
 }
static ssize_t firmware_file_version_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
 	unsigned int  i=0;
       for(i=0;i<MAX_OBJECT_NUM;i++){
		if(USERDATA_T38 == config_data[i][0] ){
			return sprintf(buf, "%04d", config_data[i][2]) ;
		}
	  }
 	return 1;
 }
 static ssize_t firmware_file_version_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
       int ret = -1;
	 disable_irq(mxt224_tsc->irq); 
	 ret = i2c_update_firmware(buf,count);
	if(ret != 0){
		memset(config_data,0,sizeof(config_data));
	}
	enable_irq(mxt224_tsc->irq);
	return 1 ;
 }
static ssize_t tp_control_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
    return 0;
}

static ssize_t tp_control_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    struct mxt224_platform_data * ts ;
    int on = -1;   
    ts = (struct mxt224_platform_data *)g_client->dev.platform_data; 
    on = simple_strtoul(buf, NULL, 0);
    if (on == 0 && is_upgrade_firmware == 0) {
      ts->chip_poweroff();
      return  count;
    } else if (on == 1) {      
      ts->chip_poweron_reset(); 
      return count;
    } else {
          printk(KERN_ERR "Invalid argument or firmware upgrading.");
          return -1;
    }
}
  
static ssize_t firmware_tptype_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
	uint8_t  touchscreen_type = 0;

    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
	poweron_touchscreen();
	touchscreen_type = read_register(USERDATA_T38,TOUCHSCREEN_OFFSET) ;
	if(0x00 == touchscreen_type)
		return sprintf(buf, "%s", FIMRWARE_TYPE) ;
	else
		return sprintf(buf, "%s", FIMRWARE_TYPE) ;
}
static ssize_t firmware_tptype_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
return 0;
}
 
#ifdef CONFIG_DEBUG_MXT224_FIRMWARE 
static ssize_t ts_debug_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	return 0 ;
}

uint8_t delta_buf[4][128] = {{0},{0},{0},{0}};
static uint8_t get_one_byte(int i, int j)
{
	return delta_buf[i][j];
}
static ssize_t ts_debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
     int ret = 0 ;
     struct mxt224_platform_data * ts ;
	 	
     ts = (struct mxt224_platform_data *)g_client->dev.platform_data; 
	  
     if(buf[0] == FLAG_RR){
		return read_register(buf[1],buf[2]) ;
     }else if(buf[0] == FLAG_WR){
              return write_register(buf[1] ,buf[2] ,(uint8_t*)(buf+3));
     }else if(buf[0] == FLAG_RPX){
		return ts_debug_X ;
     }else if(buf[0] == FLAG_RPY){
		return ts_debug_Y ;
     }else if(buf[0] == FLAG_RNIT){
		return ts->interrupts_pin_status() ;
     }else if(buf[0] == FLAG_HRST){
		return ts->chip_reset() ;
     }else  if(buf[0] == FLAG_SRST){
	     ret = reset_chip();
	    if(WRITE_MEM_FAILED == ret)
			return 0;
	    else 
			return 1;
     }else if(buf[0] == FLAG_EINT){
	    /*if not interrupts modem ,return fail*/
	   #if !defined(USE_WQ)
	        return 0 ;
	    #endif
            /*register msm8660 interrupts*/
	     ret = request_irq(mxt224_tsc->irq, mxt224_irq, IRQF_TRIGGER_FALLING, /* IRQF_TRIGGER_LOW IRQF_TRIGGER_HIGH */
            g_client->dev.driver->name, mxt224_tsc);
		if (ret < 0) {
			printk(KERN_ERR "Failed to request IRQ!ret = %d\n", ret); 
		  return 0 ;
		}
	
	    /* enable mxt224 interrupte when being sleep */
	    config_enable_mxt244();
	    /* clear msg when being resume */
	    mxt224_read_values(mxt224_tsc);
	    return 1 ;
     }else if(buf[0] == FLAG_DINT){
	    /*if not interrupts modem ,return fail*/
	   #if !defined(USE_WQ)
	        return 0 ;
	    #endif
            /*register msm8660 interrupts*/
	    free_irq(mxt224_tsc->irq, mxt224_tsc);
	    /* clear msg when being sleep */
	    mxt224_read_values(mxt224_tsc);
	    /* disable mxt224 interrupte when being sleep */
	    config_disable_mxt244();
	    return 1 ;
     }else  if(buf[0] == FLAG_PRST){
		return ts->chip_poweron_reset() ; 
    }else  if(buf[0] == FLAG_POFF){
		return ts->chip_poweroff() ; 
    }else  if(buf[0] == FLAG_BR){
	     ret = backup_config();
	    if(WRITE_MEM_FAILED == ret)
			return 0;
	    else 
			return 1;
    }else  if(buf[0] == FLAG_ROA){
	    if(OBJECT_NOT_FOUND == get_object_address(buf[1], 0))
			return 0;
	    else 
			return 1;
    }else  if(buf[0] == FLAG_ROS){
	    return get_object_size(buf[1]);
    }else if(buf[0] == FLAG_RRP){
    	if(buf[3] == 1){
	    	read_delta_data();
			return 1;
    	}else{
    		return get_one_byte(buf[1], buf[2]);
    	}
	}else {
		return 0;
    }
}
 #endif
#endif
static void mxt224_penup_timeout(unsigned long arg)
{
 	return ;
}

static inline void restart_scan(struct mxt224 *ts, unsigned long delay)
{
#if defined(USE_WQ)
    schedule_work(&mxt224_work);
#elif defined(USE_NORMAL_TIMER)
    mod_timer(&ts->timer,
        jiffies + usecs_to_jiffies(500000));  // delay / 1000
#else
    hrtimer_start(&ts->hr_timer, ktime_set(0, delay),
                HRTIMER_MODE_REL);
#endif
}

struct finger {
	int x;
	int y;
    int z;    
	int size; 
    int pressure; 
};
static struct finger fingers[MAX_FINGERS] = {};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxt224_early_suspend(struct early_suspend *h);
static void mxt224_late_resume(struct early_suspend *h);
#endif

void mxt224_update_pen_state(void *tsc)
{
	struct mxt224 *ts = tsc;

	u8  status;
	u16 xpos = 0xFFFF;
	u16 ypos = 0xFFFF;
	u8  touch_size = 255;
	u8  touch_number;
    static int nPrevID= -1;
	int bChangeUpDn= 0;
    u8  i;
	uint8_t  instance;

	if((1 == tp_is_calibrating) || (1 == tp_is_facesuppression) ) {
		tp_is_facesuppression = 0 ;
		memset(fingers,0,sizeof(fingers));
		input_mt_sync(ts->input);
		input_sync(ts->input);
		return ;
	}
    
	
	DBG( " ----------------process_T9_message ------------ \n");
	if(TOUCH_MULTITOUCHSCREEN_T9 != report_id_to_type(ts->tc.touch_number_id,&instance))
	 	return ;
	
	status = ts->tc.tchstatus;
    
	if (status & MXT_MSGB_T9_SUPPRESS) {
		/* Touch has been suppressed by grip/face */
		/* detection                              */
		input_mt_sync(ts->input);
		input_sync(ts->input);
	} else { 	
		xpos = ts->tc.x;
	    ypos = ts->tc.y;
        touch_number = ts->tc.touch_number_id-min_multitouch_report_id;

		if (status & MXT_MSGB_T9_DETECT) {
			/*
			 * TODO: more precise touch size calculation?
			 * mXT224 reports the number of touched nodes,
			 * so the exact value for touch ellipse major
			 * axis length would be 2*sqrt(touch_size/pi)
			 * (assuming round touch shape).
			 */
			DBG( " ----process_T9_message --MXT_MSGB_T9_DETECT--- \n");
			touch_size = ts->tc.tcharea;
			touch_size = touch_size >> 1;
			if (!touch_size)
				touch_size = 1;
            
	            if (status & MXT_MSGB_T9_PRESS)
	            	bChangeUpDn= 1;
            
	            if ((status & MXT_MSGB_T9_PRESS)
				    || (status & MXT_MSGB_T9_MOVE)
				    || (status & MXT_MSGB_T9_AMP)){	
		            fingers[touch_number].x = xpos;
					fingers[touch_number].y = ypos;
					fingers[touch_number].size = touch_size;
		            fingers[touch_number].pressure= ts->tc.tchamp;    
			}
	   } else if (status & MXT_MSGB_T9_RELEASE) {
			/* The previously reported touch has been removed.*/
              DBG( " ----process_T9_message --MXT_MSGB_T9_RELEASE--- \n");
              bChangeUpDn= 1;
              fingers[touch_number].pressure= 0;
#ifdef CONFIG_DEBUG_MXT224_FIRMWARE 
		ts_debug_X =0 ;
		ts_debug_Y = 0 ;
#endif
		}
        
		//report_sync(ts);
		if( nPrevID >= touch_number || bChangeUpDn )
	    {
			for (i = 0; i < MAX_FINGERS; i++) {
				
                if(fingers[i].pressure == -1)
                {
                  	continue;
                }

				if(fingers[i].pressure == 0)
                {
					input_mt_sync(ts->input);
                    fingers[i].pressure= -1;
					continue;
                }
				
				DBG("report_sync touch_number=%d, x=%d, y=%d, width=%d\n",
						i, fingers[i].x, fingers[i].y, fingers[i].size);
				input_report_abs(ts->input, ABS_MT_POSITION_X, fingers[i].x);
				input_report_abs(ts->input, ABS_MT_POSITION_Y, fingers[i].y);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, fingers[i].pressure);
				input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR,  (touch_number <<8)|fingers[i].size);
				input_mt_sync(ts->input);
			}

			input_sync(ts->input);
			DBG("report_sync() OUT\n");
	    }
    	nPrevID= touch_number;
	}
        
	return;
}

static int mxt224_read_values(struct mxt224 *tsc)
{
    mxt224_get_message(tsc);
	return 0;
}

typedef struct
{
    uint8_t mode;
    uint8_t page;
    uint8_t data[128];
} debug_diagnositc_t37_delta_t;
extern unsigned char read_diagnostic_delta(debug_diagnositc_t37_delta_t * dbg, unsigned char page);
static void read_delta_data(void)
{
	int j = 0;
#ifdef DEBUG 
	int i = 0;
	int n = 0;
	short tmp;
#endif
    debug_diagnositc_t37_delta_t dbg;
    dbg.mode = 0x10;
    dbg.page = 0;
	memset(dbg.data, 0, sizeof(dbg.data));        

	for (j = 0; j < 4; j++) {
	    read_diagnostic_delta(&dbg, j);
#ifdef DEBUG 
		for (i = 0; i < 128; i += 2, n+=2) {         
	        if (((n % 22) == 0)) {
	            printk("\n");
	        }            
	       tmp = (short) ((dbg.data[i+1] << 8) | (dbg.data[i])); 
		   printk("%05d ", tmp);		   
	    }
#endif
		memset(delta_buf[j], 0, sizeof(dbg.data));
		memcpy(delta_buf[j], dbg.data, sizeof(dbg.data));
    }
}
#if 0
#define MIN_ANTI_TOUCH    (-350)
#define MAX_TOUHCH		  (350)
#define ATCHFRCCALTHR_CAL (40)
#define ATCHFRCCARATIO_CAL (24) // ATCHFRCCALTHR_CAL * 0.6
#define ANTI_TOUCH_NUM		(0)
#define TOUCH_NUM			(0)
//#define DEBUG
int cal_succsess = 0;

static void calibrate_chip_resume(struct work_struct *work)
{
    static int i = 0;
	short tmp;
	int j = 0;
    int n = 0;
    int min = 0;
    int max = 0;
    int touch_channels = 0;
    int antitouch_channels = 0;
    int total = 0;

    debug_diagnositc_t37_delta_t dbg;
    dbg.mode = 0x10;
    dbg.page = 0;
	memset(dbg.data, 0, sizeof(dbg.data));        

	for (j = 0; j < 4; j++) {
	    read_diagnostic_delta(&dbg, j);
		for (i = 0; i < 128; i += 2, n+=2) {
#ifdef DEBUG           
	        if (((n % 22) == 0)) {
	            printk("\n");
	        }    
#endif	        
	       tmp = (short) ((dbg.data[i+1] << 8) | (dbg.data[i]));
#ifdef DEBUG
		   printk("%05d ", tmp);
#endif		   
	       if (tmp < min)  {
				min = tmp;
           } else if (tmp > max) {
				max = tmp;
           }

		   if (tmp < MIN_ANTI_TOUCH) {
               antitouch_channels++;
           } else if (tmp > MAX_TOUHCH){
				touch_channels++;
           }

            
	    }
    }

#ifdef DEBUG
    printk(KERN_ERR "[%s %d] min %d, max %d\n", __func__, __LINE__, min, max);    
    printk(KERN_ERR "[%s %d] antitouch_channels %d, touch_channels %d\n", __func__, __LINE__, antitouch_channels, touch_channels);
#endif

    total = (antitouch_channels + touch_channels);
    if (((total > ATCHFRCCALTHR_CAL) && (ATCHFRCCARATIO_CAL < antitouch_channels))) {
		calibrate_chip();
        cal_succsess = 1;
    } else if ((touch_channels <= TOUCH_NUM) && (antitouch_channels > ANTI_TOUCH_NUM)) {
		calibrate_chip();
        cal_succsess = 1;
    } else {
#ifdef DEBUG  
        printk(KERN_ERR "[%s %d] cal_succsess is 0", __func__, __LINE__);
#endif
	    cal_succsess = 0;
    }    


#ifdef DEBUG    
    printk("\n");
    printk("***********************************\n");
    printk("***********************************\n");
    printk("***********************************\n");
#endif
}

#define TIME_CHECK     (50) //time = TIME_CHECK * TIME_POLL
#define TIME_AFTER_CAL (20) // time_after_cal = TIME_AFTER_CAL * TIME_POLL
#define TIME_POLL		(800)   //ms

static void mxt224_cal_timeout(unsigned long arg)
{
	static int i = 0;
    schedule_work(&cal_work);
    
#ifdef DEBUG
	printk(KERN_ERR "[%s %d] i %d", __func__, __LINE__, i);
#endif

    if ((i++ < TIME_CHECK)) {
    	mod_timer(&timer_cal, jiffies + msecs_to_jiffies(TIME_POLL));  
	} else {
		i = 0;
	}

    if (cal_succsess == 1) {
		i = TIME_CHECK - TIME_AFTER_CAL;
    }
    
    return ;
}
#else
static void calibrate_chip_resume(struct work_struct *work)
{
	int i = 0;
	uint8_t anti_config[] = {255, 1, 0, 0};
    for (i = 0; i < 4; i++) {
		write_register(8, i + 6,anti_config + i);
    }    
}
#endif

extern bool g_screen_touch_event;

#if defined(USE_WQ)
static void mxt224_timer(struct work_struct *work)
{
	struct mxt224 *ts = mxt224_tsc;
#elif defined(USE_NORMAL_TIMER)
static void mxt224_timer(unsigned long tsc)
{
    struct mxt224 *ts = (struct mxt224 *)tsc;
#else
static enum hrtimer_restart mxt224_timer(struct hrtimer *handle)
{
	struct mxt224 *ts = container_of(handle, struct mxt224, hr_timer);
#endif
	ts = mxt224_tsc;
	DBG("%s:Enter mxt224_timer",__func__);
	g_screen_touch_event = true;

    while((!gpio_get_value_cansleep(GPIO_CTP_INT)) && (is_suspend == 0)) {
		mxt224_read_values(ts);
		mxt224_update_pen_state(ts);
    }

    /* kick pen up timer - to make sure it expires again(!) */
    mod_timer(&ts->timer, jiffies + msecs_to_jiffies(TS_PENUP_TIMEOUT_MS));
    enable_irq(ts->irq);

#if defined(USE_WQ)
	return;
#elif defined(USE_NORMAL_TIMER)
	return;
#else
	return HRTIMER_NORESTART;
#endif
}

static irqreturn_t mxt224_irq(int irq, void *handle)
{
	struct mxt224 *ts = handle;
	unsigned long flags;

	spin_lock_irqsave(&ts->lock, flags);
	disable_irq_nosync(ts->irq);
	restart_scan(ts, TS_POLL_DELAY);
	spin_unlock_irqrestore(&ts->lock, flags);

	return IRQ_HANDLED;
}

static int mxt224_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct mxt224 *ts;
	struct mxt224_platform_data *pdata = client->dev.platform_data;
	struct input_dev *input_dev;
	int err;

	DBG("Begin Probe MXT224 driver");

        g_client = client;  

	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	ts = kzalloc(sizeof(struct mxt224), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_kfree;
	}

    mxt224_tsc = ts;
	ts->x_max = 1280; // MAX_12BIT;
	ts->y_max = 800; // MAX_12BIT;
	
	ts->client = client;
	ts->input = input_dev;
	ts->irq = client->irq;
	i2c_set_clientdata(client, ts);    

	/* convert touchscreen coordinator to coincide with LCD coordinator */
	/* get tsp at (0,0) */
	tsp_ul_50x50_convert.x = (tsp_ul_50x50.x < tsp_dr_50x50.x) 
	                        ? tsp_ul_50x50.x : (ts->x_max - tsp_ul_50x50.x);
	tsp_ul_50x50_convert.y = (tsp_ul_50x50.y < tsp_dr_50x50.y) 
	                        ? tsp_ul_50x50.y : (ts->y_max - tsp_ul_50x50.y);
	tsp_dr_50x50_convert.x = (tsp_ul_50x50.x < tsp_dr_50x50.x) 
	                        ? tsp_dr_50x50.x : (ts->x_max - tsp_dr_50x50.x);
	tsp_dr_50x50_convert.y = (tsp_ul_50x50.y < tsp_dr_50x50.y) 
	                        ? tsp_dr_50x50.y : (ts->y_max - tsp_dr_50x50.y);
	tsp_origin_convert.x = tsp_ul_50x50_convert.x 
	                            - (((tsp_dr_50x50_convert.x - tsp_ul_50x50_convert.x) 
	                               * 50) / (lcd_size.x - 100));
	tsp_origin_convert.y = tsp_ul_50x50_convert.y 
	                            - (((tsp_dr_50x50_convert.y - tsp_ul_50x50_convert.y) 
	                               * 50) / (lcd_size.y - 100));
#if defined(USE_WQ)
	setup_timer(&ts->timer, mxt224_penup_timeout, (unsigned long)ts);
#elif defined(USE_NORMAL_TIMER)
	setup_timer(&ts->timer, mxt224_timer, (unsigned long)ts);
#else
	ts->hr_timer.function = mxt224_timer;
	hrtimer_init(&ts->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
#endif

	spin_lock_init(&ts->lock);

	if (pdata->init_platform_hw) {
		if ((err = pdata->init_platform_hw()) < 0)
	        goto err_hrtimer;
    }

	if ((err = mxt224_generic_probe(ts)) < 0)
        goto err_platform_hw;
        
	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "mxt224_touchscreen";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;
    set_bit(EV_ABS, input_dev->evbit);
    set_bit(ABS_X, input_dev->absbit);
    set_bit(ABS_Y, input_dev->absbit);
    
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ts->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ts->y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xF, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 0xF, 0, 0);

	if ((err = input_register_device(input_dev)) < 0)
		goto err_mxt224_generic_remove;

    	err = request_irq(ts->irq, mxt224_irq, IRQF_TRIGGER_FALLING, /* IRQF_TRIGGER_LOW IRQF_TRIGGER_HIGH */
            client->dev.driver->name, ts);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", ts->irq);
		goto err_input_unregister_device;
	}
	dev_info(&client->dev, "registered with irq (%d)\n", ts->irq);
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = mxt224_early_suspend;
	ts->early_suspend.resume = mxt224_late_resume;
    register_early_suspend(&ts->early_suspend);
#endif
/*exchange the position of g_client = client and  mxt224_resume(ts->client) ,if not g_client will be NULL in function poweron_touchscreen()*/
#ifdef CONFIG_UPDATE_MXT224_FIRMWARE 
       ts_firmware_file();
#endif

    INIT_DELAYED_WORK(&cal_work, calibrate_chip_resume);

/*begin: add by liyaobing l00169718 20110413 for no respond problem when system boot up*/
       mxt224_resume(ts->client);
/*end: add by liyaobing l00169718 20110413 for no respond problem when system boot up*/

	DBG("Probe MXT224 driver SUCCESS");
	return 0;


err_input_unregister_device:
    	input_unregister_device(ts->input);
err_mxt224_generic_remove:
    	mxt224_generic_remove(ts);
err_platform_hw:
    if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();
err_hrtimer:
#if !defined(USE_WQ) && !defined(USE_NORMAL_TIMER)
	hrtimer_cancel(&ts->hr_timer);
#endif
	input_free_device(input_dev);
err_kfree:
	kfree(ts);

	DBG("Probe MXT224 driver FAILED");
	return err;
}

static void mxt224_suspend(struct i2c_client *client, 
			pm_message_t mesg)
{
    struct mxt224_platform_data *pdata = client->dev.platform_data;
	/* clear msg when being sleep */
	is_suspend = 1;
    mxt224_read_values(mxt224_tsc);
        if(0 == is_upgrade_firmware){
          pdata->chip_poweroff() ; 
	   msm_gpiomux_put(GPIO_3V3_EN);
          msm_gpiomux_put(GPIO_CTP_INT);   
	   msm_gpiomux_put(GPIO_CTP_RESET);  
       }
    return ;
}

static void mxt224_resume(struct i2c_client *client)
{
    uint8_t data ;
struct mxt224_platform_data *pdata = client->dev.platform_data;
	 is_suspend = 0;
     if(0 == is_upgrade_firmware) {
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
 if ((get_object_address(USERDATA_T38, 0) == 0) ||(get_object_size(USERDATA_T38) == 0) 
	  	||(READ_MEM_FAILED == read_mem(get_object_address(USERDATA_T38, 0) , 1, &data))){
        msm_gpiomux_get(GPIO_3V3_EN);
        msm_gpiomux_get(GPIO_CTP_INT);   
        msm_gpiomux_get(GPIO_CTP_RESET);  
        pdata->chip_poweron() ; 
         msleep(50);
    }
	 }
    /* clear msg when being resume */
    mxt224_read_values(mxt224_tsc);

    schedule_delayed_work(&cal_work, msecs_to_jiffies(50000));
    
    return ;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxt224_early_suspend(struct early_suspend *h)
{
	struct mxt224 *ts;
	ts = container_of(h, struct mxt224, early_suspend);
	mxt224_suspend(ts->client, PMSG_SUSPEND);
}

static void mxt224_late_resume(struct early_suspend *h)
{
	struct mxt224 *ts;
	ts = container_of(h, struct mxt224, early_suspend);
	mxt224_resume(ts->client);
}
#endif

static int mxt224_remove(struct i2c_client *client)
{
	struct mxt224 *ts = i2c_get_clientdata(client);
	struct mxt224_platform_data *pdata;

	DBG("mxt224 remove");
	pdata = client->dev.platform_data;

	input_unregister_device(ts->input);
	mxt224_generic_remove(ts);
	if (pdata->exit_platform_hw)
	    pdata->exit_platform_hw();
#if !defined(USE_WQ) && !defined(USE_NORMAL_TIMER)
	hrtimer_cancel(&ts->hr_timer);
#endif
	input_free_device(ts->input);
	kfree(ts);

	free_irq(ts->irq, ts);

	return 0;
}



static struct i2c_device_id mxt224_idtable[] = {
	{ "mxt224", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, mxt224_idtable);

static struct i2c_driver mxt224_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "mxt224"
	},
	.id_table	= mxt224_idtable,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend 	= mxt224_suspend,
	.resume 	= mxt224_resume,
#endif
	.probe		= mxt224_probe,
	.remove		= mxt224_remove,
};

static int __init mxt224_init(void)
{
	DBG("Begin INIT MXT224 Module!");
	return i2c_add_driver(&mxt224_driver);
}

static void __exit mxt224_exit(void)
{
	i2c_del_driver(&mxt224_driver);
}

module_init(mxt224_init);
module_exit(mxt224_exit);

MODULE_AUTHOR("Li Yaobing <liyaobing@S7.com>");
MODULE_DESCRIPTION("MXT224 TouchScreen Driver");
MODULE_LICENSE("GPL v2");
