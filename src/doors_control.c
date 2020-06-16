#include "doors.h"
#include "doors_global.h"
#include "doors_config.h"

#define DOORS_CONTROL 1
#include "doors_control.h"

#include "driver/gpio.h"

static const char * TAG = "DOORS_CONTROL";

static QueueHandle_t relay_command_queue[DOOR_COUNT];
static QueueHandle_t relays_control_queue;

#define BUTTON_OPEN        0x01
#define BUTTON_CLOSE       0x02
#define BUTTON_OPEN_START  0x04  // First step for deboucing
#define BUTTON_CLOSE_START 0x08  // idem
#define BUTTON_OPEN_MASK  (BUTTON_OPEN  | BUTTON_OPEN_START )
#define BUTTON_CLOSE_MASK (BUTTON_CLOSE | BUTTON_CLOSE_START)

#define RELAY_OPEN   0
#define RELAY_CLOSE  1

#define ISACTIVE(bit, active_low) ((bit && !active_low) || (!bit && active_low))

static const uint8_t gpio_button_open[5]  = { 36, 34, 32, 25, 27 };
static const uint8_t gpio_button_close[5] = { 39, 35, 33, 26, 14 };
static const uint8_t gpio_relay[8]        = { 23, 22, 21, 19, 18, 05, 17, 16 };

static const uint8_t latch_relay_open     = 4;
static const uint8_t latch_relay_close    = 2;

static const bool active_low = true;

static void set_relay(uint8_t connector, relay_command command, bool level) 
{
  ESP_LOGI(TAG, 
           "Set Relay connector %d, bank %s to %d.", 
           connector, 
           (command == RELAY_OPEN) ? "Open" : "Close", 
           level);
  uint8_t data = connector | (command << 7) | (level << 6);

  xQueueSendToBack(relays_control_queue, &data, 0);
}

void add_relay_command(uint8_t door_idx, relay_command command)
{
  ESP_LOGI(TAG, "Adding command %d to door %d.", command, door_idx);
  xQueueSendToBack(relay_command_queue[door_idx], &command, 0);
}

static void relays_control_process(void * not_used)
{
  static uint8_t relay_open_values  = active_low ? 0xFF : 0x00;
  static uint8_t relay_close_values = active_low ? 0xFF : 0x00;
  static uint8_t new_relay_open_values;
  static uint8_t new_relay_close_values;
  
  int i;

  gpio_pad_select_gpio(latch_relay_open);
  gpio_pad_select_gpio(latch_relay_close);
  gpio_set_direction(latch_relay_open,   GPIO_MODE_OUTPUT);
  gpio_set_direction(latch_relay_close,  GPIO_MODE_OUTPUT);

  for (i = 0; i < 8; i++) {
    gpio_pad_select_gpio(gpio_relay[i]);
    gpio_set_direction(gpio_relay[i],  GPIO_MODE_OUTPUT);
  }

  vTaskDelay(pdMS_TO_TICKS(50));

  for (i = 0; i < 8; i++) {
    gpio_set_level(gpio_relay[i], active_low);
  }

  gpio_set_level(latch_relay_open,  1);
  gpio_set_level(latch_relay_close, 1);
  vTaskDelay(pdMS_TO_TICKS(5));
  gpio_set_level(latch_relay_open,  0);
  gpio_set_level(latch_relay_close, 0);

  while (true) {
    uint8_t data;

    TickType_t waitTime = pdMS_TO_TICKS(10000);

    if (xQueueReceive(relays_control_queue, &data, waitTime) == pdTRUE) {
      new_relay_close_values = relay_close_values;
      new_relay_open_values = relay_open_values;

      if (data & 0x80) {    // RELAY_CLOSE ?
        if (data & 0x40) {  // level = 1 ?
          new_relay_close_values |= (1 << (data & 0x07));
        }
        else {
          new_relay_close_values &= ~(1 << (data & 0x07));
        }
      }
      else {
        if (data & 0x40) {  // level = 1 ?
          new_relay_open_values |= (1 << (data & 0x07));
        }
        else {
          new_relay_open_values &= ~(1 << (data & 0x07));
        }
      }
      while (uxQueueMessagesWaiting(relays_control_queue) > 0) {
        xQueueReceive(relays_control_queue, &data, 0);
        if (data & 0x80) {    // RELAY_CLOSE ?
          if (data & 0x40) {  // level = 1 ?
            new_relay_close_values |= (1 << (data & 0x07));
          }
          else {
            new_relay_close_values &= ~(1 << (data & 0x07));
          }
        }
        else {
          if (data & 0x40) {  // level = 1 ?
            new_relay_open_values |= (1 << (data & 0x07));
          }
          else {
            new_relay_open_values &= ~(1 << (data & 0x07));
          }
        }
      }
      if (new_relay_open_values != relay_open_values) {
        for (int i = 0; i < 7; i++) {
          gpio_set_level(gpio_relay[i], new_relay_open_values & (1 << i));
        }
        gpio_set_level(latch_relay_open, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(latch_relay_open, 0);

        relay_open_values = new_relay_open_values;
      }
      if (new_relay_close_values != relay_close_values) {
        for (int i = 0; i < 7; i++) {
          gpio_set_level(gpio_relay[i], new_relay_close_values & (1 << i));
        }
        gpio_set_level(latch_relay_close, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(latch_relay_close, 0);

        relay_close_values = new_relay_close_values;
      }
    }
  }

  vTaskDelete(NULL);
}

static void door_relay_control_process(void * idx)
{
  uint8_t door_idx = * (uint8_t *)idx;
  uint8_t conn_relays;
  seq_t   seq_open[SEQ_SIZE], seq_close[SEQ_SIZE];
  uint8_t seq_idx;

  // initialization

  conn_relays  = doors_config.doors[door_idx].conn_relays;
  
  memcpy(seq_open,  doors_config.doors[door_idx].seq_open,  SEQ_SIZE * sizeof(seq_t));
  memcpy(seq_close, doors_config.doors[door_idx].seq_close, SEQ_SIZE * sizeof(seq_t));

  relay_command current_command = RELAY_IDLE;
  relay_command new_command;

  TickType_t waitTime = pdMS_TO_TICKS(1000);
  TickType_t remainingTime;

  while (true) {
    TickType_t startLoopTime = xTaskGetTickCount();

    if (xQueueReceive(relay_command_queue[door_idx], &new_command, waitTime) == pdTRUE) {
      while (uxQueueMessagesWaiting(relay_command_queue[door_idx]) > 0) {
        xQueueReceive(relay_command_queue[door_idx], &new_command, 0);
      }

      TickType_t elapse = xTaskGetTickCount() - startLoopTime;
      remainingTime = elapse >= waitTime ? 0 : waitTime - elapse;

      if (new_command != current_command) {
        if (current_command != RELAY_IDLE) {
          set_relay(conn_relays, current_command, active_low);
          waitTime = pdMS_TO_TICKS(250);
        }
        current_command = (new_command == RELAY_STOP) ? RELAY_IDLE : new_command;
        seq_idx = 0;
      }
      else if (remainingTime > pdMS_TO_TICKS(10)) {
        waitTime = remainingTime;
      }
    }
    else if (current_command != RELAY_IDLE) {
      if (((current_command == RELAY_OPEN) && (seq_open[seq_idx] == 0)) ||
          ((current_command == RELAY_CLOSE) && (seq_close[seq_idx] == 0))) {
        set_relay(conn_relays, current_command, active_low);
        current_command = RELAY_IDLE;
      }
      else {
        set_relay(conn_relays, current_command, (seq_idx & 1) ? active_low : !active_low);
        waitTime = pdMS_TO_TICKS(seq_open[seq_idx++]);
      }
    }
    else {
      waitTime = pdMS_TO_TICKS(1000);
    }
  }

  vTaskDelete(NULL);
}

static void door_button_control_process(void * idx)
{
  uint8_t door_idx = * (uint8_t *)idx;
  uint8_t button_state = 0;
  uint8_t bopen, bclose;

  // initialization

  bopen  = gpio_button_open[doors_config.doors[door_idx].conn_buttons];
  bclose = gpio_button_close[doors_config.doors[door_idx].conn_buttons];

  gpio_pad_select_gpio(bopen);
  gpio_pad_select_gpio(bclose);
  gpio_set_direction(bopen,  GPIO_MODE_INPUT);
  gpio_set_direction(bclose, GPIO_MODE_INPUT);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(50));

    if (ISACTIVE(gpio_get_level(bopen), active_low)) {
      if (button_state & BUTTON_OPEN_MASK) {
        if (button_state & BUTTON_OPEN_START) {
          button_state = (button_state & BUTTON_CLOSE_MASK) | BUTTON_OPEN;
          add_relay_command(door_idx, (button_state & BUTTON_CLOSE) ? RELAY_STOP : RELAY_OPEN);
        } 
      } 
      else {
        button_state |= BUTTON_OPEN_START;
      }
    }
    else {
      button_state &= BUTTON_CLOSE_MASK; // Clear button open bits
    }

    if (ISACTIVE(gpio_get_level(bclose), active_low)) {
      if (button_state & BUTTON_CLOSE_MASK) {
        if (button_state & BUTTON_CLOSE_START) {
          button_state = (button_state & BUTTON_OPEN_MASK) | BUTTON_CLOSE;
          add_relay_command(door_idx, (button_state & BUTTON_OPEN) ? RELAY_STOP : RELAY_CLOSE);
        } 
      } 
      else {
        button_state |= BUTTON_CLOSE_START;
      }
    }
    else {
      button_state &= BUTTON_OPEN_MASK; // Clear button close bits
    }
  }

  vTaskDelete(NULL);
}

static TaskHandle_t   button_handle[DOOR_COUNT];
static TaskHandle_t    relay_handle[DOOR_COUNT];
static TaskHandle_t   relays_handle;

static uint8_t            doors_idx[DOOR_COUNT];

bool start_doors_control()
{
  char process_name[16];
  BaseType_t result;

  strcpy(process_name, "buttons_");

  ESP_LOGI(TAG, "Starting Doors Control Processes.");

  for (uint8_t idx = 0; idx < DOOR_COUNT; idx++) {

    button_handle[idx] = NULL;

    if (doors_config.doors[idx].enabled) {
      process_name[13] = '0' + idx;
      process_name[14] = 0;
      doors_idx[idx] = idx;

      result = xTaskCreatePinnedToCore(
        &door_button_control_process, 
        process_name, 
        2048, 
        &doors_idx[idx],  // Must be statically defined for the life of the process 
        5, 
        &button_handle[idx], 
        1);               // core id

      if (result != pdPASS) {
        ESP_LOGE(TAG, "Unable to start door %d button control process.", idx);
        return false;
      }
    }
  }

  strcpy(process_name, "relay_");

  relays_control_queue = xQueueCreate(20, 1);
       
  result = xTaskCreatePinnedToCore(
    &relays_control_process,          // low_level relays control (at gpio level)
    "relays", 
    2048, 
    NULL,  // Must be statically defined for the life of the process 
    5, 
    &relays_handle, 
    1);               // core id

  for (uint8_t idx = 0; idx < DOOR_COUNT; idx++) {

    relay_command_queue[idx] = xQueueCreate(10, sizeof(relay_command));
    relay_handle[idx] = NULL;

    if (doors_config.doors[idx].enabled) {
      process_name[13] = '0' + idx;
      process_name[14] = 0;
      doors_idx[idx] = idx;

       result = xTaskCreatePinnedToCore(
        &door_relay_control_process,  // relays control (at command level)
        process_name, 
        2048, 
        &doors_idx[idx],  // Must be statically defined for the life of the process 
        5, 
        &relay_handle[idx], 
        1);               // core id

      if (result != pdPASS) {
        ESP_LOGE(TAG, "Unable to start door %d relay control process (%d).", idx, result);
        return false;
      }
    }
  }

  return true;
}

void stop_doors_control()
{
  ESP_LOGI(TAG, "Stopping Doors Control Processes.");

  for (int idx = 0; idx < DOOR_COUNT; idx++) {
    vTaskDelete(button_handle[idx]);
    vTaskDelete(relay_handle[idx]);
  }

  vTaskDelete(relays_handle);
}

void init_doors_control()
{

}