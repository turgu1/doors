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

PUBLIC char message_0[MESSAGE_SIZE];
PUBLIC char message_1[MESSAGE_SIZE];
PUBLIC char severity_0[10];
PUBLIC char severity_1[10];

PUBLIC void start_http_server();
PUBLIC void init_http_server();

#undef PUBLIC
#endif