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

PUBLIC int sec_update(char ** hdr, www_packet_struct ** pkts);
PUBLIC int sec_edit(char ** hdr, www_packet_struct ** pkts);

#undef PUBLIC
#endif
