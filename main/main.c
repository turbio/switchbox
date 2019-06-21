#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "display.h"
#include "game.h"

#define GPIO_OUT_MASK                                                          \
  ((1ULL << CONFIG_BUZZER_GPIO) | (1ULL << CONFIG_DISPLAY_GPIO_RS) |           \
   (1ULL << CONFIG_DISPLAY_GPIO_E) | (1ULL << CONFIG_DISPLAY_DB_GPIO_4) |      \
   (1ULL << CONFIG_DISPLAY_DB_GPIO_5) | (1ULL << CONFIG_DISPLAY_DB_GPIO_6) |   \
   (1ULL << CONFIG_DISPLAY_DB_GPIO_7))

#define GPIO_IN_MASK                                                           \
  ((1ULL << CONFIG_SWITCH_GPIO_0) | (1ULL << CONFIG_SWITCH_GPIO_1) |           \
   (1ULL << CONFIG_SWITCH_GPIO_2) | (1ULL << CONFIG_SWITCH_GPIO_3) |           \
   (1ULL << CONFIG_SWITCH_GPIO_4) | (1ULL << CONFIG_SWITCH_GPIO_5) |           \
   (1ULL << CONFIG_SWITCH_GPIO_5) | (1ULL << CONFIG_SWITCH_GPIO_6) |           \
   (1ULL << CONFIG_SWITCH_GPIO_7))

static int gpio_switch_map[GPIO_NUM_MAX] = {
    [CONFIG_SWITCH_GPIO_0] = 0,
    [CONFIG_SWITCH_GPIO_1] = 1,
    [CONFIG_SWITCH_GPIO_2] = 2,
    [CONFIG_SWITCH_GPIO_3] = 3,
    [CONFIG_SWITCH_GPIO_4] = 4,
    [CONFIG_SWITCH_GPIO_5] = 5,
    [CONFIG_SWITCH_GPIO_6] = 6,
    [CONFIG_SWITCH_GPIO_7] = 7,
};

static int switch_gpio_map[8] = {
    CONFIG_SWITCH_GPIO_0,
    CONFIG_SWITCH_GPIO_1,
    CONFIG_SWITCH_GPIO_2,
    CONFIG_SWITCH_GPIO_3,
    CONFIG_SWITCH_GPIO_4,
    CONFIG_SWITCH_GPIO_5,
    CONFIG_SWITCH_GPIO_6,
    CONFIG_SWITCH_GPIO_7,
};

static xQueueHandle switch_changes = NULL;

void IRAM_ATTR on_change(void *arg) {
  static uint8_t switch_v;

  uint64_t pin_mask =
      (uint64_t)GPIO.status | ((uint64_t)GPIO.status1.intr_st << 32);

  GPIO.status_w1tc = GPIO.status;
  GPIO.status1_w1tc.intr_st = GPIO.status1.intr_st;

  uint8_t before = switch_v;

  int lpin = -1;
  while (pin_mask) {
    int i = __builtin_ffsll(pin_mask);
    pin_mask = pin_mask >> i;

    int pin = lpin + i;

    lpin = pin;

    if (!gpio_get_level(pin)) {
      switch_v |= 1 << gpio_switch_map[pin];
      gpio_set_intr_type(pin, GPIO_INTR_HIGH_LEVEL);
    } else {
      gpio_set_intr_type(pin, GPIO_INTR_LOW_LEVEL);
      switch_v &= ~(1 << gpio_switch_map[pin]);
    }
  }

  if (switch_v != before) {
    xQueueOverwriteFromISR(switch_changes, &switch_v, NULL);
  }
}

uint8_t read_pins() {
  uint8_t acc = 0;
  for (int i = 0; i < 8; i++) {
    acc |= (!gpio_get_level(switch_gpio_map[i])) << i;
  }
  return acc;
}

void app_main() {
  gpio_config_t out_conf = {
      .intr_type = GPIO_PIN_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = GPIO_OUT_MASK,
      .pull_down_en = 0,
      .pull_up_en = 0,
  };

  gpio_config(&out_conf);

  gpio_config_t in_conf = {
      .intr_type = GPIO_INTR_LOW_LEVEL,
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = GPIO_IN_MASK,
      .pull_down_en = 0,
      .pull_up_en = 0,
  };

  gpio_config(&in_conf);

  switch_changes = xQueueCreate(1, sizeof(uint8_t));

  display_init();
  game_init();

  gpio_isr_register(on_change, NULL, 0, NULL);

  uint8_t v = read_pins();
  xQueueSend(switch_changes, &v, 0);

  for (;;) {
    if (!xQueueReceive(switch_changes, &v, portMAX_DELAY)) {
      continue;
    }

    game_tick(v);
  }
}
