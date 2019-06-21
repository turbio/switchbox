#include "game.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "display.h"

void menu_state(uint8_t);

static void (*state)(uint8_t);

void draw_time(TickType_t ticks) {
  char buff[16];
  TickType_t secs = (ticks / portTICK_PERIOD_MS) / 10;

  sprintf(buff, "%02d:%02d %03d", secs / 60, secs % 60, (ticks % 100) * 10);

  display_send_cmd(DISPLAY_PTR(4));
  display_send_str(buff);
}

void draw_timer(void *start) {
  for (;;) {
    TickType_t ticks = xTaskGetTickCount() - (TickType_t)start;
    draw_time(ticks);
  }
}

void flash_time(void *ticks) {
  for (;;) {
    draw_time((TickType_t)ticks);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    display_send_cmd(DISPLAY_HOME1);
    display_send_str("                ");

    vTaskDelay(300 / portTICK_PERIOD_MS);
  }
}

void count_all_game_state(uint8_t v) {
  static TaskHandle_t timer_task;
  static TickType_t start;
  static TickType_t end;

  start = xTaskGetTickCount();

  display_send_cmd(DISPLAY_HOME1);
  display_send_str("                ");
  display_send_cmd(DISPLAY_HOME2);
  display_send_str("                ");

  xTaskCreate(draw_timer, "draw_timer", 2048, (void *)start, 10, &timer_task);

  void show_result(uint8_t v) {
    display_send_cmd(DISPLAY_HOME2);
    display_send_str("  ZERO TO MENU  ");

    static TaskHandle_t flash_task;
    xTaskCreate(
        flash_time, "flash_time", 2048, (void *)(end - start), 10, &flash_task);

    void return_to_menu(uint8_t v) {
      if (!v) {
        vTaskDelete(flash_task);
        state = menu_state;
      }
    }

    state = return_to_menu;
  }

  static uint8_t guesses[256];
  static int last_prog;

  last_prog = 0;

  for (int i = 0; i < sizeof(guesses); i++) {
    guesses[i] = 0;
  }

  void on_change(uint8_t v) {

    end = xTaskGetTickCount();

    guesses[v] = 1;

    for (int i = 0; i < 256; i++) {
      if (!guesses[i]) {
        if (i > last_prog) {
          last_prog = i;

          gpio_set_level(CONFIG_BUZZER_GPIO, 1);
          vTaskDelay(100 / portTICK_PERIOD_MS);
          gpio_set_level(CONFIG_BUZZER_GPIO, 0);
        }

        return;
      }
    }

    vTaskDelete(timer_task);

    draw_time(end - start);

    gpio_set_level(CONFIG_BUZZER_GPIO, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_BUZZER_GPIO, 0);

    state = show_result;
  }

  state = on_change;
}

void guess_game_state(uint8_t v) {
  static TaskHandle_t timer_task;
  static TickType_t start;
  static TickType_t end;

  start = xTaskGetTickCount();

  display_send_cmd(DISPLAY_HOME1);
  display_send_str("                ");
  display_send_cmd(DISPLAY_HOME2);
  display_send_str("                ");

  xTaskCreate(draw_timer, "draw_timer", 2048, (void *)start, 10, &timer_task);

  void show_result(uint8_t v) {
    display_send_cmd(DISPLAY_HOME2);
    display_send_str("  ZERO TO MENU  ");

    static TaskHandle_t flash_task;
    xTaskCreate(
        flash_time, "flash_time", 2048, (void *)(end - start), 10, &flash_task);

    void return_to_menu(uint8_t v) {
      if (!v) {
        vTaskDelete(flash_task);
        state = menu_state;
      }
    }

    state = return_to_menu;
  }

  static uint8_t answer;

  answer = start % 256;

  printf("answer is: %d\n", answer);

  void on_change(uint8_t v) {
    end = xTaskGetTickCount();

    printf("at: %d\n", v);

    if (answer != v) {
      return;
    }

    vTaskDelete(timer_task);

    draw_time(end - start);

    gpio_set_level(CONFIG_BUZZER_GPIO, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_BUZZER_GPIO, 0);

    state = show_result;
  }

  state = on_change;
}

void wait_guess_zero_state(uint8_t v) {
  display_send_cmd(DISPLAY_HOME1);
  display_send_str(" ZERO TO BEGIN  ");
  display_send_cmd(DISPLAY_HOME2);
  display_send_str("                ");

  void zero_to_guess(uint8_t v) {
    if (!v) {
      state = guess_game_state;
    }
  }

  state = zero_to_guess;
}

void wait_count_zero_state(uint8_t v) {
  display_send_cmd(DISPLAY_HOME1);
  display_send_str("  ZERO TO BEGIN ");
  display_send_cmd(DISPLAY_HOME2);
  display_send_str("                ");

  void zero_to_count(uint8_t v) {
    if (!v) {
      state = count_all_game_state;
    }
  }

  state = zero_to_count;
}

void menu_state(uint8_t v) {
  display_send_cmd(DISPLAY_HOME1);
  display_send_str("GUESS      COUNT");
  display_send_cmd(DISPLAY_HOME2);
  display_send_str("                ");

  void run_menu_state(uint8_t v) {
    display_send_cmd(DISPLAY_HOME2);
    for (int i = 7; i >= 0; i--) {
      display_send_char((v & (1u << i)) ? 0xbf : 0x5f);

      if (i == 4) {
        display_send_str("        ");
      }
    }

    if (v == 0xf0) {
      state = wait_guess_zero_state;
    } else if (v == 0x0f) {
      state = wait_count_zero_state;
    }
  }

  state = run_menu_state;
}

void game_init() { state = menu_state; }

void game_tick(uint8_t v) {
  void *before;

  do {
    before = (void *)state;
    state(v);
  } while ((void *)state != before);
}
