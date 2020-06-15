#include "doors.h"
#include "doors_config.h"
#include "doors_www.h"
#include "www_support.h"
#include "secure.h"

#include <stdlib.h>

#define DOORS_WWW_DOOR 1
#include "doors_www_door.h"

static const char * TAG = "DOORS_WWW_DOOR";

static uint8_t door_idx, door_nbr;
static char    enabled[8];
static uint8_t button_open, button_close, relay_open, relay_close;
static char    seq_open_str[100], seq_close_str[100];
static uint8_t seq_open[SEQ_SIZE], seq_close[SEQ_SIZE];
static char    name[NAME_SIZE];
static bool    enabled_value = false;

static field_struct door_fields[14] = {
  { &door_fields[ 1], STR,  "ENABLED",    enabled    },
  { &door_fields[ 2], STR,  "NAME",       NULL       },
  { &door_fields[ 3], BYTE, "BOPEN",      NULL       },
  { &door_fields[ 4], BYTE, "BCLOSE",     NULL       },
  { &door_fields[ 5], BYTE, "ROPEN",      NULL       },
  { &door_fields[ 6], BYTE, "RCLOSE",     NULL       },
  { &door_fields[ 7], STR,  "SOPEN",      NULL       },
  { &door_fields[ 8], STR,  "SCLOSE",     NULL       },
  { &door_fields[ 9], INT,  "DOOR_NBR",   &door_nbr  },
  { &door_fields[10], INT,  "DOOR_IDX",   &door_idx  },
  { &door_fields[11], STR,  "MSG_0",      message_0  },
  { &door_fields[12], STR,  "MSG_1",      message_1  },
  { &door_fields[13], STR,  "SEVERITY_0", severity_0 },
  { NULL,             STR,  "SEVERITY_1", severity_1 }
};

int door_update(char ** hdr, packet_struct ** pkts)
{
  char * field;

  if (!get_bool("ENABLED", &enabled_value  )) field = "Actif";
  if ((!get_byte("DOOR_IDX",&door_idx) || (door_idx >= DOOR_COUNT)) && (field == NULL)) field = "Id Porte";
  if (!get_str( "NAME",     name, NAME_SIZE) && (field == NULL)) field = "Nom";
  if (!get_byte("BOPEN",   &button_open    ) && (field == NULL)) field = "GPIO Bouton Ouvrir";
  if (!get_byte("BCLOSE",  &button_close   ) && (field == NULL)) field = "GPIO Bouton Fermer";
  if (!get_byte("ROPEN",   &relay_open     ) && (field == NULL)) field = "Relais Ouvrir";
  if (!get_byte("RCLOSE",  &relay_close    ) && (field == NULL)) field = "Relais Fermer";

  if (!get_str( "SOPEN",    seq_open_str,  100 ) && (field == NULL)) field = "Séquence Ouvrir";
  else {
    memset(seq_open, 0, SEQ_SIZE);
    if (!parse_seq(seq_open, seq_open_str, SEQ_SIZE) && (field == NULL)) field = "Séquence Ouvrir";
  }

  if (!get_str( "SCLOSE",   seq_close_str, 100 ) && (field == NULL)) field = "Séquence Fermer";
  else {
    memset(seq_close, 0, SEQ_SIZE);
    if (!parse_seq(seq_close, seq_close_str, SEQ_SIZE) && (field == NULL)) field = "Séquence Fermer";
  }

  if (field == NULL) {
    // Save values
    door_nbr = door_idx + 1;
    memset(doors_config.doors[door_idx].name, 0, NAME_SIZE);
    strcpy(doors_config.doors[door_idx].name, name);
    
    doors_config.doors[door_idx].enabled           = enabled_value;
    doors_config.doors[door_idx].gpio_button_open  = button_open;
    doors_config.doors[door_idx].gpio_button_close = button_close;
    doors_config.doors[door_idx].gpio_relay_open   = relay_open;
    doors_config.doors[door_idx].gpio_relay_close  = relay_close;

    memcpy(doors_config.doors[door_idx].seq_open, seq_open, SEQ_SIZE);
    memcpy(doors_config.doors[door_idx].seq_close, seq_close, SEQ_SIZE);

    seq_to_str(doors_config.doors[door_idx].seq_open,  seq_open_str,  SEQ_SIZE - 1);
    seq_to_str(doors_config.doors[door_idx].seq_close, seq_close_str, SEQ_SIZE - 1);

    strcpy(enabled, doors_config.doors[door_idx].enabled ? "checked" : "");
    
    door_fields[1].value =  doors_config.doors[door_idx].name;
    door_fields[2].value = &doors_config.doors[door_idx].gpio_button_open;
    door_fields[3].value = &doors_config.doors[door_idx].gpio_button_close;
    door_fields[4].value = &doors_config.doors[door_idx].gpio_relay_open;
    door_fields[5].value = &doors_config.doors[door_idx].gpio_relay_close;
    door_fields[6].value =  seq_open_str;
    door_fields[7].value =  seq_close_str;

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

    strcpy(enabled, enabled_value ? "checked" : "");

    door_nbr = door_idx + 1;

    door_fields[1].value =  name;
    door_fields[2].value = &button_open;
    door_fields[3].value = &button_close;
    door_fields[4].value = &relay_open;
    door_fields[5].value = &relay_close;
    door_fields[6].value =  seq_open_str;
    door_fields[7].value =  seq_close_str;
  }

  int size;

  *hdr = http_html_hdr;
  *pkts = prepare_html("/spiffs/www/doorcfg.html", door_fields, &size);              

  return size;
}

int door_edit(char ** hdr, packet_struct ** pkts)
{
  int size = 0;

  if (get_byte("door", &door_idx)) {
    door_nbr = door_idx + 1;
    if (door_idx >= DOOR_COUNT) {
      ESP_LOGE(TAG, "Door number not valid: %d", door_idx);
    }
    else {
      seq_to_str(doors_config.doors[door_idx].seq_open,  seq_open_str,  SEQ_SIZE - 1);
      seq_to_str(doors_config.doors[door_idx].seq_close, seq_close_str, SEQ_SIZE - 1);

      strcpy(enabled, doors_config.doors[door_idx].enabled ? "checked" : "");

      door_fields[1].value =  doors_config.doors[door_idx].name;
      door_fields[2].value = &doors_config.doors[door_idx].gpio_button_open;
      door_fields[3].value = &doors_config.doors[door_idx].gpio_button_close;
      door_fields[4].value = &doors_config.doors[door_idx].gpio_relay_open;
      door_fields[5].value = &doors_config.doors[door_idx].gpio_relay_close;
      door_fields[6].value =  seq_open_str;
      door_fields[7].value =  seq_close_str;

      *hdr = http_html_hdr;
      *pkts = prepare_html("/spiffs/www/doorcfg.html", door_fields, &size);              
    }
  }
  else {
    ESP_LOGE(TAG, "Param unknown: door.");
  }

  return size; 
}
