#ifndef _DOORS_WWW_DOOR_H_
#define _DOORS_WWW_DOOR_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW_DOOR
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC www_packet_struct * door_update();
PUBLIC www_packet_struct *   door_edit();

#undef PUBLIC
#endif
