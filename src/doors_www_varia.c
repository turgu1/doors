#include "doors.h"
#include "doors_config.h"
#include "doors_www.h"
#include "www_support.h"

#include <stdlib.h>

#define DOORS_WWW_VARIA 1
#include "doors_www_varia.h"

static const char * TAG = "DOORS_WWW_VARIA";

static uint16_t relay_abort_length;

www_field_struct varia_fields[5] = {
  { &varia_fields[1], SHORT, "RALEN",      &relay_abort_length  },
  { &varia_fields[2], STR,   "MSG_0",      message_0            },
  { &varia_fields[3], STR,   "MSG_1",      message_1            },
  { &varia_fields[4], STR,   "SEVERITY_0", severity_0           },
  { NULL,             STR,   "SEVERITY_1", severity_1           }
};

www_packet_struct * varia_update()
{
  char * field = NULL;

  if (!www_get_short("RALEN",  &relay_abort_length )) field = "Durée arrêt relais";

  if (field == NULL) {
    ESP_LOGD(TAG, "Fields OK. Saving modifications.");

    doors_config.relay_abort_length  = relay_abort_length;

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
    ESP_LOGW(TAG, "Some field in error: %s.", field);

    strcpy(message_1,  "Champ en erreur: ");
    strcat(message_1,  field);
    strcpy(severity_1, "error");
  }

  return www_prepare_html("/spiffs/www/variacfg.html", varia_fields, true);         
}

www_packet_struct * varia_edit()
{
  relay_abort_length  = doors_config.relay_abort_length;

  return www_prepare_html("/spiffs/www/variacfg.html", varia_fields, true);
}
