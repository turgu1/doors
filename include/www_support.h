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

#define MAX_PACKET_COUNT  40
#define PACKET_SIZE      512

typedef enum { INT, SHORT, BYTE, BOOL, STR } www_field_type;

typedef struct fld_struct {
  struct fld_struct * next;
  www_field_type type;
  char * id;
  void * value;
} www_field_struct;

typedef struct pkt_struct {
  int size;
  char * buff;
} www_packet_struct;

PUBLIC www_packet_struct * get_raw_file(char * filename);

PUBLIC www_packet_struct * www_prepare_html(char             * filename, 
                                            www_field_struct * fields,
                                            bool               header);
PUBLIC void init_www_support();

PUBLIC void www_clear_params();
PUBLIC void www_extract_params(char * str, bool get);

PUBLIC bool   www_get_int(char * id, int      * val);
PUBLIC bool www_get_short(char * id, uint16_t * val);
PUBLIC bool  www_get_byte(char * id, uint8_t  * val);
PUBLIC bool  www_get_bool(char * id, bool     * val);
PUBLIC bool   www_get_str(char * id, char     * val, int size);

#undef PUBLIC
#endif