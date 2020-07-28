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

PUBLIC www_packet_struct * testgpio_update();
PUBLIC www_packet_struct *   testgpio_edit();

#undef PUBLIC
#endif
