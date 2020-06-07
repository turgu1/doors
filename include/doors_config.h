#ifndef _DOORS_CONFIG_H_
#define _DOORS_CONFIG_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_CONFIG
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

#define DOORS_CONFIG_VERSION 1

struct door_struct {
  bool    enabled;
  char    name[NAME_SIZE];
  uint8_t gpio_button_open;
  uint8_t gpio_button_close;
  uint8_t gpio_relay_open;
  uint8_t gpio_relay_close;
  uint8_t seq_open[SEQ_SIZE];   // end of list marqued with 255
  uint8_t seq_close[SEQ_SIZE];  // idem
};

struct network_struct {
  char    ssid[SSID_SIZE];
  char    psw[PSW_SIZE];
  uint8_t ip[4];
  uint8_t mask[4];
  uint8_t router[4];
};

struct config_struct {
  uint8_t version;
  struct door_struct doors[DOOR_COUNT];
  struct network_struct network;
  char     psw[PSW_SIZE];
  bool     setup;
  bool     valid;
  uint32_t crc32;
};

PUBLIC struct config_struct doors_config;

PUBLIC bool doors_validate_config();
PUBLIC bool doors_get_config();
PUBLIC bool doors_save_config();

#undef PUBLIC
#endif