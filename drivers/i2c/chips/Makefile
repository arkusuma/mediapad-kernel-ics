#
# Makefile for miscellaneous I2C chip drivers.
#
# Do not add new drivers to this directory! It is DEPRECATED.
#
# Device drivers are better grouped according to the functionality they
# implement rather than to the bus they are connected to. In particular:
# * Hardware monitoring chip drivers go to drivers/hwmon
# * RTC chip drivers go to drivers/rtc
# * I/O expander drivers go to drivers/gpio
#


#/* <BU5D07680 liujinggang 201004013 begin */
obj-$(CONFIG_S7_FEATURE_SENSORS_ST_LSM303DLH) += st303_compass.o
obj-$(CONFIG_S7_FEATURE_SENSORS_ST_LSM303DLH) += st303_gs.o
#/* BU5D07680 liujinggang 20100413 end> */
ifeq ($(CONFIG_I2C_DEBUG_CHIP),y)
EXTRA_CFLAGS += -DDEBUG
endif

