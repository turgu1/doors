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
  uint8_t padding[3];
  char    name[NAME_SIZE];
  uint8_t gpio_button_open;
  uint8_t gpio_button_close;
  uint8_t gpio_relay_open;
  uint8_t gpio_relay_close;
  uint8_t seq_open[SEQ_SIZE];   // end of list marqued with 255
  uint8_t seq_close[SEQ_SIZE];  // idem
};

struct network_struct {
  char ssid[SSID_SIZE];
  char  pwd[PWD_SIZE];
  char   ip[IP_SIZE];
  char mask[IP_SIZE];
  char   gw[IP_SIZE];
};

struct config_struct {
  uint8_t               version;
  bool                  setup;
  uint8_t               padding[2];
  struct door_struct    doors[DOOR_COUNT];
  struct network_struct network;
  char                  pwd[PWD_SIZE];
  uint32_t              crc32;
};

PUBLIC struct config_struct doors_config;
PUBLIC void seq_to_str(uint8_t * seq, char * str, int max_size);
PUBLIC bool parse_seq(uint8_t * seq, char * str, int max_size);
PUBLIC bool doors_validate_config();
PUBLIC bool doors_get_config();
PUBLIC bool doors_save_config();

#undef PUBLIC
#endif