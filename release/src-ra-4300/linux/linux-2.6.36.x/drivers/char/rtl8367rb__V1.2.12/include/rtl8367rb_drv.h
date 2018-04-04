#ifndef _RTL8367RB_DRV_H_
#define _RTL8367RB_DRV_H_

#include "ralink_gpio.h"

#define GPIO_DIR_IN		RALINK_GPIO_DIR_IN
#define GPIO_DIR_OUT		RALINK_GPIO_DIR_OUT

extern int ralink_initGpioPin(unsigned int idx, int dir);
extern int ralink_gpio_write_bit(int idx, int value);
extern int ralink_gpio_read_bit(int idx, int *value);

#endif
