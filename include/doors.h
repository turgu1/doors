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

#include "esp_spi_flash.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define DOOR_COUNT 5
#define SEQ_SIZE   (19 + 1) // -1 = end of list
#define NAME_SIZE  (31 + 1)
#define SSID_SIZE  (21 + 1)
#define PSW_SIZE   (15 + 1)

PUBLIC char error_msg[256];

#undef PUBLIC
#endif
