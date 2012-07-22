/* drivers/input/keyboard/s7020.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (C) 2008 Texas Instrument Inc.
 * Copyright (C) 2009 Synaptics, Inc.
 *
 * provides device files /dev/input/event#
 * for named device files, use udev
 * 2D sensors report ABS_X_FINGER(0), ABS_Y_FINGER(0) through ABS_X_FINGER(7), ABS_Y_FINGER(7)
 * NOTE: requires updated input.h, which should be included with this driver
 * 1D/Buttons report BTN_0 through BTN_0 + button_count
 * TODO: report REL_X, REL_Y for flick, BTN_TOUCH for tap (on 1D/0D; done for 2D)
 * TODO: check ioctl (EVIOCGABS) to query 2D max X & Y, 1D button count
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
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/s7020.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>

#ifdef CONFIG_UPDATE_S7020_FIRMWARE 
#include <linux/uaccess.h> 
#include <linux/vmalloc.h>
#include <mach/msm_rpcrouter.h>
#endif
#include <linux/slab.h>

#define BTN_F19 BTN_0
#define BTN_F30 BTN_0
#define SCROLL_ORIENTATION REL_Y

/* maxium power up reset times */
#define MAX_POWERRESET_NUM 10 
/* enable power up reset */
static int power_reset_enable = 1;
/* current power up reset times */
static int power_reset_cnt = 0 ;
static struct workqueue_struct *s7020_wq_reset;

#define GPIO_3V3_EN            (130)  
#define GPIO_CTP_INT   		(140)
#define GPIO_CTP_RESET       	(139)
extern int msm_gpiomux_put(unsigned gpio);
extern int msm_gpiomux_get(unsigned gpio);
static int is_upgrade_firmware = 0;
static void poweron_touchscreen(void);

static struct workqueue_struct *s7020_wq;

/* Register: EGR_0 */
#define EGR_PINCH_REG		0
#define EGR_PINCH 		(1 << 6)
#define EGR_PRESS_REG 		0
#define EGR_PRESS 		(1 << 5)
#define EGR_FLICK_REG 		0
#define EGR_FLICK 		(1 << 4)
#define EGR_EARLY_TAP_REG	0
#define EGR_EARLY_TAP		(1 << 3)
#define EGR_DOUBLE_TAP_REG	0
#define EGR_DOUBLE_TAP		(1 << 2)
#define EGR_TAP_AND_HOLD_REG	0
#define EGR_TAP_AND_HOLD	(1 << 1)
#define EGR_SINGLE_TAP_REG	0
#define EGR_SINGLE_TAP		(1 << 0)
/* Register: EGR_1 */
#define EGR_PALM_DETECT_REG	1
#define EGR_PALM_DETECT		(1 << 0)

#define VALUE_ABS_MT_TOUCH_MAJOR	0x4
#define VALUE_ABS_MT_TOUCH_MINOR	0x3

static unsigned char f01_rmi_ctrl0 = 0;
static unsigned char f01_rmi_data1 = 0;
static unsigned char f11_rmi_ctrl0 = 0;

struct s7020_function_descriptor {
	__u8 queryBase;
	__u8 commandBase;
	__u8 controlBase;
	__u8 dataBase;
	__u8 intSrc;
    
#define INTERRUPT_SOURCE_COUNT(x) (x & 7)

	__u8 functionNumber;
};
#define FD_ADDR_MAX 0xE9
#define FD_ADDR_MIN 0x05
#define FD_BYTE_COUNT 6


#define TOUCH_LCD_X_MAX	3015
#define TOUCH_LCD_Y_MAX	1892

static unsigned char g_tm1771_dect_flag = 0;

/* define in platform/board file(s) */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void s7020_early_suspend(struct early_suspend *h);
static void s7020_late_resume(struct early_suspend *h);
#endif
static int s7020_attn_clear(struct s7020 *ts);

#ifdef CONFIG_UPDATE_S7020_FIRMWARE 
static struct update_firmware_addr {
	char f01_s7020_tm1771_cmd0 ;
	char f01_s7020_tm1771_query0 ;
	char f01_s7020_tm1771_data0 ;
	char f34_s7020_tm1771_query0 ;
	char f34_s7020_tm1771_query3 ;
	char f34_s7020_tm1771_query5 ;
	char f34_s7020_tm1771_query7 ;
	char f34_s7020_tm1771_data0 ;
	char f34_s7020_tm1771_data2 ;
	char f34_s7020_tm1771_data3	;
	char f34_s7020_tm1771_ctrl0 ;
} update_firmware_addr ;

#define  Manufacturer_ID  0x01
#define  TOUCHSCREEN_TYPE "S7020"

#ifdef CONFIG_DEBUG_S7020_FIRMWARE 
#define FLAG_RR 			0x00
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
#define FLAG_DBG 		0x11 
enum  layer_phase
{
    LAYER_PTHASE_DRIVER = 0x0,
    LAYER_PTHASE_EVENTHUB,
    LAYER_PTHASE_DISPATCH,
    MAX_LAYER_PTHASE,
    WFLAG_ENABLE_TA = 0xFE,
};
enum  read_flag
{
    RFLAG_ENABLE_TA = 0x0,
    RFLAG_REPORT_NUM,
    RFLAG_RESEVER=RFLAG_REPORT_NUM+2,/*report num is 2 bytes*/
    RFLAG_LAYER_PHASE_START = RFLAG_RESEVER+2/*resever 2 bytes for future use*/
};
#define  LAYER_PHASE_DATA_NUM	24 /* 24 = SIZEOF(time_tp) = 4*2*3 = 4:sizeof(long int) 2:tv_sec /tv_usec 3:time_first_start/time_first_end/time_last_end*/
#define MAX_DATA_NUM  		RFLAG_LAYER_PHASE_START +MAX_LAYER_PTHASE*LAYER_PHASE_DATA_NUM
#define CONFIG_ID_LEN		4

static char time_ta[MAX_DATA_NUM]={0};
struct time_tp {
	struct timespec  time_first_start; /* first point start time of one action phase*/
	struct timespec  time_first_end;   /* first point end time of one action phase*/
	struct timespec  time_last_end;   /* last point end time of one action phase*/
};
static struct time_tp time_tp ;

static bool  flag_first_point;	 /* flag of whether it is first point */
static bool  flag_last_point;	 /* flag of whether it is last point */
static char flag_enable_ta = false ;
static unsigned short  sensor_report_num = 0 ;

irqreturn_t s7020_irq_handler(int irq, void *dev_id);
static void s7020_disable(struct s7020 *ts);
static void s7020_enable(struct s7020 *ts);
static int ts_debug_X = 0 ;
static int ts_debug_Y = 0 ;
static int debug_level = 0 ;
static ssize_t ts_debug_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t ts_debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

firmware_attr(ts_debug);
#endif

static struct i2c_client *g_client = NULL;
static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t firmware_version_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t tp_control_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t tp_control_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(tp_control);

static int ts_firmware_file(void);
static int i2c_update_firmware(struct i2c_client *client, const  char * filename); 

firmware_attr(update_firmware);
firmware_attr(firmware_version);

#define F01_PRODUCTID_QUERY_OFFSET		11
#define F01_PRODUCTID_SIZE					10
static ssize_t firmware_tptype_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t firmware_tptype_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
firmware_attr(firmware_tptype);
#endif

static ssize_t cap_touchscreen_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
 	  return sprintf(buf, "%d", g_tm1771_dect_flag);		  
 }

static struct kobj_attribute cap_touchscreen_attribute =
         __ATTR(state, 0664, cap_touchscreen_attr_show, NULL);

static struct attribute* cap_touchscreen_attributes[] =
{
	&cap_touchscreen_attribute.attr,
	NULL
};

static struct attribute_group cap_touchscreen_defattr_group =
{
	.attrs = cap_touchscreen_attributes,
};

#ifdef CONFIG_UPDATE_S7020_FIRMWARE 
 struct S7020_TM1771_FDT{
   unsigned char m_QueryBase;
   unsigned char m_CommandBase;
   unsigned char m_ControlBase;
   unsigned char m_DataBase;
   unsigned char m_IntSourceCount;
   unsigned char m_ID;
 };
 
 static int s7020_tm1771_read_PDT(struct i2c_client *client)
 {
     struct S7020_TM1771_FDT temp_buf;
     struct S7020_TM1771_FDT m_PdtF34Flash;
     struct S7020_TM1771_FDT m_PdtF01Common;
     struct i2c_msg msg[2];
     unsigned short start_addr; 
     int ret=0;

     memset(&m_PdtF34Flash,0,sizeof(struct S7020_TM1771_FDT));
     memset(&m_PdtF01Common,0,sizeof(struct S7020_TM1771_FDT));
 
     for(start_addr = 0xe9; start_addr > 10; start_addr -= sizeof(struct S7020_TM1771_FDT)){
         msg[0].addr = client->addr;
         msg[0].flags = 0;
         msg[0].len = 1;
         msg[0].buf = (unsigned char *)&start_addr;
         msg[1].addr = client->addr;
         msg[1].flags = I2C_M_RD;
         msg[1].len = sizeof(struct S7020_TM1771_FDT);
         msg[1].buf = (unsigned char *)&temp_buf;
         if(i2c_transfer(client->adapter, msg, 2) < 0){
             return -1;
         }
 
         if(temp_buf.m_ID == 0x34){
             memcpy(&m_PdtF34Flash,&temp_buf,sizeof(struct S7020_TM1771_FDT ));
             update_firmware_addr.f34_s7020_tm1771_query0 = m_PdtF34Flash.m_QueryBase ;
             update_firmware_addr.f34_s7020_tm1771_query3 = m_PdtF34Flash.m_QueryBase+3 ;
             update_firmware_addr.f34_s7020_tm1771_query5 = m_PdtF34Flash.m_QueryBase+5 ;
             update_firmware_addr.f34_s7020_tm1771_query7 = m_PdtF34Flash.m_QueryBase+7 ;
             update_firmware_addr.f34_s7020_tm1771_data0 = m_PdtF34Flash.m_DataBase ;
             update_firmware_addr.f34_s7020_tm1771_data2 = m_PdtF34Flash.m_DataBase+2 ;
			 update_firmware_addr.f34_s7020_tm1771_ctrl0 = m_PdtF34Flash.m_ControlBase ;
             
             ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_s7020_tm1771_query3);             
             update_firmware_addr.f34_s7020_tm1771_data3 = update_firmware_addr.f34_s7020_tm1771_data2+ret ;
         }else if(temp_buf.m_ID == 0x01){
             memcpy(&m_PdtF01Common,&temp_buf,sizeof(struct S7020_TM1771_FDT ));
             update_firmware_addr.f01_s7020_tm1771_cmd0 = m_PdtF01Common.m_CommandBase ;
             update_firmware_addr.f01_s7020_tm1771_query0 = m_PdtF01Common.m_QueryBase ;
             update_firmware_addr.f01_s7020_tm1771_data0 = m_PdtF01Common.m_DataBase ;
         }else if (temp_buf.m_ID == 0){      //end of PDT
             break;
         }
       }
 	if((m_PdtF01Common.m_CommandBase != update_firmware_addr.f01_s7020_tm1771_cmd0) || (m_PdtF34Flash.m_QueryBase != update_firmware_addr.f34_s7020_tm1771_query0)){
		return -1;
    }
 
    return 0;
 
} 
 
int s7020_tm1771_wait_attn(struct i2c_client * client,int udleay)
{
     int loop_count=0;
     int ret=0;
 
     do{
         mdelay(udleay);
         ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_s7020_tm1771_data3);
         // Clear the attention assertion by reading the interrupt status register
         i2c_smbus_read_byte_data(client,update_firmware_addr.f01_s7020_tm1771_data0 + 1);
     }while(loop_count++ < 0x10 && (ret != 0x80));
 
     if(loop_count >= 0x10){
         return -1;
     }
     return 0;
 }
 
 int s7020_tm1771_disable_program(struct i2c_client *client)
 {
     unsigned char cdata; 
     unsigned int loop_count=0;
   
     // Issue a reset command
     i2c_smbus_write_byte_data(client,update_firmware_addr.f01_s7020_tm1771_cmd0,0x01);
     
     // Wait for ATTN to be asserted to see if device is in idle state
     s7020_tm1771_wait_attn(client,20);
 
     // Read F01 Status flash prog, ensure the 6th bit is '0'
     do{
     	 cdata = i2c_smbus_read_byte_data(client,update_firmware_addr.f01_s7020_tm1771_data0);
         udelay(2);
     } while(((cdata & 0x40) != 0) && (loop_count++ < 10));
 
     //Rescan the Page Description Table
     return s7020_tm1771_read_PDT(client);
 }
 
static int s7020_tm1771_enable_program(struct i2c_client *client)
{
     unsigned short bootloader_id = 0 ;
     int ret = -1;
     // Read and write bootload ID
     bootloader_id = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query0);
     i2c_smbus_write_word_data(client,update_firmware_addr.f34_s7020_tm1771_data2,bootloader_id);
 
     // Issue Enable flash command
     if(i2c_smbus_write_byte_data(client, update_firmware_addr.f34_s7020_tm1771_data3, 0x0F) < 0){
         printk("s7020_tm1771 enter flash mode error\n");
         return -1;
     }
     ret = s7020_tm1771_wait_attn(client,12);
 
     //Rescan the Page Description Table
     s7020_tm1771_read_PDT(client);
     return ret;
 }
 
 static unsigned long ExtractLongFromHeader(const unsigned char* SynaImage) 
 {
     return((unsigned long)SynaImage[0] +
          (unsigned long)SynaImage[1]*0x100 +
          (unsigned long)SynaImage[2]*0x10000 +
          (unsigned long)SynaImage[3]*0x1000000);
 }
 
static int s7020_tm1771_check_firmware(struct i2c_client *client,const unsigned char *pgm_data)
{
     unsigned long checkSumCode;
     unsigned long m_firmwareImgSize;
     unsigned long m_configImgSize;
     unsigned short m_bootloadImgID; 
     unsigned short bootloader_id;
     const unsigned char *SynaFirmware;
     unsigned char m_firmwareImgVersion;
     unsigned short UI_block_count;
     unsigned short CONF_block_count;
     unsigned short fw_block_size;
 
     SynaFirmware = pgm_data;
     checkSumCode = ExtractLongFromHeader(&(SynaFirmware[0]));
     m_bootloadImgID = (unsigned int)SynaFirmware[4] + (unsigned int)SynaFirmware[5]*0x100;
     m_firmwareImgVersion = SynaFirmware[7];
     m_firmwareImgSize    = ExtractLongFromHeader(&(SynaFirmware[8]));
     m_configImgSize      = ExtractLongFromHeader(&(SynaFirmware[12]));
  
     UI_block_count  = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query5);
     fw_block_size = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query3);
     CONF_block_count = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query7);
     bootloader_id = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query0);
     
     return (m_firmwareImgVersion != 0 || bootloader_id == m_bootloadImgID) ? 0 : -1;
}
 
 
 static int s7020_tm1771_write_image(struct i2c_client *client,unsigned char type_cmd,const unsigned char *pgm_data)
 {
     unsigned short block_size;
     unsigned short img_blocks;
     unsigned short block_index;
     const unsigned char * p_data;
     int i;
     
     block_size = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query3);
     switch(type_cmd ){
         case 0x02:
             img_blocks = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query5);   //UI Firmware
             break;
         case 0x06:
             img_blocks = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query7);   //Configure 
             break;
         default:
             printk("image type error\n");
             goto error;
     }
 
     p_data = pgm_data;
     for(block_index = 0; block_index < img_blocks; ++block_index){
         printk("."); 
         // Write Block Number
         if(i2c_smbus_write_word_data(client, update_firmware_addr.f34_s7020_tm1771_data0,block_index) < 0){
             printk("write block number error\n");
             goto error;
         }
 
         for(i=0;i<block_size;i++){
             if(i2c_smbus_write_byte_data(client, update_firmware_addr.f34_s7020_tm1771_data2+i, *(p_data+i)) < 0){
                 printk("s7020_tm1771_write_image: block %d data 0x%x error\n",block_index,*p_data);
                 goto error;
             }
             udelay(15);
         }
         p_data += block_size;   
 
         // Issue Write Firmware or configuration Block command
         if(i2c_smbus_write_word_data(client, update_firmware_addr.f34_s7020_tm1771_data3, type_cmd) < 0){
             printk("issue write command error\n");
             goto error;
         }
 
         // Wait ATTN. Read Flash Command register and check error
         if(s7020_tm1771_wait_attn(client,5) != 0)
             goto error;
     }
 	 printk("\n"); 
     return 0;
 error:
     return -1;
 }
 
 
 static int s7020_tm1771_program_configuration(struct i2c_client *client,const unsigned char *pgm_data )
 {
     int ret;
     unsigned short block_size;
     unsigned short ui_blocks;
 
     block_size = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query3);
     ui_blocks = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query5);    //UI Firmware
 
     if(s7020_tm1771_write_image(client, 0x06,pgm_data+ui_blocks*block_size ) < 0){
         printk("write configure image error\n");
         return -1;
     }
     ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_s7020_tm1771_data3);
     return ((ret & 0xF0) == 0x80 ? 0 : ret);
 }
 
 static int s7020_tm1771_program_firmware(struct i2c_client *client,const unsigned char *pgm_data)
 {
     int ret=0;
     unsigned short bootloader_id;
 
     //read and write back bootloader ID
     bootloader_id = i2c_smbus_read_word_data(client,update_firmware_addr.f34_s7020_tm1771_query0);
     i2c_smbus_write_word_data(client,update_firmware_addr.f34_s7020_tm1771_data2, bootloader_id );
     //issue erase commander
     if(i2c_smbus_write_byte_data(client, update_firmware_addr.f34_s7020_tm1771_data3, 0x03) < 0){
         printk("s7020_tm1771_program_firmware error, erase firmware error \n");
         return -1;
     }
     s7020_tm1771_wait_attn(client,300);
 
     //check status
     if((ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_s7020_tm1771_data3)) != 0x80){
		 printk("check firmware status error!\n");
         return -1;
     }
 
     //write firmware
     if( s7020_tm1771_write_image(client,0x02,pgm_data) <0 ){
         printk("write UI firmware error!\n");
         return -1;
     }
 
     ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_s7020_tm1771_data3);
     return ((ret & 0xF0) == 0x80 ? 0 : ret);
 }
 
 static int s7020_tm1771_download(struct i2c_client *client,const unsigned char *pgm_data)
 {
     int ret;
 
     ret = s7020_tm1771_read_PDT(client);
     if(ret != 0){
         printk("s7020_tm1771 page func check error\n");
         return -1;
     }
	 /* deleted for s7020 chip revision, l00185943, begin 2012/1/20 */
	 /* deleted for s7020 chip revision, l00185943, end 2012/1/20 */
	 ret = s7020_tm1771_enable_program(client);
     if( ret != 0){
         printk("%s:%d:s7020_tm1771 enable program error,return...\n",__FUNCTION__,__LINE__);
         goto error;
     }
 
     ret = s7020_tm1771_check_firmware(client,pgm_data);
     if( ret != 0){
         printk("%s:%d:s7020_tm1771 check firmware error,return...\n",__FUNCTION__,__LINE__);
         goto error;
     }
 
     ret = s7020_tm1771_program_firmware(client, pgm_data + 0x100);
     if( ret != 0){
         printk("%s:%d:s7020_tm1771 program firmware error,return...",__FUNCTION__,__LINE__);
         goto error;
     }
 
     ret = s7020_tm1771_program_configuration(client, pgm_data +  0x100);
     if( ret != 0){
         printk("%s:%d:s7020_tm1771 program configuration error,return...",__FUNCTION__,__LINE__);
         goto error;
     }
     return s7020_tm1771_disable_program(client);
 
 error:
     s7020_tm1771_disable_program(client);
     printk("%s:%d:error,return ....",__FUNCTION__,__LINE__);
     return -1;
 
 }
 
 static int i2c_update_firmware(struct i2c_client *client, const  char * file) 
 {
     char *buf;
     struct file *filp;
     struct inode *inode = NULL;
     mm_segment_t oldfs;
     uint16_t    length;
     int ret = 0;
 
     /* open file */
     oldfs = get_fs();
     set_fs(KERNEL_DS);
     filp = filp_open(file, O_RDONLY, S_IRUSR);
     if (IS_ERR(filp)) {
         printk("%s: file %s filp_open error\n", __FUNCTION__,file);
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
     printk("s7020_tm1771 image file is %s and data size is %d Bytes \n",file,length);
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
 
     /* read data */
     if (filp->f_op->read(filp, buf, length, &filp->f_pos) != length) {
         printk("%s: file read error\n", __FUNCTION__);
         filp_close(filp, NULL);
         set_fs(oldfs);
         vfree(buf);
         return -1;
     }
 
     ret = s7020_tm1771_download(client,buf);
 
     filp_close(filp, NULL);
     set_fs(oldfs);
     vfree(buf);
     return ret;
 }
 
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

	ret = sysfs_create_file(kobject_ts, &tp_control_attr.attr);
	if (ret) {
	    kobject_put(kobject_ts);
	    printk("create file error\n");
	    return -1;
	}
	
#ifdef CONFIG_DEBUG_S7020_FIRMWARE 
     ret = sysfs_create_file(kobject_ts, &ts_debug_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create debug_ts file error\n");
         return -1;
     }
#endif
     ret = sysfs_create_file(kobject_ts, &firmware_tptype_attr.attr);
     if (ret) {
         kobject_put(kobject_ts);
         printk("create tptype file error\n");
         return -1;
     }
     return 0;   
 }

static void poweron_touchscreen(void){
    struct s7020  * ts ;
    ts = (struct s7020 *)g_client->dev.platform_data; 

	if(i2c_smbus_read_byte_data(g_client,0x00) < 0){
        printk("touchscreen has poweroff and wil poweron for do some operation\n");
        msm_gpiomux_get(GPIO_3V3_EN);
        msm_gpiomux_get(GPIO_CTP_INT);   
        msm_gpiomux_get(GPIO_CTP_RESET);  
        ts->chip_poweron_reset() ; 
    }
    if(1 == is_upgrade_firmware){
		msleep(500) ;
    }
}
	
#ifdef CONFIG_DEBUG_S7020_FIRMWARE
 static ssize_t ts_debug_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
	memcpy(time_ta+RFLAG_LAYER_PHASE_START,&time_tp,LAYER_PHASE_DATA_NUM);
	memcpy(time_ta+RFLAG_REPORT_NUM,&sensor_report_num,sizeof(unsigned short));	
	memcpy(buf,time_ta,MAX_DATA_NUM); 
	return MAX_DATA_NUM ;
 }
 
 static ssize_t ts_debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
     struct s7020  * ts ;
     int ret = 0 ;
     unsigned short bootloader_id;
	 
     ts = (struct s7020 *)g_client->dev.platform_data; 
 
     if(buf[0] == FLAG_RR){
             return i2c_smbus_read_byte_data(g_client,buf[1]);
     }else if(buf[0] == FLAG_WR){
	 if(i2c_smbus_write_byte_data(g_client, buf[1],buf[2]) < 0){
                 return 0 ;
        }
	 else
	 	return 1 ;
     }else if(buf[0] == FLAG_RPX){
		return ts_debug_X ;
     }else if(buf[0] == FLAG_RPY){
		return ts_debug_Y ;
     }else if(buf[0] == FLAG_RNIT){
		return ts->interrupts_pin_status() ;
     }else if(buf[0] == FLAG_HRST){
		return ts->chip_reset() ; ;
     }else  if(buf[0] == FLAG_SRST){
     		ret =  i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_s7020_tm1771_cmd0);
		ret |= 0x01  ;
		 if(i2c_smbus_write_byte_data(g_client, update_firmware_addr.f01_s7020_tm1771_cmd0,ret) < 0){
	                 return 0 ;
	        }
		 else
		 	return 1 ;
     }else if(buf[0] == FLAG_EINT){
            /* if not interrupts modem ,return fail */
           if (0 == ts->use_irq)
	        return 0 ;
            /* register msm8660 interrupts */
	     ret = request_irq(g_client->irq, s7020_irq_handler,IRQF_TRIGGER_LOW,  g_client->name, ts);
	     if(ret) {
	        printk(KERN_ERR "Failed to request IRQ!ret = %d\n", ret); 
		  	return 0 ;
	     }
		 
	    /* clear s7020 interrupt flag */
	    s7020_attn_clear(ts);
	    /* s7020 leave sleep mode */
	    ret = i2c_smbus_read_byte_data(g_client,f01_rmi_ctrl0);
	    ret &=  ~(0x03);
	    i2c_smbus_write_byte_data(g_client,f01_rmi_ctrl0,ret);
	    return 1 ;
     }else if(buf[0] == FLAG_DINT){
        /* if not interrupts modem ,return fail */
        if (0 == ts->use_irq )
	     	return 0 ;
	     /* s7020 enter sleep mode */
	    ret = i2c_smbus_read_byte_data(g_client,f01_rmi_ctrl0);
	    ret = (ret | 1) & (~(1 << 1)) ;
	    i2c_smbus_write_byte_data(g_client,f01_rmi_ctrl0,ret);
	    /* clear s7020 interrupt flag */
	    s7020_attn_clear(ts);
	    /* disable msm8660 interrupts */
	    s7020_disable(ts);
	   /* unregister msm8660 interrupts */
	    free_irq(g_client->irq, ts);
	    return 1 ;
 }else if(buf[0] == FLAG_RTA){
       if(buf[1] == WFLAG_ENABLE_TA){
		flag_enable_ta = buf[2] ;
		time_ta[RFLAG_ENABLE_TA] = flag_enable_ta ;
		return 1 ;
      	}else if(buf[1]== LAYER_PTHASE_DRIVER){
		return 1 ;/* write of time_ta is at ts_debug_show(),so here return directly */
      }else if((buf[1]>= LAYER_PTHASE_EVENTHUB) &&  (buf[1] <= MAX_LAYER_PTHASE)){
   		memcpy(time_ta+RFLAG_LAYER_PHASE_START+buf[1]*LAYER_PHASE_DATA_NUM,buf+2,LAYER_PHASE_DATA_NUM);  
		return 1 ;
      } else
	 	return 0 ;
     }else  if(buf[0] == FLAG_PRST){
        power_reset_enable = 1 ;
        return ts->chip_poweron_reset() ; 
    }else  if(buf[0] == FLAG_POFF){
        /* disable power reset after execute power off debug command */
        power_reset_enable = 0 ;
        return ts->chip_poweroff() ; 
   }else  if(buf[0] == FLAG_EFW){
		if(s7020_tm1771_enable_program(g_client) != 0){
		         printk("s7020_tm1771 page func check error\n");
		         return 0;
	    }
		bootloader_id = i2c_smbus_read_word_data(g_client,update_firmware_addr.f34_s7020_tm1771_query0);
        i2c_smbus_write_word_data(g_client,update_firmware_addr.f34_s7020_tm1771_data2, bootloader_id );
     		//issue erase commander
     		if(i2c_smbus_write_byte_data(g_client, update_firmware_addr.f34_s7020_tm1771_data3, 0x03) < 0){
	         	printk("s7020_tm1771_program_firmware error, erase firmware error \n");
	         	return 0;
	    }
    	s7020_tm1771_wait_attn(g_client,300);
	    //check status
	    if((ret = i2c_smbus_read_byte_data(g_client,update_firmware_addr.f34_s7020_tm1771_data3)) != 0x80){
			 printk("check firmware status error!\n");
	         return 0;
     	}
		 return 1 ;
    }else if(buf[0] == FLAG_DBG){
	       	 debug_level = buf[1] ;
		return 1 ;
   }else {
		return 0;
     }
  }
 #endif
 
static ssize_t firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
	char values[CONFIG_ID_LEN] = {0};
	unsigned int i;
	
	poweron_touchscreen();
    /* modified for s7020 chip revision reading, l00185943, begin 2012/1/20 */
	for(i = 0; i < CONFIG_ID_LEN-1; i++) {
		values[i] = i2c_smbus_read_byte_data(g_client, (uint8_t)update_firmware_addr.f34_s7020_tm1771_ctrl0+i+1);
		if ((values[i] < '0') || (values[i] > '9')) {
			values[i] = '0';
		}
	}
    /* modified for s7020 chip revision reading, l00185943, end 2012/1/20 */
	return sprintf(buf, "%s", values) ;
 }
 
 static ssize_t firmware_version_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
	return 0;
 }
 
  static ssize_t tp_control_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
  {
      return 0;
  }
 
  static ssize_t tp_control_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
  {
      struct s7020  * ts ;
      int on = -1;
      ts = (struct s7020 *)g_client->dev.platform_data;     
      on = simple_strtoul(buf, NULL, 0);
      if (on == 0 && is_upgrade_firmware == 0) {
      	power_reset_enable = 0;
        ts->chip_poweroff() ;
      	return  count;
      } else if (on == 1) {      
      	power_reset_enable = 1;
      	ts->chip_poweron_reset(); 
        return count;
      } else {
			printk(KERN_ERR "Invalid argument or firmware upgrading.");
            return -1;
      }
  }
 static ssize_t firmware_tptype_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
	char tptype[F01_PRODUCTID_SIZE+1]={0};
	unsigned char i=0;

    poweron_touchscreen();
	
	for(i=0;i<F01_PRODUCTID_SIZE;i++)
       	tptype[i] = i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_s7020_tm1771_query0+F01_PRODUCTID_QUERY_OFFSET+i);
	return sprintf(buf, "%s", TOUCHSCREEN_TYPE);
 }
 
 static ssize_t firmware_tptype_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
	return 0;
 }

 static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
     return sprintf(buf, "%s", TOUCHSCREEN_TYPE);	
 }
 
 static ssize_t update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
     int i, ret = -1;
     unsigned char path_image[255];
 
     printk("start s7020_tm1771 firmware update download\n");
 
     if(count >255 || count == 0 || strnchr(buf, count, 0x20))
         return ret;     
     
     memcpy (path_image, buf,  count);
     /* replace '\n' with  '\0'  */ 
     if((path_image[count-1]) == '\n')
     	path_image[count-1] = '\0'; 
     else
 		path_image[count] = '\0';
     
     if(1){
        
         goto firmware_find_device;
 
         /* driver detect its device  */
         for (i = 0; i < 3; i++) {   
         	 ret = i2c_smbus_read_byte_data(g_client, update_firmware_addr.f01_s7020_tm1771_query0);
             if (ret == Manufacturer_ID){
                 goto firmware_find_device;
             }
 
         }
         printk("Do not find s7020_tm1771 device\n");   
         return -1;
 
 firmware_find_device:
		power_reset_enable = 0 ;
		is_upgrade_firmware = 1 ;
	    poweron_touchscreen();
	     disable_irq(g_client->irq);          
	     ret = i2c_update_firmware(g_client, path_image);
	     enable_irq(g_client->irq);

	     if( 0 != ret ){
	         printk("Update firmware failed!\n");
	         ret = -1;
	     } else {
	         printk("Update firmware success!\n");
	         //arm_pm_restart('0', (const char *)&ret);
	         ret = 1;
	     }
     }
	power_reset_enable = 1 ;
	is_upgrade_firmware = 0 ;
	return ret;
  }
 
#endif
static void ts_update_pen_state(struct s7020 *ts, int x, int y, int pressure, int wx, int wy)
{
	if (pressure) {
#ifdef CONFIG_SYNA_MT
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
        input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
        input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, VALUE_ABS_MT_TOUCH_MAJOR/*max(wx, wy)*/);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, VALUE_ABS_MT_TOUCH_MINOR/*min(wx, wy)*/);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, pressure/2);
        input_mt_sync(ts->input_dev);
#endif
	} else {
#ifdef CONFIG_DEBUG_S7020_FIRMWARE
	ts_debug_X =x ;
	ts_debug_Y = y ;
	if(debug_level >= 1)
		printk("x=%d,y=%d\n",ts_debug_X,ts_debug_Y);
#endif

#ifdef CONFIG_SYNA_MT
			input_mt_sync(ts->input_dev);
#endif
		}
	}

static int s7020_read_pdt(struct s7020 *ts)
{
	int ret = 0;
	int nFd = 0;
	int interruptCount = 0;
	__u8 data_length;

	struct i2c_msg fd_i2c_msg[2];
	__u8 fd_reg;
	struct s7020_function_descriptor fd;

	struct i2c_msg query_i2c_msg[2];
	__u8 query[14];
	__u8 *egr;

	fd_i2c_msg[0].addr = ts->client->addr;
	fd_i2c_msg[0].flags = 0;
	fd_i2c_msg[0].buf = &fd_reg;
	fd_i2c_msg[0].len = 1;

	fd_i2c_msg[1].addr = ts->client->addr;
	fd_i2c_msg[1].flags = I2C_M_RD;
	fd_i2c_msg[1].buf = (__u8 *)(&fd);
	fd_i2c_msg[1].len = FD_BYTE_COUNT;

	query_i2c_msg[0].addr = ts->client->addr;
	query_i2c_msg[0].flags = 0;
	query_i2c_msg[0].buf = &fd.queryBase;
	query_i2c_msg[0].len = 1;

	query_i2c_msg[1].addr = ts->client->addr;
	query_i2c_msg[1].flags = I2C_M_RD;
	query_i2c_msg[1].buf = query;
	query_i2c_msg[1].len = sizeof(query);


	ts->hasF11 = false;
	ts->hasF19 = false;
	ts->hasF30 = false;
	ts->data_reg = 0xff;
	ts->data_length = 0;

	for (fd_reg = FD_ADDR_MAX; fd_reg >= FD_ADDR_MIN; fd_reg -= FD_BYTE_COUNT) {
		ret = i2c_transfer(ts->client->adapter, fd_i2c_msg, 2);
		if (ret < 0) {
			printk(KERN_ERR "I2C read failed querying $%02X capabilities\n", ts->client->addr);
			return ret;
		}

		if (!fd.functionNumber) {
			/* End of PDT */
			ret = nFd;
			printk("Read %d functions from PDT\n", nFd);
			break;
		}

		++nFd;

		switch (fd.functionNumber) {
			case 0x01: /* Interrupt */
				ts->f01.data_offset = fd.dataBase;
				f01_rmi_ctrl0 = fd.controlBase;
        		f01_rmi_data1 = fd.dataBase+1 ;
        		/*
				 * Can't determine data_length
				 * until whole PDT has been read to count interrupt sources
				 * and calculate number of interrupt status registers.
				 * Setting to 0 safely "ignores" for now.
				 */
				data_length = 0;
				break;
			case 0x11: /* 2D */
				ts->hasF11 = true;
                f11_rmi_ctrl0 = fd.controlBase;
                ts->f11.data_offset = fd.dataBase;
				ts->f11.interrupt_offset = interruptCount / 8;
				ts->f11.interrupt_mask = ((1 << INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F11 query registers\n");

				ts->f11.points_supported = (query[1] & 7) + 1;
				if (ts->f11.points_supported == 6)
					ts->f11.points_supported = 10;

				ts->f11_fingers = kcalloc(ts->f11.points_supported,
				                          sizeof(*ts->f11_fingers), 0);
				memset(ts->f11_fingers, 0, 
					ts->f11.points_supported * sizeof(*ts->f11_fingers));
				ts->f11_has_gestures = (query[1] >> 5) & 1;
				ts->f11_has_relative = (query[1] >> 3) & 1;

				i2c_smbus_write_byte_data(ts->client, fd.controlBase+2, 0x19);
                i2c_smbus_write_byte_data(ts->client, fd.controlBase+3, 0x19);
				
				egr = &query[7];

#define EGR_DEBUG
#ifdef EGR_DEBUG
#define EGR_INFO printk
#else
#define EGR_INFO
#endif
				EGR_INFO("EGR features:\n");
				ts->hasEgrPinch = egr[EGR_PINCH_REG] & EGR_PINCH;
				EGR_INFO("\tpinch: %u\n", ts->hasEgrPinch);
				ts->hasEgrPress = egr[EGR_PRESS_REG] & EGR_PRESS;
				EGR_INFO("\tpress: %u\n", ts->hasEgrPress);
				ts->hasEgrFlick = egr[EGR_FLICK_REG] & EGR_FLICK;
				EGR_INFO("\tflick: %u\n", ts->hasEgrFlick);
				ts->hasEgrEarlyTap = egr[EGR_EARLY_TAP_REG] & EGR_EARLY_TAP;
				EGR_INFO("\tearly tap: %u\n", ts->hasEgrEarlyTap);
				ts->hasEgrDoubleTap = egr[EGR_DOUBLE_TAP_REG] & EGR_DOUBLE_TAP;
				EGR_INFO("\tdouble tap: %u\n", ts->hasEgrDoubleTap);
				ts->hasEgrTapAndHold = egr[EGR_TAP_AND_HOLD_REG] & EGR_TAP_AND_HOLD;
				EGR_INFO("\ttap and hold: %u\n", ts->hasEgrTapAndHold);
				/* since if  ts->hasEgrSingleTap is 1,there will have some problem in android ICS version,
				  *  so here we set it 0 diretcly
				  *  //ts->hasEgrSingleTap = egr[EGR_SINGLE_TAP_REG] & EGR_SINGLE_TAP;
				  */
				ts->hasEgrSingleTap = 0 ;
				EGR_INFO("\tsingle tap: %u\n", ts->hasEgrSingleTap);
				ts->hasEgrPalmDetect = egr[EGR_PALM_DETECT_REG] & EGR_PALM_DETECT;
				EGR_INFO("\tpalm detect: %u\n", ts->hasEgrPalmDetect);


				query_i2c_msg[0].buf = &fd.controlBase;
				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F11 control registers\n");

				query_i2c_msg[0].buf = &fd.queryBase;

				ts->f11_max_x = ((query[7] & 0x0f) * 0x100) | query[6];
				ts->f11_max_y = ((query[9] & 0x0f) * 0x100) | query[8];

				printk("max X: %d; max Y: %d\n", ts->f11_max_x, ts->f11_max_y);

				ts->f11.data_length = data_length =
					/* finger status, four fingers per register */
					((ts->f11.points_supported + 3) / 4)
					/* absolute data, 5 per finger */
					+ 5 * ts->f11.points_supported
					/* two relative registers */
					+ (ts->f11_has_relative ? 2 : 0)
					/* F11_2D_Data8 is only present if the egr_0 register is non-zero. */
					+ (egr[0] ? 1 : 0)
					/* F11_2D_Data9 is only present if either egr_0 or egr_1 registers are non-zero. */
					+ ((egr[0] || egr[1]) ? 1 : 0)
					/* F11_2D_Data10 is only present if EGR_PINCH or EGR_FLICK of egr_0 reports as 1. */
					+ ((ts->hasEgrPinch || ts->hasEgrFlick) ? 1 : 0)
					/* F11_2D_Data11 and F11_2D_Data12 are only present if EGR_FLICK of egr_0 reports as 1. */
					+ (ts->hasEgrFlick ? 2 : 0)
					;

				break;
			case 0x19: /* Cap Buttons */
				ts->hasF19 = true;

				ts->f19.data_offset = fd.dataBase;
				ts->f19.interrupt_offset = interruptCount / 8;
				ts->f19.interrupt_mask = ((1 < INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F19 query registers\n");


				ts->f19.points_supported = query[1] & 0x1F;
				ts->f19.data_length = data_length = (ts->f19.points_supported + 7) / 8;

				printk(KERN_NOTICE "$%02X F19 has %d buttons\n", ts->client->addr, ts->f19.points_supported);

				break;
			case 0x30: /* GPIO */
				ts->hasF30 = true;

				ts->f30.data_offset = fd.dataBase;
				ts->f30.interrupt_offset = interruptCount / 8;
				ts->f30.interrupt_mask = ((1 < INTERRUPT_SOURCE_COUNT(fd.intSrc)) - 1) << (interruptCount % 8);

				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F30 query registers\n");


				ts->f30.points_supported = query[1] & 0x1F;
				ts->f30.data_length = data_length = (ts->f30.points_supported + 7) / 8;

				break;
#ifdef CONFIG_UPDATE_S7020_FIRMWARE
            case 0x34:                         
                ts->hasF34 = true;
                ts->f34.data_offset  = fd.dataBase;                    
                break;
#endif 
			default:
				goto pdt_next_iter;
		}

		// Change to end address for comparison
		// NOTE: make sure final value of ts->data_reg is subtracted
		data_length += fd.dataBase;
		if (data_length > ts->data_length) {
			ts->data_length = data_length;
		}

		if (fd.dataBase < ts->data_reg) {
			ts->data_reg = fd.dataBase;
		}

pdt_next_iter:
		interruptCount += INTERRUPT_SOURCE_COUNT(fd.intSrc);
	}

	// Now that PDT has been read, interrupt count determined, F01 data length can be determined.
	ts->f01.data_length = data_length = 1 + ((interruptCount + 7) / 8);
	// Change to end address for comparison
	// NOTE: make sure final value of ts->data_reg is subtracted
	data_length += ts->f01.data_offset;
	if (data_length > ts->data_length) {
		ts->data_length = data_length;
	}

	// Change data_length back from end address to length
	// NOTE: make sure this was an address
	ts->data_length -= ts->data_reg;

	// Change all data offsets to be relative to first register read
	// TODO: add __u8 *data (= &ts->data[ts->f##.data_offset]) to struct rmi_function_info?
	ts->f01.data_offset -= ts->data_reg;
	ts->f11.data_offset -= ts->data_reg;
	ts->f19.data_offset -= ts->data_reg;
	ts->f30.data_offset -= ts->data_reg;

	ts->data = kcalloc(ts->data_length, sizeof(*ts->data), 0);
	if (ts->data == NULL) {
		printk(KERN_ERR "Not enough memory to allocate space for data\n");
		ret = -ENOMEM;
	}

	ts->data_i2c_msg[0].addr = ts->client->addr;
	ts->data_i2c_msg[0].flags = 0;
	ts->data_i2c_msg[0].len = 1;
	ts->data_i2c_msg[0].buf = &ts->data_reg;

	ts->data_i2c_msg[1].addr = ts->client->addr;
	ts->data_i2c_msg[1].flags = I2C_M_RD;
	ts->data_i2c_msg[1].len = ts->data_length;
	ts->data_i2c_msg[1].buf = ts->data;

	printk(KERN_ERR " $%02X data read: $%02X + %d\n",
		ts->client->addr, ts->data_reg, ts->data_length);

	return ret;
}

#ifdef CONFIG_SYNA_BUTTONS_SCROLL
static int first_button(__u8 *button_data, __u8 points_supported)
{
	int b, reg;

	for (reg = 0; reg < ((points_supported + 7) / 8); reg++)
		for (b = 0; b < 8; b++)
			if ((button_data[reg] >> b) & 1)
				return reg * 8 + b;

	return -1;
}

static void s7020_report_scroll(struct input_dev *dev,
                                    __u8 *button_data,
                                    __u8 points_supported,
                                    int ev_code)
{
	int scroll = 0;
	static int last_button = -1, current_button = -1;

	// This method is slightly problematic
	// It makes no check to find if lift/touch is more likely than slide
	current_button = first_button(button_data, points_supported);

	if (current_button >= 0 && last_button >= 0) {
		scroll = current_button - last_button;
		// This filter mostly works to isolate slide motions from lift/touch
		if (abs(scroll) == 1) {
			input_report_rel(dev, ev_code, scroll);
		}
	}

	last_button = current_button;
}
#endif

static void s7020_report_buttons(struct input_dev *dev,
                                     __u8 *data,
                                     __u8 points_supported,
                                     int base)
{
	int b;

	for (b = 0; b < points_supported; ++b) {
		int button = (data[b / 8] >> (b % 8)) & 1;
		input_report_key(dev, base + b, button);
	}
}

static void s7020_work_reset_func(struct work_struct *work)
{
	struct s7020  * ts ;
 	ts = (struct s7020 *)g_client->dev.platform_data; 

    /* if disable power reset,return directly */
	if(0 == power_reset_enable)
		return ;
	if(i2c_smbus_read_byte_data(g_client,0x00) < 0){
		/* if i2c communication has problem, power up reset touchcreen chip */
		if(power_reset_cnt < MAX_POWERRESET_NUM){
			printk(KERN_ERR "%s:touchscreen i2c has problem and power up reset it\n", __func__);
			ts->chip_poweron_reset() ; 
			power_reset_cnt++;
		}
	}else{
		power_reset_cnt = 0 ;
	}
}

extern bool g_screen_touch_event;
static void s7020_work_func(struct work_struct *work)
{
	int ret;
	u32 z = 0;
	u32 wx = 0, wy = 0;
	u32 x = 0, y = 0;
	u8 prev_status;

	struct s7020 *ts = container_of(work,struct s7020, work);

	g_screen_touch_event = true;
    power_reset_enable = 0 ;
    
	ret = i2c_transfer(ts->client->adapter, ts->data_i2c_msg, 2);

	if (ret < 0) {
		printk(KERN_ERR "%s: i2c_transfer failed\n", __func__);
	} else {
		__u8 *interrupt = &ts->data[ts->f01.data_offset + 1];
		if (ts->hasF11 && interrupt[ts->f11.interrupt_offset] & ts->f11.interrupt_mask) {
			__u8 *f11_data = &ts->data[ts->f11.data_offset];
			int f;
			__u8 finger_status_reg = 0;
			__u8 fsr_len = (ts->f11.points_supported + 3) / 4;
			__u8 finger_status;            

			for (f = 0; f < ts->f11.points_supported; ++f) {			
				if (!(f % 4))
					finger_status_reg = f11_data[f / 4];
				finger_status = (finger_status_reg >> ((f % 4) * 2)) & 3;
				prev_status = ts->f11_fingers[f].status;
				if (!finger_status && prev_status) {
					ts_update_pen_state(ts, 0, 0, 0, 0, 0); 
					ts->f11_fingers[f].status = finger_status;
#ifdef CONFIG_DEBUG_S7020_FIRMWARE
				if ( 1 == flag_enable_ta){
					if (false == flag_last_point){
						flag_last_point = true ;								
					}
		         }
#endif
					continue;
				} else if (!finger_status && !prev_status) {
					ts->f11_fingers[f].status = finger_status;
 				} else {
						__u8 reg = fsr_len + 5 * f;
						__u8 *finger_reg = &f11_data[reg];

						x = (finger_reg[0] * 0x10) | (finger_reg[2] % 0x10);
						y = (finger_reg[1] * 0x10) | (finger_reg[2] / 0x10);
						wx = finger_reg[3] % 0x10;
						wy = finger_reg[3] / 0x10;
						z = finger_reg[4];
#ifdef CONFIG_DEBUG_S7020_FIRMWARE
						ts_debug_X =x ;
						ts_debug_Y = y ;
					if(debug_level >= 1)
						printk("x=%d,y=%d\n",ts_debug_X,ts_debug_Y);
#endif
                        ts_update_pen_state(ts, x, y, z, wx, wy);

#ifdef CONFIG_SYNA_MULTIFINGER
						/* Report multiple fingers for software prior to 2.6.31 - not standard - uses special input.h */
						input_report_abs(ts->input_dev, ABS_X_FINGER(f), x);
						input_report_abs(ts->input_dev, ABS_Y_FINGER(f), y);
						input_report_abs(ts->input_dev, ABS_Z_FINGER(f), z);
#endif

						ts->f11_fingers[f].status = finger_status;
	            	}
	            }
		if(debug_level >= 3)
			printk("%s:s7020 work func \n",__func__);
	            /* end: modify by liyaobing 2011/1/6 */
                
				/* f == ts->f11.points_supported */
				/* set f to offset after all absolute data */
				f = (f + 3) / 4 + f * 5;
				if (ts->f11_has_relative) {
					/* NOTE: not reporting relative data, even if available */
					/* just skipping over relative data registers */
					f += 2;
				}

	            if (ts->hasEgrPalmDetect) {
	                         	input_report_key(ts->input_dev,
					                 BTN_DEAD,
					                 f11_data[f + EGR_PALM_DETECT_REG] & EGR_PALM_DETECT);
				}

	            if (ts->hasEgrFlick) {
	                         	if (f11_data[f + EGR_FLICK_REG] & EGR_FLICK) {
						input_report_rel(ts->input_dev, REL_X, f11_data[f + 2]);
						input_report_rel(ts->input_dev, REL_Y, f11_data[f + 3]);
					}
				}

	            if (ts->hasEgrSingleTap) {
					input_report_key(ts->input_dev,
					                 BTN_TOUCH,
					                 f11_data[f + EGR_SINGLE_TAP_REG] & EGR_SINGLE_TAP);
				}

	            if (ts->hasEgrDoubleTap) {
					input_report_key(ts->input_dev,
					                 BTN_TOOL_DOUBLETAP,
					                 f11_data[f + EGR_DOUBLE_TAP_REG] & EGR_DOUBLE_TAP);
				}
			}            
            
		if (ts->hasF19 && interrupt[ts->f19.interrupt_offset] & ts->f19.interrupt_mask) {
			int reg;
			int touch = 0;
			for (reg = 0; reg < ((ts->f19.points_supported + 7) / 8); reg++) {
				if (ts->data[ts->f19.data_offset + reg]) {
					touch = 1;
					break;
				}
            }
			input_report_key(ts->input_dev, BTN_DEAD, touch);

#ifdef  CONFIG_SYNA_BUTTONS
			s7020_report_buttons(ts->input_dev,
			                         &ts->data[ts->f19.data_offset],
                                                 ts->f19.points_supported, BTN_F19);
#endif

#ifdef  CONFIG_SYNA_BUTTONS_SCROLL
			s7020_report_scroll(ts->input_dev,
			                        &ts->data[ts->f19.data_offset],
			                        ts->f19.points_supported,
			                        SCROLL_ORIENTATION);
#endif
		}

		if (ts->hasF30 && interrupt[ts->f30.interrupt_offset] & ts->f30.interrupt_mask) {
			s7020_report_buttons(ts->input_dev,
			                         &ts->data[ts->f30.data_offset],
		                                 ts->f30.points_supported, BTN_F30);
		}
		input_sync(ts->input_dev);
#ifdef CONFIG_DEBUG_S7020_FIRMWARE
		if ( 1 == flag_enable_ta){
			if(true == flag_first_point){
				getnstimeofday(&time_tp.time_first_end);
				flag_first_point = false ;								
			}
			
			if(true == flag_last_point){
				getnstimeofday(&time_tp.time_last_end);
				flag_last_point = false ;	
				flag_first_point = true ;  

				time_tp.time_first_start.tv_nsec /=  NSEC_PER_USEC;
				time_tp.time_first_end.tv_nsec /=  NSEC_PER_USEC;
				time_tp.time_last_end.tv_nsec /=  NSEC_PER_USEC;	
			}
		}
#endif	
	}

	 /* if chip report exception detectde power up reset touchscreent */
	 if( 0x03 ==  (i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_s7020_tm1771_data0) & 0x07)){
		if (i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_s7020_tm1771_data0+1) & 0x02){
			printk(KERN_ERR "%s: s7020 chip detect exception and will power up reset it\n", __func__);
			ts->chip_poweron_reset() ; 
		}
	 }
	
	if (ts->use_irq){
		enable_irq(ts->client->irq);
    }
    /* disable power reset after process point report */
	power_reset_enable = 1 ;
	if(debug_level >= 3)
		printk("%s:exit s7020 work func\n",__func__);
}

static enum hrtimer_restart s7020_timer_reset_func(struct hrtimer *timer)
{
	struct s7020 *ts = container_of(timer, \
					struct s7020, timer_reset);

	queue_work(s7020_wq_reset, &ts->work_reset);

	hrtimer_start(&ts->timer_reset, ktime_set(2, 0), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart s7020_timer_func(struct hrtimer *timer)
{
	struct s7020 *ts = container_of(timer, \
					struct s7020, timer);

	queue_work(s7020_wq, &ts->work);

	hrtimer_start(&ts->timer, ktime_set(0, 12 * NSEC_PER_MSEC), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

irqreturn_t s7020_irq_handler(int irq, void *dev_id)
{
	struct s7020 *ts = dev_id;
	int ret ;
#ifdef CONFIG_DEBUG_S7020_FIRMWARE
	if (1 == flag_enable_ta){
		sensor_report_num ++ ;
		if(true == flag_first_point){
			sensor_report_num = 1 ;
			getnstimeofday(&time_tp.time_first_start);
		}
	}
#endif
	disable_irq_nosync(ts->client->irq);
	ret = queue_work(s7020_wq, &ts->work);
     if(debug_level >= 3)
		printk("%s:irq handler ret=%d\n",__func__,ret);
	return IRQ_HANDLED;
}

static void s7020_enable(struct s7020 *ts)
{
	if (ts->use_irq)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	ts->enable = 1;
}

static void s7020_disable(struct s7020 *ts)
{
	if (ts->use_irq)
		disable_irq_nosync(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);
	
    cancel_work_sync(&ts->work_reset);
    cancel_work_sync(&ts->work);
	ts->enable = 0;
}

static ssize_t s7020_enable_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
	struct s7020 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->enable);
}

static ssize_t s7020_enable_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
	struct s7020 *ts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	val = !!val;

	if (val != ts->enable) {
		if (val)
			s7020_enable(ts);
		else
			s7020_disable(ts);
	}

	return count;
}

static int s7020_attn_clear(struct s7020 *ts)
{
	int ret = 0;
	__u8 attn_reg = f01_rmi_data1;
    __u8 data = 0x00;
    struct i2c_msg fd_i2c_msg[2];

	fd_i2c_msg[0].addr = ts->client->addr;
	fd_i2c_msg[0].flags = 0;
	fd_i2c_msg[0].buf = &attn_reg;
	fd_i2c_msg[0].len = 1;

	fd_i2c_msg[1].addr = ts->client->addr;
	fd_i2c_msg[1].flags = I2C_M_RD;
	fd_i2c_msg[1].buf = (__u8 *)(&data);
	fd_i2c_msg[1].len = 1;

	ret = i2c_transfer(ts->client->adapter, fd_i2c_msg, 2);
	if (ret < 0) {
	    printk(KERN_ERR "s7020_attn_clear: init attn fail\n");
	    return 0;
	}
	return 1;
}

static struct device_attribute s7020_dev_attr_enable = {
	   .attr = {.name = "s7020_dev_attr", .mode = 0664},
       .show = s7020_enable_show,
       .store = s7020_enable_store
};

static int s7020_probe(struct i2c_client *client, 
    							const struct i2c_device_id *id)
{
	int i;
	int ret = 0;

	struct s7020 *ts;
    struct kobject *kobj = NULL;
	
    printk(KERN_ERR "s7020 device %s at $%02X...\n", client->name, client->addr);

	g_client = client;  
		
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}


	ts = (struct s7020 *)client->dev.platform_data; 
	INIT_WORK(&ts->work, s7020_work_func);
	INIT_WORK(&ts->work_reset, s7020_work_reset_func);
    ts->client = client;
	ts->client ->adapter->retries = 3 ;
	i2c_set_clientdata(client, ts);
    
    if (ts->init_platform_hw) {
		if ((ts->init_platform_hw()) < 0)
	        goto init_platform_failed;
    }
    
    mdelay(50); 

	ret = s7020_attn_clear(ts);
	if (!ret) 
		goto err_pdt_read_failed;

	ret = s7020_read_pdt(ts);
	if (ret <= 0) {
		if (ret == 0)
			printk(KERN_ERR "Empty PDT\n");

		printk(KERN_ERR "Error identifying device (%d)\n", ret);
		ret = -ENODEV;
		goto err_pdt_read_failed;
	}
	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		printk(KERN_ERR "failed to allocate input device.\n");
		ret = -EBUSY;
		goto err_alloc_dev_failed;
	}

    /* begin: added by huangzhikui for scaling axis 2010/12/25 */
    /* convert touchscreen coordinator to coincide with LCD coordinator */
    /* get tsp at (0,0) */
	ts->input_dev->name = "s7020";
	ts->input_dev->phys = client->name;

	set_bit(EV_ABS, ts->input_dev->evbit);
//	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);


   	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
   	set_bit(KEY_BACK, ts->input_dev->keybit);

	ts->x_max = TOUCH_LCD_X_MAX;
	ts->y_max = TOUCH_LCD_Y_MAX;
	if (ts->hasF11) {
		for (i = 0; i < ts->f11.points_supported; ++i) {

#ifdef CONFIG_SYNA_MT

			/* begin: added by z00168965 for multi-touch */
			input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->x_max, 0, 0);
			input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->y_max, 0, 0);
			/* end: added by z00168965 for multi-touch */

            input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xF, 0, 0);
			input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 0xF, 0, 0);
			input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 0xFF, 0, 0);
#endif

#ifdef CONFIG_SYNA_MULTIFINGER
			/*  multiple fingers for software built prior to 2.6.31 - uses non-standard input.h file. */
			input_set_abs_params(ts->input_dev, ABS_X_FINGER(i), 0, ts->f11_max_x, 0, 0);
			input_set_abs_params(ts->input_dev, ABS_Y_FINGER(i), 0, ts->f11_max_y, 0, 0);
			input_set_abs_params(ts->input_dev, ABS_Z_FINGER(i), 0, 0xFF, 0, 0);
#endif
		}
        
		if (ts->hasEgrPalmDetect)
			set_bit(BTN_DEAD, ts->input_dev->keybit);
		if (ts->hasEgrFlick) {
			set_bit(REL_X, ts->input_dev->keybit);
			set_bit(REL_Y, ts->input_dev->keybit);
		}
        
		if (ts->hasEgrSingleTap)
			set_bit(BTN_TOUCH, ts->input_dev->keybit);
		if (ts->hasEgrDoubleTap)
			set_bit(BTN_TOOL_DOUBLETAP, ts->input_dev->keybit);
	}
    
	if (ts->hasF19) {
		set_bit(BTN_DEAD, ts->input_dev->keybit);
#ifdef CONFIG_SYNA_BUTTONS
		/* F19 does not (currently) report ABS_X but setting maximum X is a convenient way to indicate number of buttons */
		input_set_abs_params(ts->input_dev, ABS_X, 0, ts->f19.points_supported, 0, 0);
		for (i = 0; i < ts->f19.points_supported; ++i) {
			set_bit(BTN_F19 + i, ts->input_dev->keybit);
		}
#endif

#ifdef CONFIG_SYNA_BUTTONS_SCROLL
		set_bit(EV_REL, ts->input_dev->evbit);
		set_bit(SCROLL_ORIENTATION, ts->input_dev->relbit);
#endif
	}
    
	if (ts->hasF30) {
		for (i = 0; i < ts->f30.points_supported; ++i) {
			set_bit(BTN_F30 + i, ts->input_dev->keybit);
		}
	}

	if (client->irq) {
		printk(KERN_INFO "Requesting IRQ...\n");
		ret = request_irq(client->irq, s7020_irq_handler,
				IRQF_TRIGGER_LOW, client->name, ts);

		if(ret) {
			printk(KERN_ERR "Failed to request IRQ!ret = %d\n", ret);          		
		}else {
			printk(KERN_INFO "Set IRQ Success!\n");
            		ts->use_irq = 1;
		}

	}

	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = s7020_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

	hrtimer_init(&ts->timer_reset, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->timer_reset.function = s7020_timer_reset_func;
	/* set 2s time out */
	hrtimer_start(&ts->timer_reset, ktime_set(2, 0), HRTIMER_MODE_REL);
	/*
	 * Device will be /dev/input/event#
	 * For named device files, use udev
	 */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "s7020_probe: Unable to register %s"
				"input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	} else {
		printk("s7020 input device registered\n");
	}

	ts->enable = 1;
	dev_set_drvdata(&ts->input_dev->dev, ts);

	if (sysfs_create_file(&ts->input_dev->dev.kobj, &s7020_dev_attr_enable.attr) < 0)
		printk("failed to create sysfs file for input device\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = s7020_early_suspend;
	ts->early_suspend.resume = s7020_late_resume;
 	register_early_suspend(&ts->early_suspend);
#endif

	kobj = kobject_create_and_add("cap_touchscreen", NULL);
  	if (kobj == NULL) {	
		printk(KERN_ERR "kobject_create_and_add error\n" );
		ret = -1;
		goto err_input_register_device_failed;
	}
  	if (sysfs_create_group(kobj, &cap_touchscreen_defattr_group)) {
		kobject_put(kobj);
		printk(KERN_ERR "sysfs_create_group error\n" );
		ret = -1;
		goto err_input_register_device_failed;
	}

    g_tm1771_dect_flag = 1;

#ifdef CONFIG_UPDATE_S7020_FIRMWARE  
	ts_firmware_file();
    s7020_tm1771_read_PDT(g_client);
#endif

#ifdef CONFIG_DEBUG_S7020_FIRMWARE
	flag_first_point = true ;
	flag_last_point = false ;
	flag_enable_ta = 0 ;
#endif
   	return 0;

err_input_register_device_failed:
	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
	}
	hrtimer_cancel(&ts->timer_reset);
	input_free_device(ts->input_dev);

err_alloc_dev_failed:
err_pdt_read_failed:
    if (ts->exit_platform_hw)
        ts->exit_platform_hw();
init_platform_failed:
err_check_functionality_failed:
	return ret;
}

static int s7020_remove(struct i2c_client *client)
{
	struct s7020 *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);

	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);

	hrtimer_cancel(&ts->timer_reset);
	
	input_unregister_device(ts->input_dev);
   
    if (ts->exit_platform_hw)
		ts->exit_platform_hw();
    
	kfree(ts);
    
	return 0;
}

static int s7020_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct s7020 *ts = i2c_get_clientdata(client);
    int ret=0;
	
    hrtimer_cancel(&ts->timer_reset);
	/* disable power reset when system suspend */
	power_reset_enable = 0 ;

    /* Enter sleep mode */
    ret = i2c_smbus_read_byte_data(client,f01_rmi_ctrl0);
    ret = (ret | 1) & (~(1 << 1)) ;
    i2c_smbus_write_byte_data(client,f01_rmi_ctrl0,ret);

	s7020_attn_clear(ts);
    s7020_disable(ts);
    if (ts->use_irq)
        free_irq(client->irq, ts);
        if(0 == is_upgrade_firmware){
          	ts->chip_poweroff() ; 
	   		msm_gpiomux_put(GPIO_3V3_EN);
          	msm_gpiomux_put(GPIO_CTP_INT);   
	   		msm_gpiomux_put(GPIO_CTP_RESET);  
       	}
		ts_update_pen_state(ts, 0, 0, 0, 0, 0);    
	return 0;
}

static int s7020_resume(struct i2c_client *client)
{
	struct s7020 *ts = i2c_get_clientdata(client);
    int ret=0;
    if(0 == is_upgrade_firmware){
    	/* since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       	 * will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
        poweron_touchscreen();
	 }
	if (client->irq) {
		printk(KERN_INFO "Requesting IRQ...\n");
		ret = request_irq(client->irq, s7020_irq_handler,
				IRQF_TRIGGER_LOW,  client->name, ts);

		if(ret) {
			printk(KERN_ERR "Failed to request IRQ!ret = %d\n", ret);          		
		}else {
			printk(KERN_INFO "Set IRQ Success!\n");
            		ts->use_irq = 1;
		}

	}
    s7020_attn_clear(ts);
  	
    /* Enter normal mode */
   	ret = i2c_smbus_read_byte_data(client,f01_rmi_ctrl0);
    ret &=  ~(0x03);
    i2c_smbus_write_byte_data(client,f01_rmi_ctrl0,ret);
   
  	/* enable power reset when system resume */
	power_reset_enable = 1 ;
	hrtimer_start(&ts->timer_reset, ktime_set(2, 0), HRTIMER_MODE_REL);
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void s7020_early_suspend(struct early_suspend *h)
{
	struct s7020 *ts;
	ts = container_of(h, struct s7020, early_suspend);
	s7020_suspend(ts->client, PMSG_SUSPEND);
}

static void s7020_late_resume(struct early_suspend *h)
{
	struct s7020 *ts;
	ts = container_of(h, struct s7020, early_suspend);
	s7020_resume(ts->client);
}
#endif

static struct i2c_device_id s7020_id[]={
	{"s7020",0},
	{},	
};

static struct i2c_driver s7020_driver = {
	.probe		= s7020_probe,
	.remove		= s7020_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= s7020_suspend,
	.resume		= s7020_resume,
#endif
	.id_table	= s7020_id,
	.driver = {
		.name	= "s7020",
	},
};

static int __devinit s7020_init(void)
{
	s7020_wq = create_singlethread_workqueue("s7020_wq");
	if (!s7020_wq) {
		printk(KERN_ERR "Could not create work queue s7020_wq: no memory");
		return -ENOMEM;
	}

	s7020_wq_reset = create_singlethread_workqueue("s7020_wq_reset");
	if (!s7020_wq_reset) {
		printk(KERN_ERR "Could not create work queue s7020_wq_reset: no memory");
		return -ENOMEM;
	}

	return i2c_add_driver(&s7020_driver);
}

static void __exit s7020_exit(void)
{
	i2c_del_driver(&s7020_driver);

	if (s7020_wq)
		destroy_workqueue(s7020_wq);
	if (s7020_wq_reset)
		destroy_workqueue(s7020_wq_reset);
}

module_init(s7020_init);
module_exit(s7020_exit);

MODULE_DESCRIPTION("s7020 Driver");
MODULE_LICENSE("GPL");
