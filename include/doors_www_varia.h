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

PUBLIC int varia_update(char ** hdr, www_packet_struct ** pkts);
PUBLIC int varia_edit(char ** hdr, www_packet_struct ** pkts);

#undef PUBLIC
#endif
