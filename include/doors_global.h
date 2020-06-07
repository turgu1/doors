#ifndef _DOORS_GLOBAL_H_
#define _DOORS_GLOBAL_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if GLOBAL
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

typedef enum { IDDLE, OPENING, CLOSING, STOPPED, DISABLE } DOOR_STATE;

struct state_struct {
  volatile bool config_ok;
  volatile bool stopped;   // When TRUE, no control is allowed
  volatile DOOR_STATE door_states[DOOR_COUNT];
};

PUBLIC struct state_struct state;

#undef PUBLIC
#endif
