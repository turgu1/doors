#ifndef _DOORS_CONTROL_H_
#define _DOORS_CONTROL_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_CONTROL
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

typedef enum { RELAY_OPEN, RELAY_CLOSE, RELAY_STOP, RELAY_IDLE } relay_command;

PUBLIC void add_relay_command(uint8_t door_idx, relay_command command);
PUBLIC bool start_doors_control();
PUBLIC void stop_doors_control();
PUBLIC void init_doors_control();

#undef PUBLIC
#endif
