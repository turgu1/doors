#ifndef _DOORS_NET_H_
#define _DOORS_NET_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_NET
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC bool start_network();
PUBLIC bool check_network();

#undef PUBLIC
#endif