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
typedef enum { STOP, RUN } MAIN_STATE;

struct state_struct {
  volatile bool config_ok;
  volatile MAIN_STATE main_state;   // When TRUE, no control is allowed
  volatile DOOR_STATE door_states[DOOR_COUNT];
};

PUBLIC struct state_struct state;
PUBLIC void change_main_state(MAIN_STATE new_state);

#undef PUBLIC
#endif
