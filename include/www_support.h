#ifndef _WWW_SUPPORT_H_
#define _WWW_SUPPORT_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if WWW_SUPPORT
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

#define MAX_PACKET_COUNT  20
#define PACKET_SIZE      512

typedef enum { INT, BYTE, BOOL, STR } field_type;

typedef struct fld_struct {
  struct fld_struct * next;
  field_type type;
  char * name;
  void * value;
} field_struct;

typedef struct pkt_struct {
  int size;
  char * buff;
} packet_struct;

PUBLIC packet_struct * prepare_html(char * filename, 
                                    field_struct * fields,
                                    int * size);
PUBLIC void init_www_support();

#undef PUBLIC
#endif