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

PUBLIC int door_update(char ** hdr, www_packet_struct ** pkts);
PUBLIC int door_edit(char ** hdr, www_packet_struct ** pkts);

#undef PUBLIC
#endif
