#ifndef _DOORS_WWW_VARIA_H_
#define _DOORS_WWW_VARIA_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW_VARIA
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC www_packet_struct * varia_update();
PUBLIC www_packet_struct *   varia_edit();

#undef PUBLIC
#endif
