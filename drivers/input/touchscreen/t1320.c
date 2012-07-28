/* drivers/input/keyboard/t1320.c
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
#include <linux/t1320.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#ifdef CONFIG_UPDATE_T1320_FIRMWARE 
#include <linux/uaccess.h> 
#include <linux/vmalloc.h>
#include <mach/msm_rpcrouter.h>
#endif
#include <linux/slab.h>

#define BTN_F19 BTN_0
#define BTN_F30 BTN_0
#define SCROLL_ORIENTATION REL_Y

/*maxium power up reset times*/
#define MAX_POWERRESET_NUM 10 
/*enable power up reset*/
static int power_reset_enable = 1 ;
/*current power up reset times*/
static int power_reset_cnt = 0 ;
static struct workqueue_struct *t1320_wq_reset;
#define GPIO_3V3_EN             (130)  
#define GPIO_CTP_INT   			(140)
#define GPIO_CTP_RESET       	(139)
extern int msm_gpiomux_put(unsigned gpio);
extern int msm_gpiomux_get(unsigned gpio);
static int is_upgrade_firmware = 0 ;
void poweron_touchscreen(void);
static struct workqueue_struct *t1320_wq;

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

struct t1320_function_descriptor {
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


/* begin: added by huangzhikui for scaling axis 2010/12/25 */

#define TOUCH_LCD_X_MAX	3015
#define TOUCH_LCD_Y_MAX	1892


/* end: added by huangzhikui for scaling axis 2010/12/25 */

/* start: added by liyaobing 00169718 for MMI test 20110105 */
static unsigned char g_tm1771_dect_flag = 0;
/* end: added by liyaobing 00169718 for MMI test 20110105 */

/* begin: added by liyaobing 2011/1/6 */
//static unsigned char finger_num=0;
/* end: added by liyaobing 2011/1/6 */

/* define in platform/board file(s) */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void t1320_early_suspend(struct early_suspend *h);
static void t1320_late_resume(struct early_suspend *h);
#endif
static int t1320_attn_clear(struct t1320 *ts);

#ifdef CONFIG_UPDATE_T1320_FIRMWARE 
static struct update_firmware_addr {
	char f01_t1320_tm1771_cmd0 ;
	char f01_t1320_tm1771_query0 ;
	char f01_t1320_tm1771_data0 ;
	char f34_t1320_tm1771_query0 ;
	char f34_t1320_tm1771_query3 ;
	char f34_t1320_tm1771_query5 ;
	char f34_t1320_tm1771_query7 ;
	char f34_t1320_tm1771_data0 ;
	char f34_t1320_tm1771_data2 ;
	char f34_t1320_tm1771_data3	;
} update_firmware_addr ;
#define  Manufacturer_ID  0x01
/* start: added by liyaobing 00169718 for firmware update download 20110118 */
#define TOUCHSCREEN_TYPE "t1320_tm1828"
/* end: added by liyaobing 00169718 for firmware update download 20110118 */
#ifdef CONFIG_DEBUG_T1320_FIRMWARE 
#define FLAG_RR 			0x00
#define FLAG_WR 		0x01
#define FLAG_RPX 		0x02
#define FLAG_RPY 		0x03
#define FLAG_RNIT 		0x04
#define FLAG_HRST 		0x05
#define FLAG_SRST 		0x06
#define FLAG_EINT 		0x07
#define FLAG_DINT 		0x08
/*tp file struct(bytes):
	0:		RFLAG_ENABLE_TA
	1~2:	RFLAG_REPORT_NUM
	3~4:	RFLAG_RESEVER
	5+LAYER_PTHASE_NUM*LAYER_PHASE_DATA_NUM ~ 5+((LAYER_PTHASE_NUM+1)*LAYER_PHASE_DATA_NUM-1): tp timing data
*/
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

char time_ta[MAX_DATA_NUM]={0};
struct time_tp {
	struct timespec  time_first_start; /* first point start time of one action phase*/
	struct timespec  time_first_end;   /* first point end time of one action phase*/
	struct timespec  time_last_end;   /* last point end time of one action phase*/
};
struct time_tp time_tp ;

bool  flag_first_point;	 /* flag of whether it is first point */
bool  flag_last_point;	 /* flag of whether it is last point */
char flag_enable_ta = false ;
unsigned short  sensor_report_num = 0 ;

irqreturn_t t1320_irq_handler(int irq, void *dev_id);
static void t1320_disable(struct t1320 *ts);
static void t1320_enable(struct t1320 *ts);
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

/* start: added by liyaobing 00169718 for MMI test 20110105 */ 
static ssize_t cap_touchscreen_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
 {
 	  return sprintf(buf, "%d", g_tm1771_dect_flag);		  
 }

// Define the device attributes 
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
 /* end: added by liyaobing 00169718 for MMI test 20110105 */ 

#ifdef CONFIG_UPDATE_T1320_FIRMWARE 
 struct T1320_TM1771_FDT{
   unsigned char m_QueryBase;
   unsigned char m_CommandBase;
   unsigned char m_ControlBase;
   unsigned char m_DataBase;
   unsigned char m_IntSourceCount;
   unsigned char m_ID;
 };
 
 static int t1320_tm1771_read_PDT(struct i2c_client *client)
 {
     // Read config data
     struct T1320_TM1771_FDT temp_buf;
     struct T1320_TM1771_FDT m_PdtF34Flash;
     struct T1320_TM1771_FDT m_PdtF01Common;
     struct i2c_msg msg[2];
     unsigned short start_addr; 
 		 int ret=0;
 
     memset(&m_PdtF34Flash,0,sizeof(struct T1320_TM1771_FDT));
     memset(&m_PdtF01Common,0,sizeof(struct T1320_TM1771_FDT));
 
     for(start_addr = 0xe9; start_addr > 10; start_addr -= sizeof(struct T1320_TM1771_FDT)){
         msg[0].addr = client->addr;
         msg[0].flags = 0;
         msg[0].len = 1;
         msg[0].buf = (unsigned char *)&start_addr;
         msg[1].addr = client->addr;
         msg[1].flags = I2C_M_RD;
         msg[1].len = sizeof(struct T1320_TM1771_FDT);
         msg[1].buf = (unsigned char *)&temp_buf;
         if(i2c_transfer(client->adapter, msg, 2) < 0){
             return -1;
         }
 
         if(temp_buf.m_ID == 0x34){
             memcpy(&m_PdtF34Flash,&temp_buf,sizeof(struct T1320_TM1771_FDT ));
             update_firmware_addr.f34_t1320_tm1771_query0 = m_PdtF34Flash.m_QueryBase ;
             update_firmware_addr.f34_t1320_tm1771_query3 = m_PdtF34Flash.m_QueryBase+3 ;
             update_firmware_addr.f34_t1320_tm1771_query5 = m_PdtF34Flash.m_QueryBase+5 ;
             update_firmware_addr.f34_t1320_tm1771_query7 = m_PdtF34Flash.m_QueryBase+7 ;
             update_firmware_addr.f34_t1320_tm1771_data0 = m_PdtF34Flash.m_DataBase ;
             update_firmware_addr.f34_t1320_tm1771_data2 = m_PdtF34Flash.m_DataBase+2 ;
             
             ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_t1320_tm1771_query3);             
             update_firmware_addr.f34_t1320_tm1771_data3 = update_firmware_addr.f34_t1320_tm1771_data2+ret ;
         }else if(temp_buf.m_ID == 0x01){
             memcpy(&m_PdtF01Common,&temp_buf,sizeof(struct T1320_TM1771_FDT ));
             update_firmware_addr.f01_t1320_tm1771_cmd0 = m_PdtF01Common.m_CommandBase ;
             update_firmware_addr.f01_t1320_tm1771_query0 = m_PdtF01Common.m_QueryBase ;
             update_firmware_addr.f01_t1320_tm1771_data0 = m_PdtF01Common.m_DataBase ;
         }else if (temp_buf.m_ID == 0){      //end of PDT
             break;
         }
       }
     if((m_PdtF01Common.m_CommandBase != update_firmware_addr.f01_t1320_tm1771_cmd0) || (m_PdtF34Flash.m_QueryBase != update_firmware_addr.f34_t1320_tm1771_query0)){
         return -1;
     }
 
     return 0;
 
 } 
 
 //to be improved .......
 int t1320_tm1771_wait_attn(struct i2c_client * client,int udleay)
 {
     int loop_count=0;
     int ret=0;
 
     do{
         mdelay(udleay);
         ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_t1320_tm1771_data3);
         // Clear the attention assertion by reading the interrupt status register
         i2c_smbus_read_byte_data(client,update_firmware_addr.f01_t1320_tm1771_data0 + 1);
     }while(loop_count++ < 0x10 && (ret != 0x80));
 
     if(loop_count >= 0x10){
         return -1;
     }
     return 0;
 }
 
 int t1320_tm1771_disable_program(struct i2c_client *client)
 {
     unsigned char cdata; 
     unsigned int loop_count=0;
   
     // Issue a reset command
     i2c_smbus_write_byte_data(client,update_firmware_addr.f01_t1320_tm1771_cmd0,0x01);
     
     // Wait for ATTN to be asserted to see if device is in idle state
     t1320_tm1771_wait_attn(client,20);
 
     // Read F01 Status flash prog, ensure the 6th bit is '0'
     do{
         cdata = i2c_smbus_read_byte_data(client,update_firmware_addr.f01_t1320_tm1771_data0);
         udelay(2);
     } while(((cdata & 0x40) != 0) && (loop_count++ < 10));
 
     //Rescan the Page Description Table
     return t1320_tm1771_read_PDT(client);
 }
 
 static int t1320_tm1771_enable_program(struct i2c_client *client)
 {
     unsigned short bootloader_id = 0 ;
     int ret = -1;
      // Read and write bootload ID
     bootloader_id = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query0);
     i2c_smbus_write_word_data(client,update_firmware_addr.f34_t1320_tm1771_data2,bootloader_id);
 
       // Issue Enable flash command
     if(i2c_smbus_write_byte_data(client, update_firmware_addr.f34_t1320_tm1771_data3, 0x0F) < 0){
         printk("t1320_tm1771 enter flash mode error\n");
         return -1;
     }
     ret = t1320_tm1771_wait_attn(client,12);
 
     //Rescan the Page Description Table
     t1320_tm1771_read_PDT(client);
     return ret;
 }
 
 static unsigned long ExtractLongFromHeader(const unsigned char* SynaImage) 
 {
     return((unsigned long)SynaImage[0] +
          (unsigned long)SynaImage[1]*0x100 +
          (unsigned long)SynaImage[2]*0x10000 +
          (unsigned long)SynaImage[3]*0x1000000);
 }
 
 static int t1320_tm1771_check_firmware(struct i2c_client *client,const unsigned char *pgm_data)
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
  
     UI_block_count  = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query5);
     fw_block_size = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query3);
     CONF_block_count = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query7);
     bootloader_id = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query0);
     
       return (m_firmwareImgVersion != 0 || bootloader_id == m_bootloadImgID) ? 0 : -1;
 
 }
 
 
 static int t1320_tm1771_write_image(struct i2c_client *client,unsigned char type_cmd,const unsigned char *pgm_data)
 {
     unsigned short block_size;
     unsigned short img_blocks;
     unsigned short block_index;
     const unsigned char * p_data;
     int i;
     
     block_size = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query3);
     switch(type_cmd ){
         case 0x02:
             img_blocks = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query5);   //UI Firmware
             break;
         case 0x06:
             img_blocks = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query7);   //Configure 
             break;
         default:
             printk("image type error\n");
             goto error;
     }
 
     p_data = pgm_data;
     for(block_index = 0; block_index < img_blocks; ++block_index){
         printk("."); 
         // Write Block Number
         if(i2c_smbus_write_word_data(client, update_firmware_addr.f34_t1320_tm1771_data0,block_index) < 0){
             printk("write block number error\n");
             goto error;
         }
 
         for(i=0;i<block_size;i++){
             if(i2c_smbus_write_byte_data(client, update_firmware_addr.f34_t1320_tm1771_data2+i, *(p_data+i)) < 0){
                 printk("t1320_tm1771_write_image: block %d data 0x%x error\n",block_index,*p_data);
                 goto error;
             }
             udelay(15);
         }
         p_data += block_size;   
 
         // Issue Write Firmware or configuration Block command
         if(i2c_smbus_write_word_data(client, update_firmware_addr.f34_t1320_tm1771_data3, type_cmd) < 0){
             printk("issue write command error\n");
             goto error;
         }
 
         // Wait ATTN. Read Flash Command register and check error
         if(t1320_tm1771_wait_attn(client,5) != 0)
             goto error;
     }
 	 printk("\n"); 
     return 0;
 error:
     return -1;
 }
 
 
 static int t1320_tm1771_program_configuration(struct i2c_client *client,const unsigned char *pgm_data )
 {
     int ret;
     unsigned short block_size;
     unsigned short ui_blocks;
 
     block_size = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query3);
     ui_blocks = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query5);    //UI Firmware
 
     if(t1320_tm1771_write_image(client, 0x06,pgm_data+ui_blocks*block_size ) < 0){
         printk("write configure image error\n");
         return -1;
     }
     ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_t1320_tm1771_data3);
     return ((ret & 0xF0) == 0x80 ? 0 : ret);
 }
 
 static int t1320_tm1771_program_firmware(struct i2c_client *client,const unsigned char *pgm_data)
 {
     int ret=0;
     unsigned short bootloader_id;
 
     //read and write back bootloader ID
     bootloader_id = i2c_smbus_read_word_data(client,update_firmware_addr.f34_t1320_tm1771_query0);
     i2c_smbus_write_word_data(client,update_firmware_addr.f34_t1320_tm1771_data2, bootloader_id );
     //issue erase commander
     if(i2c_smbus_write_byte_data(client, update_firmware_addr.f34_t1320_tm1771_data3, 0x03) < 0){
         printk("t1320_tm1771_program_firmware error, erase firmware error \n");
         return -1;
     }
     t1320_tm1771_wait_attn(client,300);
 
     //check status
     if((ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_t1320_tm1771_data3)) != 0x80){
		 printk("check firmware status error!\n");
         return -1;
     }
 
     //write firmware
     if( t1320_tm1771_write_image(client,0x02,pgm_data) <0 ){
         printk("write UI firmware error!\n");
         return -1;
     }
 
     ret = i2c_smbus_read_byte_data(client,update_firmware_addr.f34_t1320_tm1771_data3);
     return ((ret & 0xF0) == 0x80 ? 0 : ret);
 }
 
 static int t1320_tm1771_download(struct i2c_client *client,const unsigned char *pgm_data)
 {
     int ret;
 
     ret = t1320_tm1771_read_PDT(client);
     if(ret != 0){
         printk("t1320_tm1771 page func check error\n");
         return -1;
     }
		 printk("t1320_tm1771 firmware version is %x\n",i2c_smbus_read_byte_data(client,update_firmware_addr.f01_t1320_tm1771_query0+3));
     ret = t1320_tm1771_enable_program(client);
     if( ret != 0){
         printk("%s:%d:t1320_tm1771 enable program error,return...\n",__FUNCTION__,__LINE__);
         goto error;
     }
 
     ret = t1320_tm1771_check_firmware(client,pgm_data);
     if( ret != 0){
         printk("%s:%d:t1320_tm1771 check firmware error,return...\n",__FUNCTION__,__LINE__);
         goto error;
     }
 
     ret = t1320_tm1771_program_firmware(client, pgm_data + 0x100);
     if( ret != 0){
         printk("%s:%d:t1320_tm1771 program firmware error,return...",__FUNCTION__,__LINE__);
         goto error;
     }
 
     ret = t1320_tm1771_program_configuration(client, pgm_data +  0x100);
     if( ret != 0){
         printk("%s:%d:t1320_tm1771 program configuration error,return...",__FUNCTION__,__LINE__);
         goto error;
     }
     return t1320_tm1771_disable_program(client);
 
 error:
     t1320_tm1771_disable_program(client);
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
     printk("t1320_tm1771 image file is %s and data size is %d Bytes \n",file,length);
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
 
     ret = t1320_tm1771_download(client,buf);
 
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

#ifdef CONFIG_DEBUG_T1320_FIRMWARE 
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

void poweron_touchscreen(void){
    struct t1320  * ts ;
    ts = (struct t1320 *)g_client->dev.platform_data; 
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
    if(i2c_smbus_read_byte_data(g_client,0x00) < 0){
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
	
#ifdef CONFIG_DEBUG_T1320_FIRMWARE
 static ssize_t ts_debug_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
	memcpy(time_ta+RFLAG_LAYER_PHASE_START,&time_tp,LAYER_PHASE_DATA_NUM);
	memcpy(time_ta+RFLAG_REPORT_NUM,&sensor_report_num,sizeof(unsigned short));	
	memcpy(buf,time_ta,MAX_DATA_NUM); 
	return MAX_DATA_NUM ;
 }
 
 static ssize_t ts_debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
     struct t1320  * ts ;
     int ret = 0 ;
     unsigned short bootloader_id;
     ts = (struct t1320 *)g_client->dev.platform_data; 
 
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
     		ret =  i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_t1320_tm1771_cmd0);
		ret |= 0x01  ;
		 if(i2c_smbus_write_byte_data(g_client, update_firmware_addr.f01_t1320_tm1771_cmd0,ret) < 0){
	                 return 0 ;
	        }
		 else
		 	return 1 ;
     }else if(buf[0] == FLAG_EINT){
            /*if not interrupts modem ,return fail*/
           if (0 == ts->use_irq)
	        return 0 ;
            /*register msm8660 interrupts*/
	     ret = request_irq(g_client->irq, t1320_irq_handler,IRQF_TRIGGER_LOW,  g_client->name, ts);
	     if(ret) {
	         printk(KERN_ERR "Failed to request IRQ!ret = %d\n", ret); 
		  return 0 ;
	     }
		 
	    /*clear t1320 interrupt flag*/
	    t1320_attn_clear(ts);
	    /*t1320 leave sleep mode*/
	    ret = i2c_smbus_read_byte_data(g_client,f01_rmi_ctrl0);
	    ret &=  ~(0x03);
	    i2c_smbus_write_byte_data(g_client,f01_rmi_ctrl0,ret);
	    return 1 ;
     }else if(buf[0] == FLAG_DINT){
           /*if not interrupts modem ,return fail*/
           if (0 == ts->use_irq )
	        return 0 ;
	     /*t1320 enter sleep mode*/
	    ret = i2c_smbus_read_byte_data(g_client,f01_rmi_ctrl0);
	    ret = (ret | 1) & (~(1 << 1)) ;
	    i2c_smbus_write_byte_data(g_client,f01_rmi_ctrl0,ret);
	    /*clear t1320 interrupt flag*/
	    t1320_attn_clear(ts);
	   /*disable msm8660 interrupts*/
	    t1320_disable(ts);
	   /*unregister msm8660 interrupts*/
	    free_irq(g_client->irq, ts);
	    return 1 ;
 }else if(buf[0] == FLAG_RTA){
       if(buf[1] == WFLAG_ENABLE_TA){
		flag_enable_ta = buf[2] ;
		time_ta[RFLAG_ENABLE_TA] = flag_enable_ta ;
		return 1 ;
      	}else if(buf[1]== LAYER_PTHASE_DRIVER){
		return 1 ;/*write of time_ta is at ts_debug_show(),so here return directly */
      }else if((buf[1]>= LAYER_PTHASE_EVENTHUB) &&  (buf[1] <= MAX_LAYER_PTHASE)){
   		memcpy(time_ta+RFLAG_LAYER_PHASE_START+buf[1]*LAYER_PHASE_DATA_NUM,buf+2,LAYER_PHASE_DATA_NUM);  
		return 1 ;
      } else
	 	return 0 ;
     }else  if(buf[0] == FLAG_PRST){
        /*enable power reset after execute power up debug command*/
        power_reset_enable = 1 ;
        return ts->chip_poweron_reset() ; 
    }else  if(buf[0] == FLAG_POFF){
        /*disable power reset after execute power off debug command*/
        power_reset_enable = 0 ;
        return ts->chip_poweroff() ; 
   }else  if(buf[0] == FLAG_EFW){
		  if(t1320_tm1771_enable_program(g_client) != 0){
		         printk("t1320_tm1771 page func check error\n");
		         return 0;
	       }
		  bootloader_id = i2c_smbus_read_word_data(g_client,update_firmware_addr.f34_t1320_tm1771_query0);
          i2c_smbus_write_word_data(g_client,update_firmware_addr.f34_t1320_tm1771_data2, bootloader_id );
     		//issue erase commander
     		if(i2c_smbus_write_byte_data(g_client, update_firmware_addr.f34_t1320_tm1771_data3, 0x03) < 0){
	         	printk("t1320_tm1771_program_firmware error, erase firmware error \n");
	         	return 0;
	      }
    		 t1320_tm1771_wait_attn(g_client,300);
	     //check status
	     if((ret = i2c_smbus_read_byte_data(g_client,update_firmware_addr.f34_t1320_tm1771_data3)) != 0x80){
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
       char  ret = -1;
	   
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
	poweron_touchscreen();
       ret = i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_t1320_tm1771_query0+3);
	return sprintf(buf, "%03d", ret) ;
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
      struct t1320  * ts ;
      int on = -1;
      ts = (struct t1320 *)g_client->dev.platform_data;     
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

    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
	poweron_touchscreen();
	
	for(i=0;i<F01_PRODUCTID_SIZE;i++)
       	tptype[i] = i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_t1320_tm1771_query0+F01_PRODUCTID_QUERY_OFFSET+i);

	return sprintf(buf, "%s", tptype) ;
 }
 
 static ssize_t firmware_tptype_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
	return 0;
 }
 /*
  * The "update_firmware" file where a static variable is read from and written to.
  */
 static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
 {
  /* start: modify by liyaobing 00169718 for firmware update download 20110118 */
     return sprintf(buf, "%s", TOUCHSCREEN_TYPE);	
  /* end: modify by liyaobing 00169718 for firmware update download 20110118 */    
 }
 
 static ssize_t update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 {
     int i, ret = -1;
     unsigned char path_image[255];
 
     printk("start t1320_tm1771 firmware update download\n");
 
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
             ret = i2c_smbus_read_byte_data(g_client, update_firmware_addr.f01_t1320_tm1771_query0);
             if (ret == Manufacturer_ID){
                 goto firmware_find_device;
             }
 
         }
         printk("Do not find t1320_tm1771 device\n");   
         return -1;
 
 firmware_find_device:
        /*disable power reset when firmware upgrade*/
        power_reset_enable = 0 ;
        is_upgrade_firmware = 1 ;
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
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
        /*enable power reset after firmware upgrade*/
        power_reset_enable = 1 ;
        is_upgrade_firmware = 0 ;
     return ret;
  }
 
#endif

static int t1320_read_pdt(struct t1320 *ts)
{
	int ret = 0;
	int nFd = 0;
	int interruptCount = 0;
	__u8 data_length;

	struct i2c_msg fd_i2c_msg[2];
	__u8 fd_reg;
	struct t1320_function_descriptor fd;

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
				ts->f11_has_gestures = (query[1] >> 5) & 1;
				ts->f11_has_relative = (query[1] >> 3) & 1;

				/* robert mccarthy fix for DroidBench touch problem, begin 2011/9/21 */
                i2c_smbus_write_byte_data(ts->client, fd.controlBase+2, 0x19);
                i2c_smbus_write_byte_data(ts->client, fd.controlBase+3, 0x19);
				/* robert mccarthy fix for DroidBench touch problem, end 2011/9/21 */

				egr = &query[7];

				ts->hasEgrPinch = egr[EGR_PINCH_REG] & EGR_PINCH;
				ts->hasEgrPress = egr[EGR_PRESS_REG] & EGR_PRESS;
				ts->hasEgrFlick = egr[EGR_FLICK_REG] & EGR_FLICK;
				ts->hasEgrEarlyTap = egr[EGR_EARLY_TAP_REG] & EGR_EARLY_TAP;
				ts->hasEgrDoubleTap = egr[EGR_DOUBLE_TAP_REG] & EGR_DOUBLE_TAP;
				ts->hasEgrTapAndHold = egr[EGR_TAP_AND_HOLD_REG] & EGR_TAP_AND_HOLD;
				ts->hasEgrSingleTap = egr[EGR_SINGLE_TAP_REG] & EGR_SINGLE_TAP;
				ts->hasEgrPalmDetect = egr[EGR_PALM_DETECT_REG] & EGR_PALM_DETECT;

				query_i2c_msg[0].buf = &fd.controlBase;
				ret = i2c_transfer(ts->client->adapter, query_i2c_msg, 2);
				if (ret < 0)
					printk(KERN_ERR "Error reading F11 control registers\n");

				query_i2c_msg[0].buf = &fd.queryBase;

				ts->f11_max_x = ((query[7] & 0x0f) * 0x100) | query[6];
				ts->f11_max_y = ((query[9] & 0x0f) * 0x100) | query[8];


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
#ifdef CONFIG_UPDATE_T1320_FIRMWARE
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

static void t1320_report_scroll(struct input_dev *dev,
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

static void t1320_report_buttons(struct input_dev *dev,
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

static void t1320_work_reset_func(struct work_struct *work)
{
	struct t1320  * ts ;
 	ts = (struct t1320 *)g_client->dev.platform_data; 

      /*if disable power reset,return directly*/
	if(0 == power_reset_enable)
		return ;
	if(i2c_smbus_read_byte_data(g_client,0x00) < 0){
		/*if i2c communication has problem, power up reset touchcreen chip */
		if(power_reset_cnt < MAX_POWERRESET_NUM){
			printk(KERN_ERR "%s:touchscreen i2c has problem and power up reset it\n", __func__);
			ts->chip_poweron_reset() ; 
			power_reset_cnt++;
		}
	}else{
		power_reset_cnt = 0 ;
	}
}

/**
 * Minimum sample count that should be collected before filtering begin.
 */
#define MIN_SAMPLE   5

/**
 * Maximum sample count that should be kept in history.
 */
#define SAMPLE_SIZE 10

#if SAMPLE_SIZE > MAX_SAMPLE
#error "SAMPLE_SIZE > MAX_SAMPLE"
#endif

#if MIN_SAMPLE > MAX_SAMPLE
#error "MIN_SAMPLE > MAX_SAMPLE"
#endif

/**
 * MediaPad screen is 1280x800 pixels, and the touchscreen controller
 * is 3015x1892. So you get 2.365 touchscreen unit for each screen pixel.
 */

/**
 * Sampled touches outside FILTER_RADIUS won't be used for filtering,
 * and will be discarded.
 */
#define FILTER_RADIUS 95 /* 40 pixels * 2.365 */

/**
 * How far should a touch move before it will be considered as a swipe.
 */
#define TOUCH_THRES   10 /* 4 pixels * 2.365 */

/**
 * Minimum distance between swipe.
 */
#define SWIPE_THRES    3 /* 1 pixel * 2.365 */

struct my_attribute {
	struct kobj_attribute attr;
	int *value;
	int (*validator)(int);
};

static ssize_t my_attribute_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t my_attribute_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

#define DECL_ATTR(_name, _default, _validator) \
	static int _name = _default; \
	static struct my_attribute _name##_attr = { \
		.attr.attr.name = __stringify(_name), \
		.attr.attr.mode = 0666, \
		.attr.show	= my_attribute_show, \
		.attr.store = my_attribute_store, \
		.value = &_name, \
		.validator = _validator, \
	}

static int is_positive(int val)
{
	return val >= 0;
}

static int is_valid_sample(int val)
{
	return val > 0 && val <= MAX_SAMPLE;
}

DECL_ATTR(filter_radius, FILTER_RADIUS, is_positive);
DECL_ATTR(touch_thres,   TOUCH_THRES,   is_positive);
DECL_ATTR(swipe_thres,   SWIPE_THRES,   is_positive);
DECL_ATTR(sample_size,   SAMPLE_SIZE,   is_valid_sample);
DECL_ATTR(min_sample,    MIN_SAMPLE,    is_valid_sample);

static inline void reset_finger(struct f11_finger_data *finger)
{
	finger->sample_index = 0;
	finger->sample_count = 0;
	finger->report_count = 0;
	finger->x_sum = 0;
	finger->y_sum = 0;
	finger->z_sum = 0;
}

static ssize_t my_attribute_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct my_attribute *my = container_of(attr, struct my_attribute, attr);
	return sprintf(buf, "%d\n", *my->value);
}

static ssize_t my_attribute_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct my_attribute *my = container_of(attr, struct my_attribute, attr);
	struct t1320 *ts = (struct t1320 *) g_client->dev.platform_data;
	int tmp, f;
	if (sscanf(buf, "%d", &tmp) != 1 || !my->validator(tmp))
		return -EINVAL;
	*my->value = tmp;

	if (min_sample > sample_size)
		min_sample = sample_size;

	for (f = 0; f < ts->f11.points_supported; ++f) {
		struct f11_finger_data *finger = &ts->f11_fingers[f];
		reset_finger(finger);
	}

	return strnlen(buf, count);
}

static int init_filter_sysfs(void)
{
	int i;
	struct kobject *kobj;
	struct my_attribute *attrs[] = {
		&filter_radius_attr,
		&touch_thres_attr,
		&swipe_thres_attr,
		&sample_size_attr,
		&min_sample_attr,
		0
	};
 
	kobj = kobject_create_and_add("t1320", NULL);
	if (kobj == NULL) {
		printk(KERN_ERR "kobject_create_and_add() error");
		return -1;
	}
	for (i = 0; attrs[i]; i++) {
		if (sysfs_create_file(kobj, &attrs[i]->attr.attr)) {
			kobject_put(kobj);
			printk("sysfs_create_file() error");
			return -1;
		}
	}
	return 0;
}

static void add_sample(struct f11_finger_data *finger, int status, int x, int y, int z, int wx, int wy)
{
	if (z == 0)
		status = 0;

	if (!finger->status && status)
		reset_finger(finger);
	if (finger->status != status)
		finger->dirty = 1;

	finger->status = status;
	if (status) {
		int i, dx, dy, dist;
		// Remove samples outside filter radius
		while (finger->sample_count > 0) {
			i = finger->sample_index - finger->sample_count;
			if (i < 0)
				i += sample_size;

			dx = x - finger->x[i];
			dy = y - finger->y[i];
			dist = dx * dx + dy * dy;
			if (dist <= filter_radius * filter_radius)
				break;

			finger->x_sum -= finger->x[i];
			finger->y_sum -= finger->y[i];
			finger->z_sum -= finger->z[i];
			finger->sample_count--;
			finger->dirty = 1;
		}

		i = finger->sample_index;
		finger->sample_index = i + 1 < sample_size ? i + 1 : 0;
		if (finger->sample_count < sample_size) {
			finger->sample_count++;
		} else {
			finger->x_sum -= finger->x[i];
			finger->y_sum -= finger->y[i];
			finger->z_sum -= finger->z[i];
		}

		finger->x[i] = x;
		finger->y[i] = y;
		finger->z[i] = z;

		finger->x_sum += x;
		finger->y_sum += y;
		finger->z_sum += z;

		i = finger->sample_count;
		finger->x_avg = finger->x_sum / i;
		finger->y_avg = finger->y_sum / i;
		finger->z_avg = finger->z_sum / i;

		if (finger->sample_count >= min_sample) {
			dx = finger->x_avg - finger->x_last;
			dy = finger->y_avg - finger->y_last;
			dist = dx * dx + dy * dy;
			if (finger->report_count == 1) {
				if (dist >= touch_thres * touch_thres)
					finger->dirty = 1;
			} else {
				if (dist >= swipe_thres * swipe_thres)
					finger->dirty = 1;
			}
		}
	}
}

static void report_finger(struct t1320 *ts, struct f11_finger_data *finger)
{
	finger->dirty = 0;
	if (finger->status) {
#ifdef CONFIG_SYNA_MT
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, finger->x_avg);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, finger->y_avg);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, VALUE_ABS_MT_TOUCH_MAJOR);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, VALUE_ABS_MT_TOUCH_MINOR);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, finger->z_avg / 2);
#endif

#ifdef CONFIG_SYNA_MULTIFINGER
		/* Report multiple fingers for software prior to 2.6.31 - not standard - uses special input.h */
		input_report_abs(ts->input_dev, ABS_X_FINGER(f), finger->x_avg);
		input_report_abs(ts->input_dev, ABS_Y_FINGER(f), finger->y_avg);
		input_report_abs(ts->input_dev, ABS_Z_FINGER(f), finger->z_avg);
#endif

		finger->x_last = finger->x_avg;
		finger->y_last = finger->y_avg;
		finger->z_last = finger->z_avg;

		finger->report_count++;
	}

#ifdef CONFIG_SYNA_MT
	input_mt_sync(ts->input_dev);
#endif
}

extern bool g_screen_touch_event;
static void t1320_work_func(struct work_struct *work)
{
	int ret;
	u32 z = 0;
	u32 wx = 0, wy = 0;
	u32 x = 0, y = 0;

	struct t1320 *ts = container_of(work,struct t1320, work);

	g_screen_touch_event = true;

	/*disable power reset when process point report*/
	power_reset_enable = 0 ;

	ret = i2c_transfer(ts->client->adapter, ts->data_i2c_msg, 2);

	if (ret < 0) {
		printk(KERN_ERR "%s: i2c_transfer failed\n", __func__);
	} else {
		__u8 *interrupt = &ts->data[ts->f01.data_offset + 1];
		if (ts->hasF11 && interrupt[ts->f11.interrupt_offset] & ts->f11.interrupt_mask) {
			__u8 *f11_data = &ts->data[ts->f11.data_offset];
			int f, dirty = 0;
			__u8 finger_status_reg = 0;
			__u8 fsr_len = (ts->f11.points_supported + 3) / 4;
			__u8 finger_status;

			for (f = 0; f < ts->f11.points_supported; ++f) {			
				struct f11_finger_data *finger = &ts->f11_fingers[f];
				__u8 reg = fsr_len + 5 * f;
				__u8 *finger_reg = &f11_data[reg];
				
				if (!(f & 3))
					finger_status_reg = f11_data[f >> 2];
				finger_status = (finger_status_reg >> ((f & 3) << 1)) & 3;

				x = (finger_reg[0] << 4) | (finger_reg[2] & 15);
				y = (finger_reg[1] << 4) | (finger_reg[2] >> 4);
				wx = finger_reg[3] & 15;
				wy = finger_reg[3] >> 4;
				z = finger_reg[4];

				add_sample(finger, finger_status, x, y, z, wx, wy);
				if (finger->dirty)
					dirty = 1;
			}

			if (dirty) {
				for (f = 0; f < ts->f11.points_supported; ++f) {
					struct f11_finger_data *finger = &ts->f11_fingers[f];
					report_finger(ts, finger);
				}
			}

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
			t1320_report_buttons(ts->input_dev,
					&ts->data[ts->f19.data_offset],
					ts->f19.points_supported, BTN_F19);
#endif

#ifdef  CONFIG_SYNA_BUTTONS_SCROLL
			t1320_report_scroll(ts->input_dev,
					&ts->data[ts->f19.data_offset],
					ts->f19.points_supported,
					SCROLL_ORIENTATION);
#endif
		}

		if (ts->hasF30 && interrupt[ts->f30.interrupt_offset] & ts->f30.interrupt_mask) {
			t1320_report_buttons(ts->input_dev,
					&ts->data[ts->f30.data_offset],
					ts->f30.points_supported, BTN_F30);
		}
		input_sync(ts->input_dev);
#ifdef CONFIG_DEBUG_T1320_FIRMWARE
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

	/*if chip report exception detectde power up reset touchscreent*/
	if( 0x03 ==  (i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_t1320_tm1771_data0) & 0x07)){
		if (i2c_smbus_read_byte_data(g_client,update_firmware_addr.f01_t1320_tm1771_data0+1) & 0x02){
			printk(KERN_ERR "%s: t1320 chip detect exception and will power up reset it\n", __func__);
			ts->chip_poweron_reset() ; 
		}
	}

	if (ts->use_irq){
		t1320_attn_clear(ts);
		enable_irq(ts->client->irq);

	}
	/*disable power reset after process point report*/
	power_reset_enable = 1 ;
	if(debug_level >= 3)
		printk("%s:exit t1320 work func\n",__func__);
}

static enum hrtimer_restart t1320_timer_reset_func(struct hrtimer *timer)
{
	struct t1320 *ts = container_of(timer, \
					struct t1320, timer_reset);

	queue_work(t1320_wq_reset, &ts->work_reset);

	hrtimer_start(&ts->timer_reset, ktime_set(2, 0), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart t1320_timer_func(struct hrtimer *timer)
{
	struct t1320 *ts = container_of(timer, \
					struct t1320, timer);

	queue_work(t1320_wq, &ts->work);

	hrtimer_start(&ts->timer, ktime_set(0, 12 * NSEC_PER_MSEC), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

irqreturn_t t1320_irq_handler(int irq, void *dev_id)
{
	struct t1320 *ts = dev_id;
	int ret ;
#ifdef CONFIG_DEBUG_T1320_FIRMWARE
	if (1 == flag_enable_ta){
		sensor_report_num ++ ;
		if(true == flag_first_point){
			sensor_report_num = 1 ;
			getnstimeofday(&time_tp.time_first_start);
		}
	}
#endif
	disable_irq_nosync(ts->client->irq);
	ret = queue_work(t1320_wq, &ts->work);
     if(debug_level >= 3)
		printk("%s:irq handler ret=%d\n",__func__,ret);
	return IRQ_HANDLED;
}

static void t1320_enable(struct t1320 *ts)
{
	if (ts->use_irq)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	ts->enable = 1;
}

static void t1320_disable(struct t1320 *ts)
{
	if (ts->use_irq)
		disable_irq_nosync(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);
	
	cancel_work_sync(&ts->work_reset);
	  cancel_work_sync(&ts->work);
	ts->enable = 0;
}

static ssize_t t1320_enable_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
	struct t1320 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->enable);
}

static ssize_t t1320_enable_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
	struct t1320 *ts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	val = !!val;

	if (val != ts->enable) {
		if (val)
			t1320_enable(ts);
		else
			t1320_disable(ts);
	}

	return count;
}

static int t1320_attn_clear(struct t1320 *ts)
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
	    printk(KERN_ERR "t1320_attn_clear: init attn fail\n");
	    return 0;
	}
	return 1;
}

static struct device_attribute t1320_dev_attr_enable = {
	   .attr = {.name = "t1320_dev_attr", .mode = 0664},
       .show = t1320_enable_show,
       .store = t1320_enable_store
};

static int t1320_probe(struct i2c_client *client, 
    							const struct i2c_device_id *id)
{
	int i;
	int ret = 0;

	struct t1320 *ts;
    /* start: added by liyaobing 00169718 for MMI test 20110105 */
    struct kobject *kobj = NULL;
    /* end: added by liyaobing 00169718 for MMI test 20110105 */
	printk(KERN_ERR "t1320 device %s at $%02X...\n", client->name, client->addr);

        g_client = client;  
		
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}


	ts = (struct t1320 *)client->dev.platform_data; 
	INIT_WORK(&ts->work, t1320_work_func);
	INIT_WORK(&ts->work_reset, t1320_work_reset_func);
	ts->client = client;
	ts->client ->adapter->retries = 3 ;
	i2c_set_clientdata(client, ts);
    
	if (ts->init_platform_hw) {
		if ((ts->init_platform_hw()) < 0)
	        goto init_platform_failed;
    }
    
    mdelay(50); 

	ret = t1320_attn_clear(ts);
	if (!ret) 
		goto err_pdt_read_failed;

	ret = t1320_read_pdt(ts);
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
	ts->input_dev->name = "t1320";
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
		ret = request_irq(client->irq, t1320_irq_handler,
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
		ts->timer.function = t1320_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	hrtimer_init(&ts->timer_reset, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->timer_reset.function = t1320_timer_reset_func;
	/*set 2s time out */
	hrtimer_start(&ts->timer_reset, ktime_set(2, 0), HRTIMER_MODE_REL);
	/*
	 * Device will be /dev/input/event#
	 * For named device files, use udev
	 */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "t1320_probe: Unable to register %s"
				"input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	} else {
		printk("t1320 input device registered\n");
	}


	ts->enable = 1;
	dev_set_drvdata(&ts->input_dev->dev, ts);

	if (sysfs_create_file(&ts->input_dev->dev.kobj, &t1320_dev_attr_enable.attr) < 0)
		printk("failed to create sysfs file for input device\n");

	#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = t1320_early_suspend;
	ts->early_suspend.resume = t1320_late_resume;
 	/* begin: add by liyaobing l00169718 20110210 for t1320 sleep mode */
    register_early_suspend(&ts->early_suspend);
    /* end: add by liyaobing l00169718 20110210 for t1320 sleep mode */
	#endif
    /* start: added by liyaobing 00169718 for MMI test 20110105 */
    kobj = kobject_create_and_add("cap_touchscreen", NULL);
  	if (kobj == NULL) {	
		printk(KERN_ERR "kobject_create_and_add error\n" );
		goto err_input_register_device_failed;
	}
  	if (sysfs_create_group(kobj, &cap_touchscreen_defattr_group)) {
		kobject_put(kobj);
		printk(KERN_ERR "sysfs_create_group error\n" );
		goto err_input_register_device_failed;
	}

    g_tm1771_dect_flag = 1;
    /* end: added by liyaobing 00169718 for MMI test 20110105 */

		if (init_filter_sysfs() != 0)
			goto err_input_register_device_failed;

#ifdef CONFIG_UPDATE_T1320_FIRMWARE  
         ts_firmware_file();
	 t1320_tm1771_read_PDT(g_client);
#endif
#ifdef CONFIG_DEBUG_T1320_FIRMWARE
	flag_first_point = true ;
	flag_last_point = false ;
	flag_enable_ta = 0 ;
#endif
	i2c_smbus_write_byte_data(client, f11_rmi_ctrl0 + 2, 0x3);
	i2c_smbus_write_byte_data(client, f11_rmi_ctrl0 + 3, 0x3);
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

static int t1320_remove(struct i2c_client *client)
{
struct t1320 *ts = i2c_get_clientdata(client);

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

static int t1320_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct t1320 *ts = i2c_get_clientdata(client);
    int ret=0;
	
	hrtimer_cancel(&ts->timer_reset);
	 /*disable power reset when system suspend */
	power_reset_enable = 0 ;

    /*Enter sleep mode*/
    ret = i2c_smbus_read_byte_data(client,f01_rmi_ctrl0);
    ret = (ret | 1) & (~(1 << 1)) ;
    i2c_smbus_write_byte_data(client,f01_rmi_ctrl0,ret);
    t1320_attn_clear(ts);
    t1320_disable(ts);
    if (ts->use_irq)
        free_irq(client->irq, ts);
        if(0 == is_upgrade_firmware){
          ts->chip_poweroff() ; 
	   msm_gpiomux_put(GPIO_3V3_EN);
          msm_gpiomux_put(GPIO_CTP_INT);   
	   msm_gpiomux_put(GPIO_CTP_RESET);  
       }
	return 0;
}

static int t1320_resume(struct i2c_client *client)
{
	struct t1320 *ts = i2c_get_clientdata(client);
    /* begin: add by liyaobing l00169718 20110210 for t1320 sleep mode */
    int ret=0;
    /* end: add by liyaobing l00169718 20110210 for t1320 sleep mode */
	 if(0 == is_upgrade_firmware){
    /*since  will poweroff touchscreen  when system enter suspned,it is possible poweroff touchscreen here that
       will cause update firmware fail. so reading register of touchscreen ,if fail poweron touchscreen again */
	    if(i2c_smbus_read_byte_data(g_client,0x00) < 0){
	        msm_gpiomux_get(GPIO_3V3_EN);
	        msm_gpiomux_get(GPIO_CTP_INT);   
	        msm_gpiomux_get(GPIO_CTP_RESET);  
	        ts->chip_poweron() ; 
    	    }  
	 }
    if (client->irq) {
		printk(KERN_INFO "Requesting IRQ...\n");
		ret = request_irq(client->irq, t1320_irq_handler,
				IRQF_TRIGGER_LOW,  client->name, ts);

		if(ret) {
			printk(KERN_ERR "Failed to request IRQ!ret = %d\n", ret);          		
		}else {
			printk(KERN_INFO "Set IRQ Success!\n");
            		ts->use_irq = 1;
		}

	}
    t1320_attn_clear(ts);

    /*begin: add by liyaobing l00169718 20110210 for t1320 sleep mode */
    /*Enter normal mode*/
    ret = i2c_smbus_read_byte_data(client,f01_rmi_ctrl0);
    ret &=  ~(0x03);
    i2c_smbus_write_byte_data(client,f01_rmi_ctrl0,ret);
	/*end: add by liyaobing l00169718 20110210 for t1320 sleep mode */

	 /*enable power reset when system resume */
	power_reset_enable = 1 ;
	hrtimer_start(&ts->timer_reset, ktime_set(2, 0), HRTIMER_MODE_REL);
	i2c_smbus_write_byte_data(client, f11_rmi_ctrl0 + 2, 0x3);
	i2c_smbus_write_byte_data(client, f11_rmi_ctrl0 + 3, 0x3);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void t1320_early_suspend(struct early_suspend *h)
{
	struct t1320 *ts;
	ts = container_of(h, struct t1320, early_suspend);
	t1320_suspend(ts->client, PMSG_SUSPEND);
}

static void t1320_late_resume(struct early_suspend *h)
{
	struct t1320 *ts;
	ts = container_of(h, struct t1320, early_suspend);
	t1320_resume(ts->client);
}
#endif

static struct i2c_device_id t1320_id[]={
	{"t1320",0},
	{},	
};

static struct i2c_driver t1320_driver = {
	.probe		= t1320_probe,
	.remove		= t1320_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= t1320_suspend,
	.resume		= t1320_resume,
#endif
	.id_table	= t1320_id,
	.driver = {
		.name	= "t1320",
	},
};

static int __devinit t1320_init(void)
{
	t1320_wq = create_singlethread_workqueue("t1320_wq");
	if (!t1320_wq) {
		printk(KERN_ERR "Could not create work queue t1320_wq: no memory");
		return -ENOMEM;
	}
	t1320_wq_reset = create_singlethread_workqueue("t1320_wq_reset");
	if (!t1320_wq_reset) {
		printk(KERN_ERR "Could not create work queue t1320_wq_reset: no memory");
		return -ENOMEM;
	}
	return i2c_add_driver(&t1320_driver);
}

static void __exit t1320_exit(void)
{
	i2c_del_driver(&t1320_driver);

	if (t1320_wq)
		destroy_workqueue(t1320_wq);
	if (t1320_wq_reset)
		destroy_workqueue(t1320_wq_reset);
}

module_init(t1320_init);
module_exit(t1320_exit);

MODULE_DESCRIPTION("t1320 Driver");
MODULE_LICENSE("GPL");
