#ifndef _DOORS_WWW_TESTGPIO_H_
#define _DOORS_WWW_TESTGPIO_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW_TESTGPIO
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC int testgpio_update(char ** hdr, www_packet_struct ** pkts);
PUBLIC int testgpio_edit(char ** hdr, www_packet_struct ** pkts);

#undef PUBLIC
#endif
