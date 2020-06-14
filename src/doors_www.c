#include "doors.h"
#include "www_support.h"
#include "doors_config.h"
#include "secure.h"

#define DOORS_WWW 1
#include "doors_www.h"

#include "driver/gpio.h"
#include "tcpip_adapter.h"

#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

static const char * TAG = "DOORS_WWW";

// Every field list must have MSG_1, SEVERITY_1, MSG_0 and SEVERITY_0 defined.

field_struct no_param_fields [4] = {
  { &no_param_fields[1], STR, "MSG_0",      message_0               },
  { &no_param_fields[2], STR, "MSG_1",      message_1               },
  { &no_param_fields[3], STR, "SEVERITY_0", severity_0              },
  { NULL,                STR, "SEVERITY_1", severity_1              }
};

field_struct index_fields[9] = {
  { &index_fields[1], STR, "NAME_0",     doors_config.doors[0].name },
  { &index_fields[2], STR, "NAME_1",     doors_config.doors[1].name },
  { &index_fields[3], STR, "NAME_2",     doors_config.doors[2].name },
  { &index_fields[4], STR, "NAME_3",     doors_config.doors[3].name },
  { &index_fields[5], STR, "NAME_4",     doors_config.doors[4].name },
  { &index_fields[6], STR, "MSG_0",      message_0                  },
  { &index_fields[7], STR, "MSG_1",      message_1                  },
  { &index_fields[8], STR, "SEVERITY_0", severity_0                 },
  { NULL,             STR, "SEVERITY_1", severity_1                 }
};

union {
  uint32_t ip;
  uint8_t  ip_num[4];
} ip, mask, gw;

char wifi_ssid[SSID_SIZE];
char wifi_pwd[PWD_SIZE];

field_struct net_fields[18] = {
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
  { &net_fields[15], STR,  "MSG_0",      message_0                  },
  { &net_fields[16], STR,  "MSG_1",      message_1                  },
  { &net_fields[17], STR,  "SEVERITY_0", severity_0                 },
  { NULL,            STR,  "SEVERITY_1", severity_1                 }
};

char old_pwd[PWD_SIZE];
char new_pwd[PWD_SIZE];
char verif_pwd[PWD_SIZE];

field_struct sec_fields[7] = {
  { &sec_fields[1], STR, "OLD",        old_pwd    },
  { &sec_fields[2], STR, "NEW",        new_pwd    },
  { &sec_fields[3], STR, "VERIF",      verif_pwd  },
  { &sec_fields[4], STR, "MSG_0",      message_0  },
  { &sec_fields[5], STR, "MSG_1",      message_1  },
  { &sec_fields[6], STR, "SEVERITY_0", severity_0 },
  { NULL,           STR, "SEVERITY_1", severity_1 }
};

int door_idx;
char seq_open[SEQ_SIZE];
char seq_close[SEQ_SIZE];

field_struct door_fields[12] = {
  { &door_fields[ 1], STR,  "NAME",       NULL       },
  { &door_fields[ 2], BYTE, "BOPEN",      NULL       },
  { &door_fields[ 3], BYTE, "BCLOSE",     NULL       },
  { &door_fields[ 4], BYTE, "ROPEN",      NULL       },
  { &door_fields[ 5], BYTE, "RCLOSE",     NULL       },
  { &door_fields[ 6], STR,  "SOPEN",      NULL       },
  { &door_fields[ 7], STR,  "SCLOSE",     NULL       },
  { &door_fields[ 8], INT,  "DOOR_NBR",   &door_idx  },
  { &door_fields[ 9], STR,  "MSG_0",      message_0  },
  { &door_fields[10], STR,  "MSG_1",      message_1  },
  { &door_fields[11], STR,  "SEVERITY_0", severity_0 },
  { NULL,             STR,  "SEVERITY_1", severity_1 }
};

// http headers
static char http_html_hdr[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/html\r\n"
  "Content-Length: %d\r\n\r\n";

static char http_xml_hdr[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: application/xml\r\n"
  "Content-Length: %d\r\n\r\n";

static char http_html_hdr_not_found[] =
  "HTTP/1.1 404 Not Found\r\n"
  "Content-type: text/html\r\n\r\n"
  "<html><body><p>Error 404: Command not found.</p></body></html>";

static char http_css_hdr[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/css\r\n"
  "Content-Length: %d\r\n\r\n";

static char http_png_hdr[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: image/png\r\n"
  "Content-Length: %d\r\n\r\n";

// url parameter extraction struc
struct {
  char name[16];
  char value[32];
} params[15];
int param_count;

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
static int extract_params(char * str, bool get)
{
  int idx = 0;

  if (get) while (*str && (*str != '?')) str++;
  if (*str) {
    if (get) *str++ = 0;
    while (*str && (idx < 15)) {
      int len = 0;
      while (isalpha(*str) && (len < 15)) params[idx].name[len++] = *str++;
      params[idx].name[len] = 0;
      while (*str && (*str != '&') && (*str != '=')) str++;
      len = 0;
      if (*str == '=') {
        str++;
        while ((len < 31) && (*str) && (*str != '&')) {
          if (*str == '+') {
            params[idx].value[len++] = ' ';
            str++;
          }
          else {
            params[idx].value[len++] = *str++;
          }
        }
        while (*str && (*str != '&')) str++;
      }
      params[idx].value[len] = 0;
      idx++;
      if (*str == '&') str++;
    } 
  } 

  return idx;
}

bool get_byte(char * label, uint8_t * val)
{
  int idx = 0;
  int v;

  while ((idx < param_count) && (strcmp(label, params[idx].name) != 0)) idx++;
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

bool get_str(char * label, char * val, int size)
{
  int idx = 0;

  while ((idx < param_count) && (strcmp(label, params[idx].name) != 0)) idx++;
  if (idx < param_count) {
    if (strlen(params[idx].value) < (size - 1)) {
      strcpy(val, params[idx].value);
      return true;
    }
    else {
      return false;
    }
  }
  return false;
}

static void send_header(struct netconn *conn, char * hdr, int size)
{
  static char buff[256];

  snprintf(buff, 256, hdr, size);
  netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
}

static void http_server_netconn_serve(struct netconn *conn)
{
  struct netbuf * inbuf;
  char          * buf;
  uint16_t        buflen;
  err_t           err;
  char          * png_buffer = NULL;

  // Read the data from the port, blocking if nothing yet there.
  // We assume the request (the part we care about) is in one netbuf
  
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void **) &buf, &buflen);

    ESP_LOGI(TAG, "Received the following packet:");
    fwrite(buf, 1, buflen, stdout);
    fputc('\n', stdout);
    fflush(stdout);

    // Is this an HTTP GET command? (only check the first 5 chars, since
    // there are other formats for GET, and we're keeping it very simple)

    char * path = NULL;
    char * line_end;
    int param_count;

    packet_struct * pkts = NULL;
    char          * hdr  = NULL;
    int             size;


    if ((buflen >= 5) && (strncmp("POST ", buf, 5) == 0)) {

      // POST /door_update HTTP/1.1
      // Host: 192.168.1.1
      // Origin: http://192.168.1.1
      // Content-Type: application/x-www-form-urlencoded
      // Accept-Encoding: gzip, deflate
      // Connection: keep-alive
      // Upgrade-Insecure-Requests: 1
      // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
      // User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_4) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.1 Safari/605.1.15
      // Referer: http://192.168.1.1/doorcfg?door=1
      // Content-Length: 72
      // Accept-Language: en-ca

      line_end = strchr(buf, '\n');

      if (line_end != NULL) {
        //Extract the path from HTTP GET request
        path = (char *) malloc(sizeof(char) * (line_end - buf + 1));
        int path_length = line_end - buf - strlen("POST ") - strlen("HTTP/1.1") - 2;
        strncpy(path, &buf[5], path_length);
        path[path_length] = '\0';

        //Get remote IP address
        ip_addr_t remote_ip;
        u16_t     remote_port;
        netconn_getaddr(conn, &remote_ip, &remote_port, 0);
        ESP_LOGI(TAG, "[ "IPSTR" ] POST %s", IP2STR(&(remote_ip.u_addr.ip4)), path);
      }

      netbuf_delete(inbuf);

      err = netconn_recv(conn, &inbuf);

      if ((err == ERR_OK) && (inbuf != NULL)) {
        netbuf_data(inbuf, (void **) &buf, &buflen);

        ESP_LOGI(TAG, "Received the following packet:");
        fwrite(buf, 1, buflen, stdout);
        fputc('\n', stdout);
        fflush(stdout);

        param_count = extract_params(buf, false);

        bool   complete;
        char * field;

        if (strcmp(path, "/door_update") == 0) {
          static uint8_t button_open, button_close, relay_open, relay_close;
          static char seq_open_str[100], seq_close_str[100];
          static uint8_t seq_open[SEQ_SIZE], seq_close[SEQ_SIZE];
          static char name[NAME_SIZE];
          static uint8_t door_idx;
          complete = false;

          while (true) {
            field = "Id Porte";           if (!get_byte("DOOR_NBR",&door_idx       )) break;
                                          if (door_idx > 4) break;
            field = "Nom";                if (!get_str( "NAME",     name, NAME_SIZE)) break;
            field = "GPIO Bouton Ouvrir"; if (!get_byte("BOPEN",   &button_open    )) break;
            field = "GPIO Bouton Fermer"; if (!get_byte("BCLOSE",  &button_close   )) break;
            field = "Relais Ouvrir";      if (!get_byte("ROPEN",   &relay_open     )) break;
            field = "Relais Fermer";      if (!get_byte("RCLOSE",  &relay_close    )) break;

            field = "Séquence Ouvrir";    if (!get_str( "SOPEN",    seq_open_str,  100 )) break;
                                          memset(seq_open, 0, SEQ_SIZE);
                                          if (!parse_seq(seq_open, seq_open_str, SEQ_SIZE)) break;

            field = "Séquence Fermer";    if (!get_str( "SCLOSE",   seq_close_str, 100 )) break;
                                          memset(seq_close, 0, SEQ_SIZE);
                                          if (!parse_seq(seq_close, seq_close_str, SEQ_SIZE)) break;

            complete = true;
            break;
          }
          if (complete) {
            // Save values
            memset(doors_config.doors[door_idx].name, 0, NAME_SIZE);
            strcpy(doors_config.doors[door_idx].name, name);
            
            doors_config.doors[door_idx].gpio_button_open  = button_open;
            doors_config.doors[door_idx].gpio_button_close = button_close;
            doors_config.doors[door_idx].gpio_relay_open   = relay_open;
            doors_config.doors[door_idx].gpio_relay_close  = relay_close;

            memcpy(doors_config.doors[door_idx].seq_open, seq_open, SEQ_SIZE);
            memcpy(doors_config.doors[door_idx].seq_close, seq_close, SEQ_SIZE);

            seq_to_str(doors_config.doors[door_idx].seq_open,  seq_open_str,  SEQ_SIZE - 1);
            seq_to_str(doors_config.doors[door_idx].seq_close, seq_close_str, SEQ_SIZE - 1);

            door_fields[0].value =  doors_config.doors[door_idx].name;
            door_fields[1].value = &doors_config.doors[door_idx].gpio_button_open;
            door_fields[2].value = &doors_config.doors[door_idx].gpio_button_close;
            door_fields[3].value = &doors_config.doors[door_idx].gpio_relay_open;
            door_fields[4].value = &doors_config.doors[door_idx].gpio_relay_close;
            door_fields[5].value =  seq_open_str;
            door_fields[6].value =  seq_close_str;

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

            door_fields[0].value =  name;
            door_fields[1].value = &button_open;
            door_fields[2].value = &button_close;
            door_fields[3].value = &relay_open;
            door_fields[4].value = &relay_close;
            door_fields[5].value =  seq_open_str;
            door_fields[6].value =  seq_close_str;
          }

          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/doorcfg.html", door_fields, &size);              
        }
        else if (strcmp(path, "/sec_update") == 0) {
          complete = false;
          while (true) {
            field = "Ancien m.de.p.";  if (!get_str("OLD",   old_pwd,   PWD_SIZE)) break;
                                       if ((strcmp(old_pwd, doors_config.pwd) != 0) && 
                                           (strcmp(old_pwd, BACKDOOR_PWD) != 0)) break;

            field = "Nouveau m.de.p."; if (!get_str("NEW",   new_pwd,   PWD_SIZE)) break;
                                       if (strlen(new_pwd) <= 8) break;

            field = "Vérif. m.de.p.";  if (!get_str("VERIF", verif_pwd, PWD_SIZE)) break;
                                       if (strcmp(new_pwd, verif_pwd) != 0) break;
            complete = true;
            break;
          }
          if (complete) {
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
            strcpy(message_1,  "Champ en erreur: ");
            strcat(message_1,  field);
            strcpy(severity_1, "error");
          }
          
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/seccfg.html", sec_fields, &size);         
        }
        else if (strcmp(path, "/net_update") == 0) {
          complete = false;
          while (true) {
            field = "SSID";          if (!get_str( "SSID", wifi_ssid, SSID_SIZE)) break;
            field = "Mot de passe";  if (!get_str( "PWD",  wifi_pwd,  PWD_SIZE )) break;
            field = "Adr. IP";       if (!get_byte("IP_0",   &ip.ip_num[0]  )) break;
                                     if (!get_byte("IP_1",   &ip.ip_num[1]  )) break;
                                     if (!get_byte("IP_2",   &ip.ip_num[2]  )) break;
                                     if (!get_byte("IP_3",   &ip.ip_num[3]  )) break;
            field = "Masque";        if (!get_byte("MASK_0", &mask.ip_num[0])) break;
                                     if (!get_byte("MASK_1", &mask.ip_num[1])) break;
                                     if (!get_byte("MASK_2", &mask.ip_num[2])) break;
                                     if (!get_byte("MASK_3", &mask.ip_num[3])) break;
            field = "Routeur. IP";   if (!get_byte("GW_0",   &gw.ip_num[0]  )) break;
                                     if (!get_byte("GW_1",   &gw.ip_num[1]  )) break;
                                     if (!get_byte("GW_2",   &gw.ip_num[2]  )) break;
                                     if (!get_byte("GW_3",   &gw.ip_num[3]  )) break;
            complete = true;
            break;
          }
          if (complete) {
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
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/netcfg.html", net_fields, &size);
        }
      }
      else {
        ESP_LOGW(TAG, "Received nothing.");
      }
    }
    else if ((buflen >= 5) && (strncmp("GET ", buf, 4) == 0)) {

      // sample:
      // GET /l HTTP/1.1
      //
      // Accept: text/html, application/xhtml+xml, image/jxr,
      // Referer: http://192.168.1.222/h
      // Accept-Language: en-US,en;q=0.8,zh-Hans-CN;q=0.5,zh-Hans;q=0.3
      // User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.79 Safari/537.36 Edge/14.14393
      // Accept-Encoding: gzip, deflate
      // Host: 192.168.1.222
      // Connection: Keep-Alive

      //Parse URL
      
      line_end = strchr(buf, '\n');

      if (line_end != NULL) {
        //Extract the path from HTTP GET request
        path = (char *) malloc(sizeof(char) * (line_end - buf + 1));
        int path_length = line_end - buf - strlen("GET ") - strlen("HTTP/1.1") - 2;
        strncpy(path, &buf[4], path_length);
        path[path_length] = '\0';

        //Get remote IP address
        ip_addr_t remote_ip;
        u16_t     remote_port;
        netconn_getaddr(conn, &remote_ip, &remote_port, 0);
        ESP_LOGI(TAG, "[ "IPSTR" ] GET %s", IP2STR(&(remote_ip.u_addr.ip4)), path);
      }

      if (path != NULL) {

        param_count = extract_params(path, true);

        if (strcmp("/config", path) == 0) {
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/config.html", no_param_fields, &size);
        }
        else if (strcmp("/doorscfg", path) == 0) {
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/doorscfg.html", no_param_fields, &size);
        }
        else if (strcmp("/doorcfg", path) == 0) {
          if (strcmp(params[0].name, "door") == 0) {
            int door_idx = atoi(params[0].value);
            if ((door_idx < 0) || (door_idx >= 5)) {
              ESP_LOGE(TAG, "Door number not valid: %d", door_idx);
            }
            else {
              seq_to_str(doors_config.doors[door_idx].seq_open,  seq_open,  SEQ_SIZE - 1);
              seq_to_str(doors_config.doors[door_idx].seq_close, seq_close, SEQ_SIZE - 1);

              door_fields[0].value =  doors_config.doors[door_idx].name;
              door_fields[1].value = &doors_config.doors[door_idx].gpio_button_open;
              door_fields[2].value = &doors_config.doors[door_idx].gpio_button_close;
              door_fields[3].value = &doors_config.doors[door_idx].gpio_relay_open;
              door_fields[4].value = &doors_config.doors[door_idx].gpio_relay_close;
              door_fields[5].value =  seq_open;
              door_fields[6].value =  seq_close;

              hdr = http_html_hdr;
              pkts = prepare_html("/spiffs/www/doorcfg.html", door_fields, &size);              
            }
          }
          else {
            ESP_LOGE(TAG, "Param unknown: %s.", params[0].name);
          }
        }
        else if (strcmp("/netcfg", path) == 0) {
          strcpy(wifi_ssid,  doors_config.network.ssid);
          strcpy(wifi_pwd,   doors_config.network.pwd);
          inet_pton(AF_INET, doors_config.network.ip,   &ip.ip);
          inet_pton(AF_INET, doors_config.network.gw,   &gw.ip);
          inet_pton(AF_INET, doors_config.network.mask, &mask.ip);

          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/netcfg.html", net_fields, &size);
        }
        else if (strcmp("/seccfg", path) == 0) {
          new_pwd[0]   = 0;
          old_pwd[0]   = 0;
          verif_pwd[0] = 0;
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/seccfg.html", sec_fields, &size);
        }
        else if (strcmp("/style.css", path) == 0) {
          hdr = http_css_hdr;
          pkts = prepare_html("/spiffs/www/style.css", NULL, &size);
        }
        else if (strcmp("/open", path) == 0) {
          if ((param_count == 1) && (strcmp(params[0].name, "door") == 0)) {
            ESP_LOGI(TAG, "Open door %s", params[0].value);
            int idx = atoi(params[0].value);
            if ((idx >= 0) && (idx < 5)) {
              strcpy(message_1, "Ouverture de ");
              strcat(message_1, doors_config.doors[idx].name);
              strcpy(severity_1, "info");
              // open_door(idx)
            }
            else {
              ESP_LOGE(TAG, "Door number not valid: %d", idx);
              strcpy(message_1, "Porte non valide.");
              strcpy(severity_1, "error");
            }
          }
          else {
            ESP_LOGE(TAG, "Param unknown: %s.", params[0].name);
            strcpy(message_1, "Parametre non valide.");
            strcpy(severity_1, "error");
          }
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/index.html", index_fields, &size);
        }
        else if (strcmp("/close", path) == 0) {
          if (strcmp(params[0].name, "door") == 0) {
            ESP_LOGI(TAG, "Close door %s", params[0].value);
            int idx = atoi(params[0].value);
            if ((idx >= 0) && (idx < 5)) {
              strcpy(message_1, "Fermeture de ");
              strcat(message_1, doors_config.doors[idx].name);
              strcpy(severity_1, "info");
              // close_door(idx)
            }
            else {
              ESP_LOGE(TAG, "Door number not valid: %d", idx);
              strcpy(message_1, "Porte non valide.");
              strcpy(severity_1, "error");
            }
          }
          else {
            ESP_LOGE(TAG, "Param unknown: %s.", params[0].name);
            strcpy(message_1, "Parametre non valide.");
            strcpy(severity_1, "error");
          }
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/index.html", index_fields, &size);
        }
        else if ((strcmp("/index", path) == 0) || (strcmp("/", path) == 0)) {
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/index.html", index_fields, &size);
        }
        else if (strncmp("/favicon-", path, 9) == 0) {
          char * fname = (char *) malloc(strlen(path) + strlen("/spiffs/www") + 5);
          strcpy(fname, "/spiffs/www");
          strcat(fname, path);
          FILE *f = fopen(fname, "r");
          if (f != NULL) {
            fseek(f, 0L, SEEK_END);
            size = ftell(f);
            rewind(f);
            send_header(conn, http_png_hdr, size);
            png_buffer = (char *) malloc(size);
            fread(png_buffer, 1, size, f);
            fclose(f);

            int i = 0;
            while (i < size) {
              int s = size - i;
              if (s > 512) s = 512;
              netconn_write(conn, &png_buffer[i], s, NETCONN_NOCOPY);
              i += s;
            }
          }
          else {
            ESP_LOGE(TAG, "Unable to find file %s.", fname);
            hdr = http_html_hdr_not_found;
          }
          free(fname);
        }
        else if (strcmp("/browserconfig.xml", path) == 0) {
          hdr = http_xml_hdr;
          pkts = prepare_html("/spiffs/www/browserconfig.xml", NULL, &size);
        }
        else {
          ESP_LOGE(TAG, "Unknown path: %s.", path);
          hdr = http_html_hdr_not_found;
        }
      }
      else {
        hdr = http_html_hdr;
        pkts = prepare_html("/spiffs/www/index.html", index_fields, &size);
      }
    }

    // Send HTTP response header
    if (hdr) {
      send_header(conn, hdr, size);
      //netconn_write(conn, hdr, strlen(hdr), NETCONN_NOCOPY);
    }

    // Send HTML content
    if (pkts != NULL) {
      int i = 0;
      while ((i < MAX_PACKET_COUNT) && (pkts->size != 0)) {
        netconn_write(conn, pkts->buff, pkts->size, NETCONN_NOCOPY);
        i++;
        pkts++;
      }
    }

    message_1[0] = 0;
    strcpy(severity_1, "none");

    if (path != NULL) {
      free(path);
      path = NULL;
    }
  }

  // Close the connection (server closes in HTTP)
  netconn_close(conn);

  // Delete the buffer (netconn_recv gives us ownership,
  // so we have to make sure to deallocate the buffer)
  netbuf_delete(inbuf);

  if (png_buffer != NULL) {
    free(png_buffer);
    png_buffer = NULL;
  }
}

static void http_server(void *pvParameters) 
{
  struct netconn *conn, *newconn;  //conn is listening thread, newconn is new thread for each client
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);

  do {
    err = netconn_accept(conn, &newconn);
    if (err == ERR_OK) {
      http_server_netconn_serve(newconn);
      netconn_delete(newconn);
    }
  } while (err == ERR_OK);
  
  netconn_close(conn);
  netconn_delete(conn);
}

void start_http_server() 
{
  ESP_LOGI(TAG, "Web App is running ... ...");

  xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);
}

void init_http_server()
{
  message_0[0] = 0;
  message_1[0] = 0;
  strcpy(severity_0, "none");
  strcpy(severity_1, "none");
}