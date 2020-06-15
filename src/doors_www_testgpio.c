#include "doors.h"
#include "doors_config.h"
#include "doors_www.h"
#include "www_support.h"
#include "secure.h"

#include <stdlib.h>

#define DOORS_WWW_TESTGPIO 1
#include "doors_www_testgpio.h"

//static const char * TAG = "DOORS_WWW_TESTGPIO";

static uint8_t gpio;

static field_struct testgpio_fields[5] = {
  { &testgpio_fields[1], BYTE, "GPIO",       &gpio      },
  { &testgpio_fields[2], STR,  "MSG_0",      message_0  },
  { &testgpio_fields[3], STR,  "MSG_1",      message_1  },
  { &testgpio_fields[4], STR,  "SEVERITY_0", severity_0 },
  { NULL,                STR,  "SEVERITY_1", severity_1 }
};

int testgpio_update(char ** hdr, packet_struct ** pkts)
{
  char * field;

  if (!get_byte("GPIO", &gpio)) field = "GPIO";

  if (field == NULL) {

    if (doors_save_config()) {
      strcpy(message_1,  "Mise à jour complétée.");
      strcpy(severity_1, "info");
    }
    else {
      strcpy(message_1,  "ERREUR INTERNE!!");
      strcpy(severity_1, "error");
    }
  }
  else {
    strcpy(message_1,  "Champ en erreur: ");
    strcat(message_1,  field);
    strcpy(severity_1, "error");
  }
  
  int size;

  *hdr = http_html_hdr;
  *pkts = prepare_html("/spiffs/www/testgpio.html", testgpio_fields, &size);         
  
  return size;
}

int testgpio_edit(char ** hdr, packet_struct ** pkts)
{
  int size;

  gpio = 2;

  *hdr  = http_html_hdr;
  *pkts = prepare_html("/spiffs/www/testgpio.html", testgpio_fields, &size);

  return size;
}
