#include "display.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static char cache[256];
static int cache_cursor;
static int real_cursor;

static void send_half_op(int rs, int db7, int db6, int db5, int db4) {
  gpio_set_level(CONFIG_DISPLAY_GPIO_RS, rs);
  gpio_set_level(CONFIG_DISPLAY_DB_GPIO_7, db7);
  gpio_set_level(CONFIG_DISPLAY_DB_GPIO_6, db6);
  gpio_set_level(CONFIG_DISPLAY_DB_GPIO_5, db5);
  gpio_set_level(CONFIG_DISPLAY_DB_GPIO_4, db4);

  gpio_set_level(CONFIG_DISPLAY_GPIO_E, 1);
  vTaskDelay(1);
  gpio_set_level(CONFIG_DISPLAY_GPIO_E, 0);
}

static void send_op(int rs, uint8_t data) {
  taskDISABLE_INTERRUPTS(); // yikes
  send_half_op(
      rs,
      data & (1u << 7),
      data & (1u << 6),
      data & (1u << 5),
      data & (1u << 4));
  send_half_op(
      rs,
      data & (1u << 3),
      data & (1u << 2),
      data & (1u << 1),
      data & (1u << 0));
  taskENABLE_INTERRUPTS();
}

void display_send_char(char c) {
  if (cache[cache_cursor] == c) {
    cache_cursor++;
    return;
  }

  cache[cache_cursor] = c;

  if (cache_cursor != real_cursor) {
    send_op(0, DISPLAY_PTR(cache_cursor));
  }

  send_op(1, c);
  real_cursor = ++cache_cursor;
}

void display_send_str(char *str) {
  while (*str) {
    display_send_char(*str);
    str++;
  }
}

void display_send_cmd(uint8_t cmd) {
  if (cmd & (1 << 7)) {
    cache_cursor = cmd & ~(1 << 7);
    return;
  }

  send_op(0, cmd);
}

void display_init() {
  for (int i = 0; i < sizeof(cache); i++) {
    cache[i] = 0;
  }
  cache_cursor = 0;
  real_cursor = 0;

  gpio_set_level(CONFIG_DISPLAY_GPIO_E, 0);

  // set 4bit addressing mode
  send_half_op(0, 0, 0, 1, 0);

  // set 4bit addressing + two lines
  // 0 0010 1100
  send_op(0, 0x2c);

  // clear display
  // 0 0000 0001
  send_op(0, 0x01);

  // display on, cursor off
  // 0 0000 1100
  send_op(0, 0x0c);

  // writing moves w_ptr
  // 0 0000 0110
  send_op(0, 0x06);
}
