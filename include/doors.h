#ifndef _DOORS_H_
#define _DOORS_H_

#include <stdbool.h>
#include "esp_spi_flash.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define DOOR_COUNT 5
#define SEQ_SIZE   (19 + 1) // -1 = end of list
#define NAME_SIZE  (31 + 1)
#define SSID_SIZE  (21 + 1)
#define PSW_SIZE   (15 + 1)

#define TAG "Doors"  // Used with logger

#endif
