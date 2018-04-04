#ifndef _LED_DRV_H
#define _LED_DRV_H

#define MODULE_NAME	"led_drv"

#define DELAY_0_02_SEC	HZ/50	/* 0.02 sec */
#define DELAY_0_05_SEC	HZ/20	/* 0.05 sec */
#define DELAY_0_067_SEC	HZ/15	/* 0.067 sec */
#define DELAY_0_143_SEC	HZ/7	/* 0.143 sec */
#define DELAY_0_2_SEC	HZ/5	/* 0.2 sec */
#define DELAY_0_5_SEC	HZ/2	/* 0.5 sec */
#define DELAY_1_SEC	HZ*1	/* 1 sec */
#define DELAY_2_SEC	HZ*2	/* 2 sec */
#define DELAY_3_SEC	HZ*3	/* 3 sec */
#define DELAY_5_SEC	HZ*5	/* 5 sec */

extern unsigned long volatile jiffies;

/* LED driver stage: led_drv_stage nvram */
enum {
	LEDDRV_BOOT = 0,		/* boot */
	LEDDRV_NORMAL,			/* normal */
	LEDDRV_PRESS_RSTBTN,		/* hold reset button */
	LEDDRV_RELEASE_RSTBTN,		/* release reset button */
	LEDDRV_RESTART_WIFI,		/* restart_wifi/sta/apcli */
	LEDDRV_WPS,			/* WPS */
	LEDDRV_WPS_2G_SUCCESS,		/* WPS: 2.4G success */
	LEDDRV_WPS_5G_SUCCESS,		/* WPS: 5G success */
	LEDDRV_FW_UPGRADE,		/* firmware upgrade */
	LEDDRV_ATE,			/* ATE mode */
	LEDDRV_WPS_2G_SCAN,
	LEDDRV_WPS_5G_SCAN
};

/* ATE: led_drv_ate nvram */
enum {
	ATE_NONE,
	ATE_ALL_LED_ON,			/* turn on all LED */
	ATE_ALL_LED_OFF,		/* turn off all LED */
	ATE_ALL_LED_ON_HALF,		/* turn on all LED (half light) */
	ATE_ALL_ORANGE_LED_ON,		/* turn on all orange LED */
	ATE_ALL_RED_LED_ON,			/* turn on all red LED */
	ATE_ALL_GREEN_LED_ON,		/* turn on all green LED */
	ATE_ALL_BLUE_LED_ON		/* turn on all blue LED */
};
#endif
