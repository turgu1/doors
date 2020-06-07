#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp32/rom/crc.h"

#include "doors.h"

#define DOORS_CONFIG 1
#include "doors_config.h"

static const uint8_t default_ip[4]   = { 192, 168,   1, 1 };
static const uint8_t default_mask[4] = { 255, 255, 255, 0 };

static void doors_init_config()
{
  config.setup = true;
  memset(config.psw, 0, PSW_SIZE);

  // Network

  memset(config.network.ssid, 0, SSID_SIZE);
  memset(config.network.psw,  0, PSW_SIZE);
  strcpy(config.network.ssid, "DOORS");
  strcpy(config.network.psw,  "DOORS");

  memcpy(config.network.ip,     default_ip,   4);
  memcpy(config.network.mask,   default_mask, 4);
  memcpy(config.network.router, default_ip,   4);

  // Doors

  for (int i = 0; i < DOOR_COUNT; i++) {
    memset(config.doors[i].name, 0, NAME_SIZE);
    config.doors[i].gpio_button_open  = 2;
    config.doors[i].gpio_button_close = 2;
    config.doors[i].gpio_relay_open   = 2;
    config.doors[i].gpio_relay_close  = 2;
    memset(config.doors[i].seq_open,  0, SEQ_SIZE);
    memset(config.doors[i].seq_close, 0, SEQ_SIZE);
  }
}

// Retrieve configuration from non-volatile memory.
// If crc32 is not OK, initialize the config and return FALSE.
// If crc32 is OK, return TRUE

bool doors_get_config()
{
  uint32_t crc32 = crc32_le(0, (uint8_t const *) &config, sizeof(config) - 4);
  if (config.crc32 != crc32) {
    doors_init_config();
    doors_save_config();

    return false;
  }  

  return true;
}

bool doors_save_config()
{
  uint32_t crc32 = crc32_le(0, (uint8_t const *) &config, sizeof(config) - 4);
  config.crc32 = crc32;

  return true;
}