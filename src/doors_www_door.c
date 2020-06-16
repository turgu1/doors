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
static uint8_t conn_buttons, conn_relays;
static char    seq_open_str[181],  seq_close_str[181];
static seq_t   seq_open[SEQ_SIZE], seq_close[SEQ_SIZE];
static char    name[NAME_SIZE];
static bool    enabled = false;

static field_struct door_fields[14] = {
  { &door_fields[ 1], BOOL, "ENABLED",    &enabled     },
  { &door_fields[ 2], STR,  "NAME",       name         },
  { &door_fields[ 3], BYTE, "CBUTTON",   &conn_buttons },
  { &door_fields[ 6], BYTE, "CRELAY",    &conn_relays  },
  { &door_fields[ 7], STR,  "SOPEN",      seq_open_str },
  { &door_fields[ 8], STR,  "SCLOSE",     seq_close_str},
  { &door_fields[ 9], INT,  "DOOR_NBR",  &door_nbr     },
  { &door_fields[10], INT,  "DOOR_IDX",  &door_idx     },
  { &door_fields[11], STR,  "MSG_0",      message_0    },
  { &door_fields[12], STR,  "MSG_1",      message_1    },
  { &door_fields[13], STR,  "SEVERITY_0", severity_0   },
  { NULL,             STR,  "SEVERITY_1", severity_1   }
};

int door_update(char ** hdr, packet_struct ** pkts)
{
  char * field = NULL;

  if (!get_byte("DOOR_IDX", &door_idx) || (door_idx >= DOOR_COUNT)) field = "Id Porte";

  if (!get_bool("ENABLED",   &enabled        ) && (field == NULL)) field = "Actif";
  if (!get_str( "NAME",       name, NAME_SIZE) && (field == NULL)) field = "Nom";
  if (!get_byte("CBUTTON",   &conn_buttons   ) && (field == NULL)) field = "Connecteur Boutons";
  if (!get_byte("CRELAY",    &conn_relays    ) && (field == NULL)) field = "Connecteur Relais";

  if (!get_str( "SOPEN",    seq_open_str,  180 ) && (field == NULL)) field = "Séquence Ouvrir";
  else {
    memset(seq_open, 0, SEQ_SIZE * sizeof(seq_t));
    if (!parse_seq(seq_open, seq_open_str, SEQ_SIZE) && (field == NULL)) field = "Séquence Ouvrir";
  }

  if (!get_str( "SCLOSE",   seq_close_str, 180 ) && (field == NULL)) field = "Séquence Fermer";
  else {
    memset(seq_close, 0, SEQ_SIZE * sizeof(seq_t));
    if (!parse_seq(seq_close, seq_close_str, SEQ_SIZE) && (field == NULL)) field = "Séquence Fermer";
  }

  if (field == NULL) {
    // Save values
    ESP_LOGI(TAG, "Fields OK. Saving modifications.");
    
    door_nbr = door_idx + 1;

    memset(doors_config.doors[door_idx].name, 0, NAME_SIZE);
    strcpy(doors_config.doors[door_idx].name, name);
    
    doors_config.doors[door_idx].enabled      = enabled;
    doors_config.doors[door_idx].conn_buttons = conn_buttons;
    doors_config.doors[door_idx].conn_relays  = conn_relays;

    memcpy(doors_config.doors[door_idx].seq_open,  seq_open,  SEQ_SIZE * sizeof(seq_t));
    memcpy(doors_config.doors[door_idx].seq_close, seq_close, SEQ_SIZE * sizeof(seq_t));

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

    door_nbr = door_idx + 1;
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

      strcpy(name, doors_config.doors[door_idx].name);

      enabled      = doors_config.doors[door_idx].enabled;
      conn_buttons = doors_config.doors[door_idx].conn_buttons;
      conn_relays  = doors_config.doors[door_idx].conn_relays;

      *hdr  = http_html_hdr;
      *pkts = prepare_html("/spiffs/www/doorcfg.html", door_fields, &size);              
    }
  }
  else {
    ESP_LOGE(TAG, "Param unknown: door.");
  }

  return size; 
}
