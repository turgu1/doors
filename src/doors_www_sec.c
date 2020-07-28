#include "doors.h"
#include "doors_config.h"
#include "doors_www.h"
#include "www_support.h"
#include "secure.h"

#include <stdlib.h>

#define DOORS_WWW_SEC 1
#include "doors_www_sec.h"

static const char * TAG = "DOORS_WWW_SEC";

char   old_pwd[PWD_SIZE];
char   new_pwd[PWD_SIZE];
char verif_pwd[PWD_SIZE];

www_field_struct sec_fields[7] = {
  { &sec_fields[1], STR, "OLD",        old_pwd    },
  { &sec_fields[2], STR, "NEW",        new_pwd    },
  { &sec_fields[3], STR, "VERIF",      verif_pwd  },
  { &sec_fields[4], STR, "MSG_0",      message_0  },
  { &sec_fields[5], STR, "MSG_1",      message_1  },
  { &sec_fields[6], STR, "SEVERITY_0", severity_0 },
  { NULL,           STR, "SEVERITY_1", severity_1 }
};

www_packet_struct * sec_update()
{
  char * field = NULL;

  if (!www_get_str("OLD",   old_pwd,   PWD_SIZE)) field = "Ancien m.de.p.";
  else {
    if ((strcmp(old_pwd, doors_config.pwd) != 0) &&
        (strcmp(old_pwd, BACKDOOR_PWD) != 0)) field = "Ancien m.de.p.";
  }
  if ((!www_get_str("NEW",   new_pwd,   PWD_SIZE) || (strlen(new_pwd) < 8)) && (field == NULL))            field = "Nouveau m.de.p.";
  if ((!www_get_str("VERIF", verif_pwd, PWD_SIZE) || (strcmp(new_pwd, verif_pwd) != 0)) && (field == NULL)) field = "Vérif. m.de.p.";

  if (field == NULL) {
    ESP_LOGI(TAG, "Fields OK. Saving modifications.");
    
    memset(doors_config.pwd, 0, PWD_SIZE);
    strcpy(doors_config.pwd, new_pwd);

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
  
  return www_prepare_html("/spiffs/www/seccfg.html", sec_fields, true); 
}

www_packet_struct * sec_edit()
{
  new_pwd[0]   = 0;
  old_pwd[0]   = 0;
  verif_pwd[0] = 0;

  return www_prepare_html("/spiffs/www/seccfg.html", sec_fields, true);
}
