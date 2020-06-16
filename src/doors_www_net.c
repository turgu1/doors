#include "doors.h"
#include "doors_config.h"
#include "doors_www.h"
#include "www_support.h"
#include "secure.h"

#include <stdlib.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

#include "tcpip_adapter.h"

#define DOORS_WWW_NET 1
#include "doors_www_net.h"

static const char * TAG = "DOORS_WWW_NET";

static union {
  uint32_t ip;
  uint8_t  ip_num[4];
} ip, mask, gw;

static char     wifi_ssid[SSID_SIZE];
static char     wifi_pwd[PWD_SIZE];
static uint16_t www_port;

static field_struct net_fields[19] = {
  { &net_fields[ 1], STR,  "SSID",       wifi_ssid                  },
  { &net_fields[ 2], STR,  "PWD",        wifi_pwd                   },
  { &net_fields[ 3], BYTE, "IP_0",       &ip.ip_num[0]              },
  { &net_fields[ 4], BYTE, "IP_1",       &ip.ip_num[1]              },
  { &net_fields[ 5], BYTE, "IP_2",       &ip.ip_num[2]              },
  { &net_fields[ 6], BYTE, "IP_3",       &ip.ip_num[3]              },
  { &net_fields[ 7], BYTE, "MASK_0",     &mask.ip_num[0]            },
  { &net_fields[ 8], BYTE, "MASK_1",     &mask.ip_num[1]            },
  { &net_fields[ 9], BYTE, "MASK_2",     &mask.ip_num[2]            },
  { &net_fields[10], BYTE, "MASK_3",     &mask.ip_num[3]            },
  { &net_fields[11], BYTE, "GW_0",       &gw.ip_num[0]              },
  { &net_fields[12], BYTE, "GW_1",       &gw.ip_num[1]              },
  { &net_fields[13], BYTE, "GW_2",       &gw.ip_num[2]              },
  { &net_fields[14], BYTE, "GW_3",       &gw.ip_num[3]              },
  { &net_fields[15], SHORT,"WWW_PORT",   &www_port                  },
  { &net_fields[16], STR,  "MSG_0",      message_0                  },
  { &net_fields[17], STR,  "MSG_1",      message_1                  },
  { &net_fields[18], STR,  "SEVERITY_0", severity_0                 },
  { NULL,            STR,  "SEVERITY_1", severity_1                 }
};

int net_update(char ** hdr, packet_struct ** pkts)
{
  char * field = NULL;

  if (!get_str( "SSID", wifi_ssid, SSID_SIZE) && (field == NULL)) field = "SSID";
  if (!get_str( "PWD",  wifi_pwd,  PWD_SIZE ) && (field == NULL)) field = "Mot de passe";

  if (!get_byte ("IP_0",     &ip.ip_num[0]  ) && (field == NULL)) field = "Adr. IP";
  if (!get_byte ("IP_1",     &ip.ip_num[1]  ) && (field == NULL)) field = "Adr. IP";
  if (!get_byte ("IP_2",     &ip.ip_num[2]  ) && (field == NULL)) field = "Adr. IP";
  if (!get_byte ("IP_3",     &ip.ip_num[3]  ) && (field == NULL)) field = "Adr. IP";
  if (!get_byte ("MASK_0",   &mask.ip_num[0]) && (field == NULL)) field = "Masque";
  if (!get_byte ("MASK_1",   &mask.ip_num[1]) && (field == NULL)) field = "Masque";
  if (!get_byte ("MASK_2",   &mask.ip_num[2]) && (field == NULL)) field = "Masque";
  if (!get_byte ("MASK_3",   &mask.ip_num[3]) && (field == NULL)) field = "Masque";
  if (!get_byte ("GW_0",     &gw.ip_num[0]  ) && (field == NULL)) field = "Routeur. IP";
  if (!get_byte ("GW_1",     &gw.ip_num[1]  ) && (field == NULL)) field = "Routeur. IP";
  if (!get_byte ("GW_2",     &gw.ip_num[2]  ) && (field == NULL)) field = "Routeur. IP";
  if (!get_byte ("GW_3",     &gw.ip_num[3]  ) && (field == NULL)) field = "Routeur. IP";
  if (!get_short("WWW_PORT", &www_port      ) && (field == NULL)) field = "Port WWW";

  if (field == NULL) {
    ESP_LOGI(TAG, "Fields OK. Saving modifications.");
    
    memset(doors_config.network.ssid, 0, SSID_SIZE);
    strcpy(doors_config.network.ssid, wifi_ssid);

    memset(doors_config.network.pwd, 0, PWD_SIZE);
    strcpy(doors_config.network.pwd, wifi_pwd);

    memset(doors_config.network.ip,   0, IP_SIZE);
    memset(doors_config.network.mask, 0, IP_SIZE);
    memset(doors_config.network.gw,   0, IP_SIZE);

    sprintf(doors_config.network.ip,   "%u.%u.%u.%u",   ip.ip_num[0],   ip.ip_num[1],   ip.ip_num[2],   ip.ip_num[3]);
    sprintf(doors_config.network.mask, "%u.%u.%u.%u", mask.ip_num[0], mask.ip_num[1], mask.ip_num[2], mask.ip_num[3]);
    sprintf(doors_config.network.gw,   "%u.%u.%u.%u",   gw.ip_num[0],   gw.ip_num[1],   gw.ip_num[2],   gw.ip_num[3]);

    doors_config.network.www_port = www_port;

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

  int size;

  *hdr = http_html_hdr;
  *pkts = prepare_html("/spiffs/www/netcfg.html", net_fields, &size);

  return size;
}

int net_edit(char ** hdr, packet_struct ** pkts)
{
  strcpy(wifi_ssid,  doors_config.network.ssid);
  strcpy(wifi_pwd,   doors_config.network.pwd);

  inet_pton(AF_INET, doors_config.network.ip,   &ip.ip);
  inet_pton(AF_INET, doors_config.network.gw,   &gw.ip);
  inet_pton(AF_INET, doors_config.network.mask, &mask.ip);

  int size;

  *hdr = http_html_hdr;
  *pkts = prepare_html("/spiffs/www/netcfg.html", net_fields, &size);

  return size;
}
