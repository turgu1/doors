#ifndef _VERSION_H_
#define _VERSION_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#ifdef VERSION
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

PUBLIC char version[4] = "1.2";

#undef PUBLIC
#endif
