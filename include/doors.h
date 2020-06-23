#ifndef _DOORS_H_
#define _DOORS_H_

#if DOORS
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_spi_flash.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define DOOR_COUNT 5

// All the following sizes must be a multiple of 4

#define SEQ_SIZE     (19 + 1) // -1 = end of list
#define NAME_SIZE    (31 + 1)
#define SSID_SIZE    (21 + 1)
#define PWD_SIZE     (15 + 1)
#define IP_SIZE      (15 + 1)
#define MESSAGE_SIZE (59 + 1)

#define DEFAULT_IP   "192.168.1.1"
#define DEFAULT_GW   "192.168.1.1"
#define DEFAULT_MASK "255.255.255.0"

#define GPIO_LED_MAIN_STATE 13
#define GPIO_LED_ERROR      12

typedef enum { IDDLE, OPENING, CLOSING, STOPPED, DISABLE } DOOR_STATE;
typedef enum { STOP, RUN } MAIN_STATE;
typedef enum { NONE, INFO, WARNING, ERROR } SEVERITY;

struct state_struct {
  volatile bool config_ok;
  volatile MAIN_STATE main_state;   // When STOP, no control is allowed
  volatile DOOR_STATE door_states[DOOR_COUNT];
};

PUBLIC char message_0[MESSAGE_SIZE];
PUBLIC char message_1[MESSAGE_SIZE];
PUBLIC char severity_0[10];
PUBLIC char severity_1[10];

PUBLIC struct state_struct state;

PUBLIC void set_state_led_on();
PUBLIC void set_state_led_off();
PUBLIC void set_error_led_on();
PUBLIC void set_error_led_off();

PUBLIC void   set_main_state(MAIN_STATE new_state);
PUBLIC void set_main_message(char * msg1, char * msg2, SEVERITY severity);

#undef PUBLIC
#endif
