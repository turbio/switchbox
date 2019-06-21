#pragma once

#include <stdint.h>

#define CONFIG_DISPLAY_GPIO_RS 15
#define CONFIG_DISPLAY_GPIO_E 2
#define CONFIG_DISPLAY_DB_GPIO_4 0
#define CONFIG_DISPLAY_DB_GPIO_5 4
#define CONFIG_DISPLAY_DB_GPIO_6 16
#define CONFIG_DISPLAY_DB_GPIO_7 17

#define DISPLAY_PTR(addr) ((addr) | (1 << 7))
#define DISPLAY_HOME1 (DISPLAY_PTR(0))
#define DISPLAY_HOME2 (DISPLAY_PTR(1 << 6))

void display_send_char(char c);
void display_send_str(char *str);
void display_send_cmd(uint8_t cmd);
void display_init();
