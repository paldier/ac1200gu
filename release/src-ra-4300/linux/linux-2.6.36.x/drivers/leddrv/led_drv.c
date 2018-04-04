#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include "../char/ralink_gpio.h"
#include <asm/rt2880/surfboardint.h>
#define ASUS_NVRAM
#include <nvram/bcmnvram.h>
#include "led_drv.h"
#include "../net/raeth/raether.h"

MODULE_DESCRIPTION("ASUS LED Driver");
MODULE_LICENSE("Proprietary");

u32 ralink_gpio6332_intp = 0;
u32 ralink_gpio6332_edge = 0;
int ralink_gpio_irqnum = 0;

extern ralink_gpio_reg_info ralink_gpio_info[RALINK_GPIO_NUMBER];
extern unsigned int apcli_linkq[2];
extern int esw_port_event_link_up;
static int sw_mode;
static int booting_cnt = 0;
/* audio jack */
static int audio_jack_insert = 0;
static int audio_jack_event_insert = 0;
/* LED */
static int last_led_stage = 0, led_stage = 0;
static int blinking_cnt = 1;
#if 0 /* mark for backup */
static int wps_scan_2g = 0;
static int wps_scan_5g = 0;
#endif
/* 
 * led_state:
 *
 * Power LED
 * 	bit15 -> GPIO47 -> Orange LED
 * 	bit16 -> GPIO48 -> Green LED
 * 2.4G LED
 * 	bit16 -> GPIO16 -> Red LED
 *	bit12 -> GPIO12 -> Blue LED
 * 	bit13 -> GPIO13 -> Green LED
 *	the pin num of 2.4G right shift 12 bits (0x1000 -> 0x1, 0x2000 -> 0x2, 0x10000 -> 0x10)
 * 5G LED
 * 	bit13 -> GPIO45 -> Red LED
 *	bit11 -> GPIO43 -> Blue LED
 * 	bit12 -> GPIO44 -> Green LED
 */
static unsigned long led_state = 0;
/* LED stage */
static unsigned long led_pwr_blinking[3] = {0x0, 0x8000, 0x10000};
//static unsigned long led_wifi_strengh[2][4] = {{0x0, 0x10000, 0x1000, 0x2000}, {0x0, 0x2000, 0x800, 0x1000}};
//static unsigned long led_wifi_strengh[2][4] = {{0x0, 0x10, 0x1, 0x2}, {0x0, 0x2000, 0x800, 0x1000}};
static unsigned long led_wifi_strengh[2][4] = {{0x0, 0x10, (0x10 + 0x2), 0x2}, {0x0, 0x2000, (0x2000 + 0x1000), 0x1000}};
//static unsigned long led_wps_blinking[16] = {
//0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000};//system
//static unsigned long led_wps_scan_blinking[3][16] = {
//{0x0, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},//2.4G
//{0x0, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},//5G
//{0x0, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}};//system

#if 0	/* make for backup */
static unsigned long led_wps_blinking[3][16] = {
{0x0, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},//2.4G
{0x0, 0x1000, 0x1000, 0x1000, 0x1000,0x1000, 0x1000, 0x1000, 0x1000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},//5G
{0x0, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}};//system
#endif
/* timer */
struct timer_list led_drv_timer;
/* work queue */
#define MAX_EV 10
struct event_data {
	unsigned char in_use;
	unsigned char number;
} ev_datas[MAX_EV];
static int ev_data_free_idx = 0, ev_data_handle_idx = 0;
struct wqs {
	spinlock_t lock;
	struct work_struct event_wq;
} g_wqs;

static void led_drv_do_timer(unsigned long unused)
{
	unsigned long delay_time = DELAY_1_SEC, tmp;
	int i, linkq = 0;
	char connStatus[32];
	unsigned int eth_state;
	int conn_stat[2] = {1, 1};
	//unsigned long total_led_state = led_pwr_blinking[1] + led_pwr_blinking[2] +	//pwoer led
	//				led_wifi_strengh[0][1] + 0x1 + led_wifi_strengh[0][3] +	//2.4G led
	//				led_wifi_strengh[1][1] + 0x800 + led_wifi_strengh[1][3];	//5G led

	led_stage = simple_strtol(nvram_safe_get("led_drv_stage"), NULL, 10);
	if (last_led_stage != led_stage) {
		if ((last_led_stage != LEDDRV_WPS_2G_SUCCESS && last_led_stage != LEDDRV_WPS_5G_SUCCESS && 
			last_led_stage != LEDDRV_WPS_2G_SCAN && last_led_stage != LEDDRV_WPS_5G_SCAN) ||
			(led_stage != LEDDRV_WPS_2G_SUCCESS && led_stage != LEDDRV_WPS_5G_SUCCESS && 
			led_stage != LEDDRV_WPS_2G_SCAN && led_stage != LEDDRV_WPS_5G_SCAN))
			blinking_cnt = 0;
		last_led_stage = led_stage;
	}

	switch (led_stage) {
	case LEDDRV_BOOT:
		delay_time = DELAY_2_SEC;
		//led_state = led_pwr_blinking[1];
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
		if (++booting_cnt > 13) {
#else
		if (++booting_cnt > 11) {
#endif
			//boot done
			delay_time = DELAY_0_2_SEC;
			if (++blinking_cnt > 3)
				nvram_set("led_drv_stage", "1");
		}
		else {
			//booting
			//led_state += led_sys_blinking[blinking_cnt];
			led_state = blinking_cnt==1 ? (led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3]) : 0;
			blinking_cnt = 1 - blinking_cnt;
			//printk(MODULE_NAME ": module has inserted %d sec\n", booting_cnt*2);
		}
		break;
	case LEDDRV_NORMAL:
		delay_time = DELAY_1_SEC;
		//Power LED
		if (sw_mode != 3) {	//none AP mode
			led_state = led_pwr_blinking[2];
		}
		else	//for ap mode
		{
			mii_mgr_read(31, 0x3008, &eth_state);	/* port 0 as wan */
			if (eth_state & 0x1) 
				led_state = led_pwr_blinking[2];
			else
			{
				delay_time = DELAY_0_5_SEC;
				led_state = blinking_cnt==1 ? led_pwr_blinking[2] : 0;
				blinking_cnt = 1 - blinking_cnt;
			}
		}

		//wireless LED
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
		for (i = 0; i < 2; i++) {
#else
		i = 0;
#endif
			switch (sw_mode) {
			case 1:
			case 3:
			case 6:
				linkq = 3;
				break;
			case 2:
			case 5:
				sprintf(connStatus, "sta%d_connStatus", i);

				if (nvram_match(connStatus, "0")) {
					linkq = 0;
					conn_stat[i] = 0;
				}
				else if (apcli_linkq[i] >= 70)
					linkq = 3;
				else if (apcli_linkq[i] >= 45)
					linkq = 2;
				else
					linkq = 1;
				break;
			default:
				printk(MODULE_NAME ": sw_mode undefined!\n");
			}
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
			if (conn_stat[i])
				led_state += led_wifi_strengh[i][linkq];
#else
			led_state += led_wifi_strengh[0][linkq] + led_wifi_strengh[1][linkq];
#endif
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
		}
		
#if 0	/* mark for backup */
		if (!conn_stat[0] && !conn_stat[1]) {
			delay_time = DELAY_2_SEC;
			led_state = blinking_cnt==1 ? (led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3]) : led_pwr_blinking[2];
			blinking_cnt = 1 - blinking_cnt;
		}
#endif
#endif
		break;
	case LEDDRV_PRESS_RSTBTN:
		delay_time = DELAY_1_SEC;
		/* pwr led: Slow blink (Green), 2G led: Slow blink (Green), 5G led: Slow blink (Green) */
		led_state = blinking_cnt==1 ? (led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3]) : 0;
		blinking_cnt = 1 - blinking_cnt;
		break;
	case LEDDRV_RELEASE_RSTBTN:
		delay_time = DELAY_0_5_SEC;
		led_state = 0;
		break;
	case LEDDRV_RESTART_WIFI:
		delay_time = DELAY_0_5_SEC;
		/* pwr led: blink (Green), 2G led: blink (Green), 5G led: blink (Green) */
		led_state = blinking_cnt==1 ? (led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3]) : 0;
		blinking_cnt = 1 - blinking_cnt;
		break;
#if 0	/* mark for backup */
	case LEDDRV_WPS:
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
		/* reset apcli quality to 0 if connected */
		apcli_linkq[0] = 0;
		apcli_linkq[1] = 0;
		wps_scan_2g = 0;
		wps_scan_5g = 0;
#endif

		delay_time = DELAY_0_05_SEC;
		led_state = led_wps_blinking[2][blinking_cnt];
		
		if (++blinking_cnt > 15)
			blinking_cnt = 0;
		break;
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
	case LEDDRV_WPS_2G_SCAN:
		wps_scan_2g = 1;
		delay_time = DELAY_0_05_SEC;
		led_state = led_wps_blinking[2][blinking_cnt] + led_wps_blinking[0][blinking_cnt]; 
		if (wps_scan_5g)
			led_state += led_wps_blinking[1][blinking_cnt];
		else
		{
			if (simple_strtol(nvram_safe_get("re_wpsc1_conn"), NULL, 10) == 1)
				led_state += led_wifi_strengh[1][3];
		}

		if (++blinking_cnt > 15)
			blinking_cnt = 0;
		break;
	case LEDDRV_WPS_5G_SCAN:
		wps_scan_5g = 1;	
		delay_time = DELAY_0_05_SEC;
		led_state = led_wps_blinking[2][blinking_cnt] + led_wps_blinking[1][blinking_cnt];
		if (wps_scan_2g)
			led_state += led_wps_blinking[0][blinking_cnt];
		else
		{
			if (simple_strtol(nvram_safe_get("re_wpsc0_conn"), NULL, 10) == 1)
				led_state += led_wifi_strengh[0][3];
		}

		if (++blinking_cnt > 15)
			blinking_cnt = 0;
		break;
	case LEDDRV_WPS_2G_SUCCESS:
		wps_scan_2g = 0;
		delay_time = DELAY_0_05_SEC;
		led_state = led_wps_blinking[2][blinking_cnt] + led_wifi_strengh[0][3]; 
		if (wps_scan_5g)
			led_state += led_wps_blinking[1][blinking_cnt];
		else
		{
			if (simple_strtol(nvram_safe_get("re_wpsc1_conn"), NULL, 10) == 1)
				led_state += led_wifi_strengh[1][3];
		}

		if (++blinking_cnt > 15)
			blinking_cnt = 0;
		break;
	case LEDDRV_WPS_5G_SUCCESS:
		wps_scan_5g = 0;
		delay_time = DELAY_0_05_SEC;
		led_state = led_wps_blinking[2][blinking_cnt] + led_wifi_strengh[1][3];
		if (wps_scan_2g)
			led_state += led_wps_blinking[0][blinking_cnt];
		else
		{
			if (simple_strtol(nvram_safe_get("re_wpsc0_conn"), NULL, 10) == 1)
				led_state += led_wifi_strengh[0][3];
		}

		if (++blinking_cnt > 15)
			blinking_cnt = 0;
		break;
#endif
#endif
	case LEDDRV_WPS:
		delay_time = DELAY_0_5_SEC;
		/* pwr led: blink (Green), 2G led: blink (Green), 5G led: blink (Green) */
		led_state = blinking_cnt==1 ? (led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3]) : 0;
		blinking_cnt = 1 - blinking_cnt;
		break;
#ifdef CONFIG_ASUS_LEDDRV_DUAL_BAND
	case LEDDRV_WPS_2G_SCAN:
	case LEDDRV_WPS_5G_SCAN:
	case LEDDRV_WPS_2G_SUCCESS:
	case LEDDRV_WPS_5G_SUCCESS:
		delay_time = DELAY_0_2_SEC;
		/* pwr led: blink (Green), 2G led: blink (Green), 5G led: blink (Green) */
		led_state = blinking_cnt==1 ? (led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3]) : 0;
		blinking_cnt = 1 - blinking_cnt;
		break;
#endif
	case LEDDRV_FW_UPGRADE:
		delay_time = DELAY_2_SEC;
		/* pwr led: Solid (Orange), 2G led: Off, 5G led: Off */
		led_state = led_pwr_blinking[1];
		break;
	case LEDDRV_ATE:
		delay_time = DELAY_0_5_SEC;
		switch (simple_strtol(nvram_safe_get("led_drv_ate"), NULL, 10)) {
		case ATE_NONE:
			goto exit;
		case ATE_ALL_LED_ON:
			led_state = led_pwr_blinking[1] + led_pwr_blinking[2] +	//pwoer led
					led_wifi_strengh[0][1] + 0x1 + led_wifi_strengh[0][3] +	//2.4G led
					led_wifi_strengh[1][1] + 0x800 + led_wifi_strengh[1][3];	//5G led
			break;
		case ATE_ALL_LED_OFF:
			led_state = 0;
			break;
		case ATE_ALL_LED_ON_HALF:
			break;
		case ATE_ALL_ORANGE_LED_ON:
			led_state = led_pwr_blinking[1];
			break;	
		case ATE_ALL_RED_LED_ON:
			led_state = led_wifi_strengh[0][1] + led_wifi_strengh[1][1];
			break;	
		case ATE_ALL_GREEN_LED_ON:
			led_state = led_pwr_blinking[2] + led_wifi_strengh[0][3] + led_wifi_strengh[1][3];
			break;
		case ATE_ALL_BLUE_LED_ON:
			led_state = 0x1 + 0x800;	/* 0x1 is blue of 2.4G, 0x800 is blue of 5G */
			break;			
		default:
			;
		}
		break;
	default:
		printk(MODULE_NAME ": wrong case!\n");
	}

	tmp = ((~led_state) << 12) & 0x13000;  //GPIO 12,13,16
	*(volatile u32 *)(RALINK_REG_PIORESET) = cpu_to_le32(tmp);
	tmp = (led_state << 12) & 0x13000;
	*(volatile u32 *)(RALINK_REG_PIOSET) = cpu_to_le32(tmp);
	tmp = ~led_state & 0x1B800;  //GPIO 43 44 45 47 48
	*(volatile u32 *)(RALINK_REG_PIO6332RESET) = cpu_to_le32(tmp);
	tmp = led_state & 0x1B800;
	*(volatile u32 *)(RALINK_REG_PIO6332SET) = cpu_to_le32(tmp);

exit:
	mod_timer(&led_drv_timer, jiffies + delay_time);
}

/*
 * send a signal(SIGUSR1) to the registered user process whenever any gpio
 * interrupt comes
 * (called by interrupt handler)
 */
void led_drv_notify_user(int usr)
{
	struct task_struct *p = NULL;

	if (ralink_gpio_irqnum < 0 || RALINK_GPIO_NUMBER <= ralink_gpio_irqnum) {
		printk(KERN_ERR MODULE_NAME ": gpio irq number out of range\n");
		return;
	}

	//don't send any signal if pid is 0 or 1
	if ((int)ralink_gpio_info[ralink_gpio_irqnum].pid < 2)
		return;

	rcu_read_lock();
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
	p = find_task_by_vpid(ralink_gpio_info[ralink_gpio_irqnum].pid);
#else
	p = find_task_by_pid(ralink_gpio_info[ralink_gpio_irqnum].pid);
#endif

	if (NULL == p) {
		printk(KERN_ERR MODULE_NAME ": no registered process to notify\n");
		return;
	}

	if (usr == 1) {
		//printk(KERN_NOTICE MODULE_NAME ": sending a SIGUSR1 to process %d\n",
		//		ralink_gpio_info[ralink_gpio_irqnum].pid);
		send_sig(SIGUSR1, p, 0);
	}
	else if (usr == 2) {
		//printk(KERN_NOTICE MODULE_NAME ": sending a SIGUSR2 to process %d\n",
		//		ralink_gpio_info[ralink_gpio_irqnum].pid);
		send_sig(SIGUSR2, p, 0);
	}
	rcu_read_unlock();
}

static void audio_jack_event(void)
{
	if (audio_jack_insert == 1) {
		audio_jack_event_insert = 1;
		if (nvram_invmatch("asus_mfg", "0"))
			led_drv_notify_user(2);
	}
	mod_timer(&led_drv_timer, jiffies + DELAY_0_05_SEC);
}

/* event dispatch */
static void led_drv_workqueue(struct work_struct *work)
{
	struct wqs *pt = container_of(work, struct wqs, event_wq); // just do normal job
	unsigned long flags;
	int i;

	while (ev_datas[ev_data_handle_idx].in_use) {
		i = ev_datas[ev_data_handle_idx].number;
#if 0
		if (i == 0)
			touch_sensor_event();
		else if (i == 3)
			audio_jack_event();
#endif
		if (i == 0)
			audio_jack_event();
		spin_lock_irqsave(&pt->lock, flags);
		ev_datas[ev_data_handle_idx].in_use = 0; // free it
		ev_data_handle_idx++;
		if (ev_data_handle_idx >= MAX_EV)
			ev_data_handle_idx = 0;
		spin_unlock_irqrestore(&pt->lock, flags);
	}
}

static void add_queue_event(int i)
{
	spin_lock(&g_wqs.lock);
	if (ev_datas[ev_data_free_idx].in_use)
		printk("ERROR!! ev queue full? %d, %d\n", ev_data_free_idx, ev_data_handle_idx);
	else {
		ev_datas[ev_data_free_idx].in_use = 1;
		ev_datas[ev_data_free_idx].number = i;
		ev_data_free_idx++;
		if (ev_data_free_idx >= MAX_EV)
			ev_data_free_idx = 0;
	}
	spin_unlock(&g_wqs.lock);
}

/*
 * 1. save the PIOINT and PIOEDGE value
 * 2. clear PIOINT by writing 1
 * (called by interrupt handler)
 */
static void led_drv_save_clear_intp(void)
{
	ralink_gpio6332_intp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO6332INT));
	ralink_gpio6332_edge = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO6332EDGE));
	*(volatile u32 *)(RALINK_REG_PIOINT) = cpu_to_le32(0xFFFFFFFF);
	*(volatile u32 *)(RALINK_REG_PIOEDGE) = cpu_to_le32(0xFFFFFFFF);
	*(volatile u32 *)(RALINK_REG_PIO6332INT) = cpu_to_le32(0xFFFFFFFF);
	*(volatile u32 *)(RALINK_REG_PIO6332EDGE) = cpu_to_le32(0xFFFFFFFF);
	*(volatile u32 *)(RALINK_REG_PIO9564INT) = cpu_to_le32(0xFFFFFFFF);
	*(volatile u32 *)(RALINK_REG_PIO9564EDGE) = cpu_to_le32(0xFFFFFFFF);
	//*(volatile u32 *)(RALINK_REG_PIO72INT) = cpu_to_le32(0x00FFFFFF);
	//*(volatile u32 *)(RALINK_REG_PIO72EDGE) = cpu_to_le32(0x00FFFFFF);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
void led_drv_irq_handler(unsigned int irq, struct irqaction *irqaction)
#else
irqreturn_t led_drv_irq_handler(int irq, void *irqaction)
#endif
{
	struct gpio_time_record {
		unsigned long falling;
		unsigned long rising;
	};
	static struct gpio_time_record record[RALINK_GPIO_NUMBER];//record;
	unsigned long now;
	int i;
	
	led_drv_save_clear_intp();

	if (led_stage != LEDDRV_NORMAL 
			&& led_stage != LEDDRV_ATE)
		return IRQ_HANDLED;

	now = jiffies;

	for (i = 32; i < 64; i++) {
		if (! (ralink_gpio6332_intp & (1 << (i - 32))))
			continue;
		ralink_gpio_irqnum = i;

		if (ralink_gpio6332_edge & (1 << (i - 32))) {
			if (record[i].rising != 0 && time_before_eq(now,
					record[i].rising + 40L)) {
			}
			else {
				if (ralink_gpio_irqnum == 42) {
					if (audio_jack_insert == 0) {
						printk(MODULE_NAME ": audio jack inserted!\n");
						audio_jack_insert = 1;
						add_queue_event(0);
						schedule_work(&g_wqs.event_wq);
					}
				}
			}
		}
		else {
			record[i].falling = now;
			if (ralink_gpio_irqnum == 42) {
				if (record[i].rising != 0 && time_before_eq(now,
						record[i].rising + 40L)) {
				}
				else {
					if (audio_jack_insert == 1) {
						printk(MODULE_NAME ": audio jack removed!\n");
						audio_jack_insert = 0;
						add_queue_event(0);
						schedule_work(&g_wqs.event_wq);
					}
				}
			}
		}
		break;
	}

	return IRQ_HANDLED;
}

int __init led_drv_init(void)
{
	int i;

	/* create timer */
	init_timer(&led_drv_timer);
	led_drv_timer.function = led_drv_do_timer;
	led_drv_timer.expires = jiffies + DELAY_1_SEC;
	add_timer(&led_drv_timer);

	/* request irq number */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	request_irq(SURFBOARDINT_GPIO, led_drv_irq_handler, IRQF_DISABLED, MODULE_NAME, NULL);
#else
	request_irq(SURFBOARDINT_GPIO, led_drv_irq_handler, SA_INTERRUPT, MODULE_NAME, NULL);
#endif

	/* initial work queue */
	for (i = 0; i < MAX_EV; i++)
		ev_datas[i].in_use = 0;
	INIT_WORK(&g_wqs.event_wq, led_drv_workqueue);
	spin_lock_init(&g_wqs.lock);

	/* initial variable */
	audio_jack_insert = (le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO6332DATA)) & 0x400) >> 10;
	sw_mode = simple_strtol(nvram_safe_get("sw_mode"), NULL, 10);

	printk("ASUS LED driver initialized\n");
	return 0;
}

void __exit led_drv_exit(void)
{
	del_timer(&led_drv_timer);

	free_irq(SURFBOARDINT_GPIO, NULL);

	flush_scheduled_work();

	printk("ASUS LED driver exited\n");
}

module_init(led_drv_init);
module_exit(led_drv_exit);
