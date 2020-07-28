#ifndef _DOORS_WWW_NET_H_
#define _DOORS_WWW_NET_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW_NET
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC www_packet_struct * net_update();
PUBLIC www_packet_struct *   net_edit();

#undef PUBLIC
#endif
