/**************************************************
 Copyright (C), 2009-2010, Huawei Tech. Co., Ltd.
 File Name: kernel/drivers/misc/hw_version_s7pro.c
**************************************************/
#include <linux/input.h> 
#include <linux/module.h> 
#include <linux/init.h>  
#include <linux/mfd/pmic8058.h>
#include <linux/kernel.h>
#include <linux/msm_adc.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/types.h>
#include <mach/board.h>

#define HW_VERSION_LEN  32
#define VOL_TOLERANCE   30

#define VERSION_NUM		50

#ifndef MIN
#define  MIN( x, y ) (((x) < (y)) ? (x) : (y))
#endif

struct kobject *kobj = NULL;
uint32_t hw_voltage = -1; 

#define HW_VERSION_LEN              32            

typedef struct
{
	int hwver_test;  
	uint32_t hwver_mV;   
} S7_version_t;

char hw_ver[HW_VERSION_LEN + 1];
S7_version_t s7_ver;

#if S7_HWID_L3H(S7, S7301, C)
typedef struct
{
   int version;  
   uint32_t mV;
   uint32_t mV2;
} adc_version_to_mv_map_type;
#else
typedef struct
{
   int version;  
   uint32_t mV;
} adc_version_to_mv_map_type;
#endif

#if S7_HWID_L3H(S7, S7301, C)
static const adc_version_to_mv_map_type mv_to_version_map[VERSION_NUM] __initdata= 
{
	{  0,  52,   52 }, 
	{  1,  2101, 52 },
	{  2,  2000, 52 },
	{  3,  1839, 52 },
	{  4,  1650, 52 },
	{  5,  52,   2000},
	{  6,  2101, 2000 },
	{  7,  1513, 52 },
	{  8,  1200, 52 },
	{  9,  1320, 52 },
	{  10, 1100, 52 },
	{  11, 842,  52 },
	{  12, 2000, 2000 },
	{  13, 1918, 2000 },	
	{  14, 52,   367 },
	{  15, 548,  52},
	{  16, 2101, 1100 }, 
	{  17, 2000, 1100 },
	{  18, 708,  1100 },
	{  19, 1839, 2000 },
	{  20, 1650, 2000 },
	{  21, 1100, 1100},
	{  22, 1200, 1100 },
	{  23, 1320, 1100 },
	{  24, 842,  1100 },
	{  25, 1513, 2000 },
	{  26, 1320, 2000 },
	{  27, 1839, 1100 },
	{  28, 1918, 1100 },
	{  29, 1650, 1100 },	
	{  30, 1513, 1100 },
	{  31, 1200, 2000},
    {  32, 1100, 2000 }, 
    {  33, 548,  1100 },
    {  34, 2101, 708 },
    {  35, 842,  2000},
    {  36, 708,  2000 },
    {  37, 2000, 708 },
    {  38, 1839, 708 },
    {  39, 1918, 708 },
    {  40, 1650, 708 },
    {  41, 1513, 708 },
    {  42, 1320, 708 },
    {  43, 1200, 708 },
    {  44, 1100, 708},
    {  45, 956,  708 }, 
    {  46, 842,  708 },
    {  47, 708,  708},
    {  48, 548,  708 }, 
    {  49, -1,   -1 }
};

#else
static const adc_version_to_mv_map_type mv_to_version_map[] = 
{
	{  1,  0 }, 
	{  2,  200 },
	{  5,  367 },
	{  6,  546 },
	{  8,  703 },
	{  10, 890 },
	{  11, 1100 },
	{  12, 1320 },
	{  13, 1467 },
	{  14, 1650 },
	{  15, 1894 },
	{  16, 2104 },
};
#endif

#if S7_HWID_L3H(S7, S7301, C)
const char* sHwsubVer[VERSION_NUM] = 
{
    "PRO301UV0001", "PRO301UV0002", "PRO301UV0003", "PRO301UV0004", "PRO301UV0005", "PRO301UV0006", "PRO301UV0007", "PRO302UV0001", "PRO302UV0002", "PRO302UV0003",
    "PRO302UV0004", "PRO302UV0005", "PRO302UV0006", "PRO302UV0007", "PRO303UV0001", "PRO303UV0002", "PRO303UV0003", "PRO303UV0002", "PRO303UV0005", "PRO303UV0006", 
    "PRO303UV0007", "PRO301WV0001", "PRO301WV0002", "PRO301WV0003", "PRO301WV0004", "PRO301WV0006", "PRO301WV0007", "PRO301CV0001", "PRO301CV0002", "PRO301CV0003", 
    "PRO301CV0004", "PRO301CV0006", "PRO301CV0007", "PRO301TV0001", "PRO301TV0002", "PRO301TV0003", "PRO301TV0004", "PRO312UV0001", "PRO312UV0002", "PRO312UV0003", 
    "PRO312UV0004", "PRO311UV0001", "PRO311UV0002", "PRO311UV0003", "PRO311UV0004", "PRO313UV0001", "PRO313UV0002", "PRO313UV0003", "PRO313UV0004", "unknow"
};
#else
const char* sHwsubVer[] = 
{
    "0000", "0001", "0002", "0003", "0004",  "0005",  "0006", "0007", "0008", "0009", "0010",
    "0011", "0012", "0013",  "0014",  "0015",  "0016"
};
#endif

static uint32_t hw_version_read_adc(int channel, int *mv_reading);

#if S7_HWID_L3H(S7, S7301, C)
static int mv_to_hw_ver(uint32_t voltage, uint32_t voltage2);
#else
static int mv_to_hw_ver(uint32_t voltage );
#endif

int isHWverNumAvailable(int ver)
{
	if ( (ver<0) || ((ver+1) > VERSION_NUM) ) {
		return 0;
	} else {
		return 1;
	}
}

#if S7_HWID_L3H(S7, S7301, C)
int get_hw_sub_ver_str(int channel)
{
	uint32_t vol_mv = 0;
	uint32_t vol_last = 0;
	int i =0;

	// 尝试5次,解决读取数值的剧烈变化
	// 如果连续两次读取值稳定，则版本号读取成功
    for ( ; i<10; i++) {
       vol_mv = hw_version_read_adc(channel, NULL);

		if (vol_last >= 0) {
    		if (((vol_last - 10) <= vol_mv) && ((vol_last + 10) > vol_mv)) {
      			break;
	  		}
		}
		vol_last = vol_mv;
    	mdelay(10);
  	}
 
 	return vol_mv;
}

static int mv_to_hw_ver(uint32_t voltage,  uint32_t voltage2)
{
	uint32_t search_index=0;
	int hard_ver = -1;

	if (voltage <= 100) {
        voltage = 52;
    }

    if (voltage2 <= 100) {
        voltage2 = 52;
    }    

	for(search_index = 0; search_index < VERSION_NUM; search_index++) {
		if ((mv_to_version_map[search_index].mV - VOL_TOLERANCE <= voltage)
            && (mv_to_version_map[search_index].mV + VOL_TOLERANCE > voltage)
            && (mv_to_version_map[search_index].mV2- VOL_TOLERANCE <= voltage2)
            && (mv_to_version_map[search_index].mV2 + VOL_TOLERANCE > voltage2)) {
            hard_ver = search_index;
            break;
        }    
    }    
	
  
  	return hard_ver;
}
#else
int get_hw_sub_ver_str(void)
{
	uint32_t vol_mv = 0;
	uint32_t vol_last = 0;
	int hard_ver = -1;
	int i =0;

	// 尝试5次,解决读取数值的剧烈变化
	// 如果连续两次读取值稳定，则版本号读取成功
 	for ( ; i<10; i++) {
    	vol_mv = hw_version_read_adc(CHANNEL_ADC_HW_VRESION, NULL);
		if (vol_last > 0) {
    		if (((vol_last - 10) <= vol_mv) && ((vol_last + 10) > vol_mv)) {
      			break;
	  		}
		}
		vol_last = vol_mv;
    	mdelay(10);
  	}

  	if (vol_last > 0 ) {
		hard_ver =  mv_to_hw_ver(vol_mv);
	} else {
  		printk(KERN_ERR "Reading adc hardware version fail.\n");
 	}
  
 	return hard_ver;
}

static int mv_to_hw_ver(uint32_t voltage )
{
	uint32_t map_size = 0;
	uint32_t search_index=0;
	int hard_ver = -1;

	if ( voltage <= (mv_to_version_map[0].mV + VOL_TOLERANCE )) {
		hard_ver = mv_to_version_map[search_index].version;
		return hard_ver;
	} else {
  		search_index++;
	}
  
	map_size = sizeof(mv_to_version_map)/sizeof(mv_to_version_map[0]);

  	while (search_index < map_size) {
    	if(((mv_to_version_map[search_index].mV - VOL_TOLERANCE) <= voltage) 
			&& ((mv_to_version_map[search_index].mV + VOL_TOLERANCE) > voltage)) {
    		hard_ver = mv_to_version_map[search_index].version;
    	    break;
   		} else if((mv_to_version_map[search_index].mV - VOL_TOLERANCE) > voltage ) {
	  		break;
		} else {
      		search_index++;
    	}
  	}

	if(search_index == map_size ) {
		hard_ver = mv_to_version_map[map_size-1].version + 1;
	}
  
  	return hard_ver;
}
#endif

#if S7_HWID_L3H(S7, S7301, C)
static void S7_version_init(void)
{
    int times =0;
    int major_vol =0;
    int minor_vol = 0;
    int version = 0;
    
    for (times =0; times<10; times++) {       
        major_vol = get_hw_sub_ver_str(CHANNEL_ADC_HW_VRESION);
    }
    
    for (times =0; times<10; times++) {       
        minor_vol = get_hw_sub_ver_str(CHANNEL_ADC_HW_VERSION2);
    }

    /* hw */
    memset(hw_ver, 0, strlen(hw_ver));
	version = mv_to_hw_ver(major_vol, minor_vol);
    if (version == -1) {
        strcpy (hw_ver, "unknown");          
    } else {    
    	strcpy(hw_ver, sHwsubVer[version]);
    }
}
#else
static void S7_version_init(void)
{
    int times =0;
    
    for (times =0; times<10; times++) {       
        s7_ver.hwver_test = get_hw_sub_ver_str();
        if(isHWverNumAvailable(s7_ver.hwver_test) == 1) {
            break;
        }
    }

    /* hw */
    if( (isHWverNumAvailable(s7_ver.hwver_test) == 1) ) {
        strcat (hw_ver, "V"); 
        
        if (strlen(sHwsubVer[s7_ver.hwver_test]) < (HW_VERSION_LEN - strlen(hw_ver))) {
            strcat (hw_ver, sHwsubVer[s7_ver.hwver_test]);
        }
    } else {
        strcpy (hw_ver, "unknown");   
    }      
}
#endif

static ssize_t hw_version_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
{
	return sprintf(buf, "%s", hw_ver);
}

static struct kobj_attribute hw_version_attribute =
     __ATTR(hw_version, 0444, hw_version_attr_show, NULL);

static ssize_t hw_voltage_attr_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
{
	int major_voltage = get_hw_sub_ver_str(CHANNEL_ADC_HW_VRESION);
    int minor_voltage = get_hw_sub_ver_str(CHANNEL_ADC_HW_VERSION2);

	return sprintf(buf, "MPP05 %dmv\nMPP08 %dmv\n", major_voltage, minor_voltage);
}

static struct kobj_attribute hw_voltage_attribute =
     __ATTR(hw_voltage, 0444, hw_voltage_attr_show, NULL);

static struct attribute* version_attributes[] =
{
     &hw_version_attribute.attr,
     &hw_voltage_attribute.attr,   
     NULL
};

static struct attribute_group version_defattr_group =
{
     .attrs = version_attributes,
};

static uint32_t hw_version_read_adc(int channel, int *mv_reading)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
					__func__, channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv_S7(h, &conv_complete_evt);
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = wait_for_completion_interruptible(&conv_complete_evt);
	if (ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret)
		pr_err("%s: couldnt close channel %d ret=%d\n",
					__func__, channel, ret);
	if (mv_reading)
		*mv_reading = (int)adc_chan_result.measurement;

	return adc_chan_result.physical;
out:   
	if (mv_reading) {
		*mv_reading = 0;
    }
	return -EINVAL;
}

static int __init
hw_version_init(void)
{
    kobj = kobject_create_and_add("version", NULL);
    if (kobj == NULL) {
		return -1;
	}
    
    if (sysfs_create_group(kobj, &version_defattr_group)) {
		kobject_put(kobj);
		return -1;
	} 
  
    S7_version_init();

	return 0;
}

static void __exit
hw_version_exit (void)
{
	sysfs_remove_group(kobj, &version_defattr_group);
}
late_initcall(hw_version_init);
module_exit(hw_version_exit);

MODULE_DESCRIPTION("hardware version for s7pro");
MODULE_LICENSE("GPL");

