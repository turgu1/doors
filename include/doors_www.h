#ifndef _DOORS_WWW_H_
#define _DOORS_WWW_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC bool start_http_server();
PUBLIC bool init_http_server();

#undef PUBLIC
#endif