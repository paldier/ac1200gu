EXTRA_CFLAGS += -I drivers/char -Idrivers/char/rtl8367rb__V1.2.12/include -D_LITTLE_ENDIAN

ifneq ($(CONFIG_SWITCH_CHIP_ID),)
EXTRA_CFLAGS += -D$(CONFIG_SWITCH_CHIP_ID)
endif

ifneq ($(CONFIG_SWITCH_CTRLIF_SMI_SCK_GPIO_NR_2_SDA_GPIO_NR_1),)
EXTRA_CFLAGS += -DSMI_SCK_GPIO=2 -DSMI_SDA_GPIO=1
endif
ifneq ($(CONFIG_SWITCH_CTRLIF_SMI_SCK_MDC_SDA_MDIO),)
EXTRA_CFLAGS += -DSMI_SCK_GPIO=23 -DSMI_SDA_GPIO=22
endif
ifneq ($(CONFIG_SWITCH_CTRLIF_MDIO),)
EXTRA_CFLAGS += -DMDC_MDIO_OPERATION
endif

obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367rb_drv.o smi.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtk_api.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_acl.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_autofallback.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_cputag.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_dot1x.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_eav.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_eee.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_eeelldp.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_eeep.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_fc.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_gpio.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_green.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_hsb.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_igmp.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_inbwctrl.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_interrupt.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_led.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_lut.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_meter.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_mib.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_mirror.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_misc.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_oam.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_phy.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_pkg.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_port.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_portIsolation.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_qos.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_rldp.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_rma.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_rrcp.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_scheduling.o
#obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_specialCongest.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_storm.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_svlan.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_trunking.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_unknownMulticast.o
obj-$(CONFIG_RTL8367B_SDK)		+= rtl8367b_asicdrv_vlan.o

