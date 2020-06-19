#define TEST 0

#if !TEST

  #include "doors.h"

#else

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <stdint.h>

  typedef enum { false, true } bool;

  #define ESP_LOGE(tag, ...) { \
    fputs("E ", stderr); \
    fputs(tag, stderr); \
    fputs(": ", stderr); \
    fprintf(stderr, __VA_ARGS__); \
  }

  #define ESP_LOGI(tag, ...) { \
    fputs("I ", stderr); \
    fputs(tag, stderr); \
    fputs(": ", stderr); \
    fprintf(stderr, __VA_ARGS__); \
  }
#endif

#define WWW_SUPPORT 1
#include "www_support.h"

static const char * TAG = "WWW_SUPPORT";

// ---- parameters extraction ---------------------------------------------------------------------

// url parameter extraction struc
static struct {
  char id[16];
  char value[200];
} params[15];
static int param_count;

static char hex(char ch)
{
  if ((ch >= '0') && (ch <= '9')) return ch - '0';
  if ((ch >= 'a') && (ch <= 'f')) return 10 + (ch - 'a');
  return 10 + (ch - 'A');
}

// Extract parameters from url string. Parameters are separated from the
// server location/command selection part of the url with character '?'.
// Every parameter is separated from each other with character '&'.
//
// Parameter:
//    char * str
//      URL to parse
//
// Return value: int
//    number of parameters parsed.

void www_extract_params(char * str, bool get)
{
  int idx = 0;

  ESP_LOGI(TAG, "Extracting parameters from: %s.", str);

  if (get) while (*str && (*str != '?')) str++;
  if (*str) {
    if (get) *str++ = 0;
    while (*str && (idx < 15)) {
      int len = 0;
      while ((isalnum(*str) || (*str == '_')) && (len < 15)) params[idx].id[len++] = *str++;
      params[idx].id[len] = 0;
      while (*str && (*str != '&') && (*str != '=')) str++;
      len = 0;
      if (*str == '=') {
        str++;
        while ((len < 199) && (*str) && (*str != '&')) {
          if (*str == '+') {
            params[idx].value[len++] = ' ';
            str++;
          }
          else if (*str == '%') {
            str++;
            char ch = 0;
            if (*str && (*str != '&')) ch = hex(*str++);
            if (*str && (*str != '&')) ch = (ch << 4) + hex(*str++);
            params[idx].value[len++] = ch;
          }
          else {
            params[idx].value[len++] = *str++;
          }
        }
        while (*str && (*str != '&')) str++;
      }
      params[idx].value[len] = 0;
      ESP_LOGI(TAG, "Param %d: %s = %s.", idx, params[idx].id, params[idx].value);
      idx++;
      if (*str == '&') str++;
    } 
  } 

  param_count = idx;
}

bool www_get_int(char * id, int * val)
{
  int idx = 0;

  while ((idx < param_count) && (strcmp(id, params[idx].id) != 0)) idx++;
  if (idx < param_count) {
    *val = atoi(params[idx].value);
    return true;
  }
  return false;
}

bool www_get_short(char * id, uint16_t * val)
{
  int idx = 0;
  int v;

  while ((idx < param_count) && (strcmp(id, params[idx].id) != 0)) idx++;
  if (idx < param_count) {
    v = atoi(params[idx].value);
    if ((v >= 0) && (v <= 65535)) {
      *val = v;
      return true;
    }
    else {
      return false;
    }
  }
  return false;
}

bool www_get_byte(char * id, uint8_t * val)
{
  int idx = 0;
  int v;

  while ((idx < param_count) && (strcmp(id, params[idx].id) != 0)) idx++;
  if (idx < param_count) {
    v = atoi(params[idx].value);
    if ((v >= 0) && (v <= 255)) {
      *val = v;
      return true;
    }
    else {
      return false;
    }
  }
  return false;
}

bool www_get_bool(char * id, bool * val)
{
  int idx = 0;
  int v;

  while ((idx < param_count) && (strcmp(id, params[idx].id) != 0)) idx++;
  if (idx < param_count) {
    v = atoi(params[idx].value);
    if ((v == 0) || (v == 1)) {
      *val = (v == 1);
      return true;
    }
  }

  *val = false;
  return true;
}

bool www_get_str(char * id, char * val, int size)
{
  int idx = 0;

  while ((idx < param_count) && (strcmp(id, params[idx].id) != 0)) idx++;
  if (idx < param_count) {
    if (strlen(params[idx].value) < size) {
      strcpy(val, params[idx].value);
      return true;
    }
    else {
      return false;
    }
  }
  return false;
}

// ---- HTML Preparation --------------------------------------------------------------------------

// We read file into buffer 256 character at a time
static char buffer[256];
static int  file_size;
static int  file_pos;
static int  buffer_size;
static int  buffer_pos;

// Packets of characters prepared for output to the www client
static www_packet_struct packets[MAX_PACKET_COUNT];
static int packet_idx;
static int packet_pos;

static FILE *file;

static char get_char()
{
  if (file_pos >= file_size) return 0;
  if (buffer_pos >= buffer_size) {
    buffer_size = file_size - file_pos;
    if (buffer_size > 256) buffer_size = 256;
    if (fread(buffer, 1, buffer_size, file) != buffer_size) {
      file_pos = file_size;
      return 0;
    };
    buffer_pos = 0;
  }
  file_pos++;
  return buffer[buffer_pos++];
}

static void put_char(char ch)
{
  if (packet_pos >= PACKET_SIZE) {
    if (++packet_idx >= MAX_PACKET_COUNT) {
      ESP_LOGE(TAG, "Not enough available packets!");
      return;
    }
    
    if (packets[packet_idx].buff != NULL) {
      ESP_LOGE(TAG, "Internal error. Packet buffer %d already allocated.", packet_idx);
      return;
    }

    packets[packet_idx].buff = (char *) malloc(PACKET_SIZE);
    packet_pos = 0;
  }
  packets[packet_idx].buff[packet_pos++] = ch;
  packets[packet_idx].size = packet_pos;
}

static void put_int(int val)
{
  int i = 0;
  char tmp[12];

  if (val < 0) {
    val = - val;
    put_char('-');
  }
  do {
    tmp[i++] = '0' + (val % 10);
    val = val / 10;
  } while (val > 0);
  do {
    put_char(tmp[--i]);
  } while (i > 0);
}

static void free_packets()
{
  for (int i = 0; i < MAX_PACKET_COUNT; i++) {
    if (packets[i].buff != NULL) free(packets[i].buff);
    packets[i].buff = NULL;
    packets[i].size = 0;
  }
}

www_packet_struct * www_prepare_html(char * filename, www_field_struct * fields, int * size)
{
  ESP_LOGI(TAG, "Preparing page %s.", filename);

  file = fopen(filename, "r");
 
  if (file == NULL) {
    ESP_LOGE(TAG, "File %s does not exists.", filename);
    return NULL;
  }

  file_pos = 0;

  fseek(file, 0L, SEEK_END);
  file_size = ftell(file);
  rewind(file);
  buffer_size = buffer_pos = 0;

  www_packet_struct * pkts = NULL;
  free_packets();
  packet_idx = -1;
  packet_pos = PACKET_SIZE;

  char ch;

  #define GET_CHAR if ((ch = get_char()) == 0) { \
    ESP_LOGE(TAG, "EOF Encountered, not expected for file %s.", filename); \
    goto eof; \
  }

  while ((ch = get_char()) != 0) {

    if (ch == '#') {
      GET_CHAR;
      if (ch == '{') {
        char id[32];
        int idx = 0;
        GET_CHAR;
        while (ch == ' ') GET_CHAR;
        while ((ch != ' ') && (ch != '}') && (idx < 31)) {
          id[idx++] = ch;
          GET_CHAR;
        }

        while (ch != '}') GET_CHAR;
        id[idx] = 0;
        www_field_struct * field = fields;

        while (field != NULL) {
          if (strcasecmp(id, field->id) == 0) break;
          field = field->next;
        }
        if (field == NULL) {
          ESP_LOGE(TAG, "No field definition for %s in file %s.", id, filename);
        }
        else {
          char * str;
          switch (field->type) {
            case INT:
              put_int(* (int *) field->value);
              break;
            case SHORT:
              put_int(* (uint16_t *) field->value);
              break;
            case BYTE:
              put_int(* (uint8_t *) field->value);
              break;
            case BOOL:
              if (* (bool *) field->value) {
                str = "checked";
                while (*str) put_char(*str++);
              }
              break;
            case STR:
              str = (char *) field->value;
              while (*str) put_char(*str++);
              break;
          }
        }
      }
      else {
        put_char('#');
        put_char(ch);
      }
    }
    else {
      put_char(ch);
    }
  }

  pkts = packets;
  int siz = 0;
  while (pkts->size > 0) {
    siz += pkts->size;
    pkts++;
  }
  *size = siz;

  pkts = packets;

eof:
  fclose(file);
  return pkts;
}

void init_www_support()
{
  for (int i = 0; i < MAX_PACKET_COUNT; i++) {
    packets[i].buff = NULL;
    packets[i].size = 0;
  }
}

#if TEST

  int     int_val  = 1234;
  uint8_t byte_val = 123;
  bool    bool_val = true;
  char    str_val[9]  = "Allo!!!!";

  field_struct fields[4] = {
    { &fields[1], INT,  "A_INT",  &int_val },
    { &fields[2], BYTE, "A_BYTE", &byte_val},
    { &fields[3], BOOL, "A_BOOL", &bool_val},
    { NULL,       STR,  "A_STR",   str_val }
  };

  int main(int argc, char **argv)
  {
    init_www_support();

    packet_struct * pkts = prepare_html(argv[1], fields);

    if (pkts != NULL) {
      int i = 0;
      while ((i < MAX_PACKET_COUNT) && (pkts[i].size != 0)) {
        fwrite(pkts[i].buff, 1, pkts[i].size, stdout);
        i++;
      }
      printf("\nPacket count: %d\n", i);

    }
    return 0;
  }

#endif