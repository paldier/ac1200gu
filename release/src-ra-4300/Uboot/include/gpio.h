#ifndef GPIO_H
#define GPIO_H

#if defined(MT7620_MP) || defined (MT7621_MP)
/* LED, Button GPIO# definition */
#if defined(ASUS_RTN14U)
#define RST_BTN		1	/* I2C_SD */
#define WPS_BTN		2	/* I2C_SCLK */
#define PWR_LED		43	/* EPHY_LED3 */
#define WIFI_2G_LED	72	/* WLAN_N */
#define WAN_LED		40	/* EPHY_LED0 */
#define LAN_LED		41	/* EPHY_LED1 */
#define USB_LED		42	/* EPHY_LED2 */

#elif defined(ASUS_RTAC52U)
#define RST_BTN		1	/* I2C_SD */
#define WPS_BTN		2	/* I2C_SCLK */
#define RADIO_ONOFF_BTN	13	/* DSR_N */
#define PWR_LED		9	/* CTS_N */
#define WIFI_2G_LED	72	/* WLAN_N */
#define WAN_LED		8	/* TXD */
#define LAN_LED		12	/* DCD_N */
#define USB_LED		14	/* RIN */

#elif defined(ASUS_RTAC51U)
#define RST_BTN		1	/* I2C_SD */
#define WPS_BTN		2	/* I2C_SCLK */
#define PWR_LED		9	/* CTS_N */
#define WIFI_2G_LED	72	/* WLAN_N */
#define USB_LED		14	/* RIN */

#elif defined(ASUS_RTAC51UP)
#define RST_BTN		1	/* I2C_SD */
#define WPS_BTN		2	/* I2C_SCLK */
#define PWR_LED		9	/* CTS_N */
#define WIFI_2G_LED	72	/* WLAN_N */
#define USB_LED		14	/* RIN */

#elif defined(ASUS_RTAC53)
#define RST_BTN		1	/* I2C_SD */
#define WPS_BTN		2	/* I2C_SCLK */
#define PWR_LED		9	/* CTS_N */
#define WIFI_2G_LED	72	/* WLAN_N */
#define USB_LED		14	/* RIN */
#define RESET_SWITCH	11	/* DTR_N */

#elif defined(ASUS_RTN54U)
#define RST_BTN		1	/* I2C_SD */
#define WPS_BTN		2	/* I2C_SCLK */
#define PWR_LED		9	/* CTS_N */
#define WIFI_2G_LED	72	/* WLAN_N */
#define USB_LED		14	/* RIN */

#elif defined(ASUS_RTAC1200GA1)
#define RST_BTN		41	/* ND_WP */
#define WPS_BTN		43	/* ND_CLE */
#define PWR_LED		48	/* ND_D3 */
#define WIFI_2G_LED	14	/* WLAN_N */
#define USB_LED		47	/* ND_D2 */
#define WIFI_5G_LED	15	/* WLAN_N */
#define WAN_LED		16	/* EPHY_LED0 */
#define LAN_LED		7	/* EPHY_LED1 */

#elif defined(ASUS_RTAC1200GU)
#define RST_BTN		41	/* ND_WP */
#define WPS_BTN		43	/* ND_CLE */
#define PWR_LED		48	/* ND_D3 */
#define WIFI_2G_LED	14	/* WLAN_N */
#define USB_LED		47	/* ND_D2 */
#define WIFI_5G_LED	15	/* WLAN_N */
#define WAN_LED		16	/* EPHY_LED0 */
#define LAN_LED		7	/* EPHY_LED1 */

#elif defined(ASUS_RTN11P)
#define RST_BTN		17	/* WDT_RST_N */
#define WIFI_2G_LED	72	/* WLAN_N */
#define WAN_LED		44	/* EPHY_LED4_N_JTRST_N */
#define LAN_LED		39	/* SPI_WP */

#elif defined(ASUS_RPAC56)
#define RST_BTN				41	/* ND_WP*/
#define WPS_BTN				10	/* CTS2_N */
#define WIFI_2G_LED_RED		16	/* JTCLK */
#define WIFI_2G_LED_GREEN	13	/* JTDO */
#define WIFI_2G_LED_BLUE	12	/* RXD2 */
#define WIFI_5G_LED_RED		45	/* ND_D0 */
#define WIFI_5G_LED_GREEN	44	/* ND_ALE */
#define WIFI_5G_LED_BLUE	43	/* ND_CLE */
#define PWR_LED_WHITE		48	/* ND_D3 */
#define PWR_LED_ORANGE		47	/* ND_D2 */
#define PWR_LED_Blue		46	/* ND_D1 */
#else
#if defined(CONFIG_ASUS_PRODUCT)
#error Invalid product
#endif //#if defined(CONFIG_ASUS_PRODUCT)
#endif //#if defined(ASUS_RTN14U)
#endif  //#if defined(MT7620_MP) || defined (MT7621_MP)

#if defined(MT7628_MP)
#if defined(ASUS_RTAC1200)
#define RST_BTN		5	/* I2C_SD */
#define WPS_BTN		11	/* GPIO0 */
#define PWR_LED		37	/* REF_CLKO */
#define WIFI_2G_LED	44	/* WLED_N */
#define USB_LED		6	/* SPI_CS1 */
#elif defined(ASUS_RTN11PB1)
#define PWR_LED		37	/* REF_CLKO */
#define RST_BTN		5	/* I2C _SD */
#define WIFI_2G_LED	44	/* WLED_N */
#define WAN_LED		43	/* EPHY_LED0_N_JTDO */
#define LAN_LED		42	/* GPIO0 */
#endif
#endif  // #if defined(MT7628_MP)


enum gpio_reg_id {
	GPIO_INT = 0,
	GPIO_EDGE,
	GPIO_RMASK,
	GPIO_MASK,
	GPIO_DATA,
	GPIO_DIR,
	GPIO_POL,
	GPIO_SET,
	GPIO_RESET,
#if defined(MT7620_MP) 
	GPIO_TOG,
#endif	
	GPIO_MAX_REG
};

#if defined(MT7620_MP) 
extern unsigned int mtk7620_get_gpio_reg_addr(unsigned short gpio_nr, enum gpio_reg_id id);
extern int mtk7620_set_gpio_dir(unsigned short gpio_nr, unsigned short gpio_dir);
extern int mtk7620_get_gpio_pin(unsigned short gpio_nr);
extern int mtk7620_set_gpio_pin(unsigned short gpio_nr, unsigned int val);
#elif defined(MT7621_MP)
extern unsigned int mtk7621_get_gpio_reg_addr(unsigned short gpio_nr, enum gpio_reg_id id);
#else
extern unsigned int get_gpio_reg_addr(unsigned short gpio_nr, enum gpio_reg_id id);
extern int set_gpio_dir(unsigned short gpio_nr, unsigned short gpio_dir);
extern int get_gpio_pin(unsigned short gpio_nr);
extern int set_gpio_pin(unsigned short gpio_nr, unsigned int val);
#if defined(ASUS_RPAC56)
extern void led_all_off();
extern void led_all_on();
#elif defined(ASUS_RTN11PB1)
extern void led_all_off();
extern void led_all_on();
extern void led_power_on();
#endif
#endif



extern void led_init(void);
extern void gpio_init(void);
extern void LEDON(void);
extern void LEDOFF(void);
extern unsigned long DETECT(void);
extern unsigned long DETECT_WPS(void);
extern void rst_fengine(void);

#if defined(ALL_LED_OFF)
extern void ALL_LEDON(void);
extern void ALL_LEDOFF(void);
#endif

#endif
