#ifndef _DOORS_WWW_SEC_H_
#define _DOORS_WWW_SEC_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW_SEC
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC www_packet_struct * sec_update();
PUBLIC www_packet_struct *   sec_edit();

#undef PUBLIC
#endif
