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

PUBLIC int net_update(char ** hdr, www_packet_struct ** pkts);
PUBLIC int net_edit(char ** hdr, www_packet_struct ** pkts);

#undef PUBLIC
#endif
