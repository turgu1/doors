#include "doors.h"
#include "www_support.h"
#include "doors_config.h"
#include "doors_control.h"
#include "doors_www_sec.h"
#include "doors_www_net.h"
#include "doors_www_door.h"
#include "doors_www_varia.h"
#include "doors_www_testgpio.h"
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

static uint8_t  door_idx;
static uint32_t last_config_access = 0;

static char   door_inactive[DOOR_COUNT][10];
static char       door_name[DOOR_COUNT][NAME_SIZE];
static bool    door_enabled[DOOR_COUNT];
static char               m[12];
static char      push_state[80];
static ip4_addr_t config_ip;

static bool restarting;

www_field_struct no_param_fields [4] = {
  { &no_param_fields[1], STR, "MSG_0",      message_0     },
  { &no_param_fields[2], STR, "MSG_1",      message_1     },
  { &no_param_fields[3], STR, "SEVERITY_0", severity_0    },
  { NULL,                STR, "SEVERITY_1", severity_1    }
};

www_field_struct index_fields[16] = {
  { &index_fields[1],  STR, "NAME_0",     door_name[0]    },
  { &index_fields[2],  STR, "NAME_1",     door_name[1]    },
  { &index_fields[3],  STR, "NAME_2",     door_name[2]    },
  { &index_fields[4],  STR, "NAME_3",     door_name[3]    },
  { &index_fields[5],  STR, "NAME_4",     door_name[4]    },
  { &index_fields[6],  STR, "ACTIVE_0",   door_inactive[0]},
  { &index_fields[7],  STR, "ACTIVE_1",   door_inactive[1]},
  { &index_fields[8],  STR, "ACTIVE_2",   door_inactive[2]},
  { &index_fields[9],  STR, "ACTIVE_3",   door_inactive[3]},
  { &index_fields[10], STR, "ACTIVE_4",   door_inactive[4]},
  { &index_fields[11], STR, "M",          m               },
  { &index_fields[12], STR, "S",          push_state      },
  { &index_fields[13], STR, "MSG_0",      message_0       },
  { &index_fields[14], STR, "MSG_1",      message_1       },
  { &index_fields[15], STR, "SEVERITY_0", severity_0      },
  { NULL,              STR, "SEVERITY_1", severity_1      }
};

static bool timeout()
{
  TickType_t current_time = xTaskGetTickCount();

  return (last_config_access == 0) || 
         ((current_time - last_config_access) > pdMS_TO_TICKS(TIMEOUT_DURATION));
}

static void update_last_config_access()
{
  last_config_access = xTaskGetTickCount();
}

static bool is_a_valid_access(ip4_addr_t * ip)
{
  return (config_ip.addr == ip->addr) && (last_config_access != 0);
}

static void reset_config_access()
{
  config_ip.addr = 0;
  last_config_access = 0;
  strcpy(m, "null");
}

static void set_push_state()
{
  strcpy(push_state, "history.pushState({id:'homepage'},'Gestion des portes','./index');");
}

static void clear_push_state()
{
  push_state[0] = 0;
}

static void open_door()
{
  if (www_get_byte("door", &door_idx)) {
    ESP_LOGI(TAG, "Open door %d.", door_idx);
    if (door_idx < DOOR_COUNT) {
      if (door_enabled[door_idx]) {
        strcpy(message_1, "Ouverture de ");
        strcat(message_1, door_name[door_idx]);
        strcpy(severity_1, "info");
        add_relay_command(door_idx, RELAY_OPEN);
      }
      else {
        strcpy(message_1, "Porte [");
        strcat(message_1, door_name[door_idx]);
        strcat(message_1, "] pas activée!");
        strcpy(severity_1, "warning");
      }
    }
    else {
      ESP_LOGE(TAG, "Door number not valid: %d", door_idx);
      strcpy(message_1, "Porte non valide.");
      strcpy(severity_1, "error");
    }
  }
  else {
    ESP_LOGE(TAG, "Param unknown: door.");
    strcpy(message_1, "Parametre non valide.");
    strcpy(severity_1, "error");
  }
}

static void close_door()
{
  if (www_get_byte("door", &door_idx)) {
    ESP_LOGI(TAG, "Close door %d.", door_idx);
    if (door_idx < DOOR_COUNT) {
      if (door_enabled[door_idx]) {
        strcpy(message_1, "Fermeture de ");
        strcat(message_1, door_name[door_idx]);
        strcpy(severity_1, "info");
        add_relay_command(door_idx, RELAY_CLOSE);
      }
      else {
        strcpy(message_1, "Porte [");
        strcat(message_1, door_name[door_idx]);
        strcat(message_1, "] pas activée!");
        strcpy(severity_1, "warning");
      }
    }
    else {
      ESP_LOGE(TAG, "Door number not valid: %d", door_idx);
      strcpy(message_1, "Porte non valide.");
      strcpy(severity_1, "error");
    }
  }
  else {
    ESP_LOGE(TAG, "Param unknown: door.");
    strcpy(message_1, "Parametre non valide.");
    strcpy(severity_1, "error");
  }
}

static struct netconn * theconn;
static struct netbuf  * inbuf;
static char           * buf;
static uint16_t         buflen;

static void send_header(char * hdr, int size)
{
  static char buff[256];

  snprintf(buff, 256, hdr, size);
  netconn_write(theconn, buff, strlen(buff), NETCONN_NOCOPY);
}

static int bufidx;

static char getch()
{
  if (buflen == 0) return 0;
  if (bufidx >= buflen) {
    netbuf_delete(inbuf);
    err_t err = netconn_recv(theconn, &inbuf);

    if ((err == ERR_OK) && (inbuf != NULL)) {
      netbuf_data(inbuf, (void **) &buf, &buflen);

      ESP_LOGI(TAG, "Received the following packet:");
      fwrite(buf, 1, buflen, stdout);
      fputc('\n', stdout);
      fflush(stdout);
    }
    else {
      buflen = 0;
      return 0;
    }
    bufidx = 0;
  }
  return buf[bufidx++];
}

static char * find_parameters(int len)
{
  static const char start[5] = "\r\n\r\n";
  int i = 0;
  char ch;
  while (i < 4) {
    if ((ch = getch()) == start[i]) i++; else i = 0;
    if (ch == 0) return NULL;
  }

  char * data = malloc(len + 1);
  i = 0;
  while (len--) data[i++] = getch();
  data[i] = 0;
  return data;
}

static int www_post(char ** hdr, www_packet_struct ** pkts)
{
  int    size = 0;
  char * path = NULL;
  char * pos;

  // sample:
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

  pos = strnstr(buf, " HTTP/1.1", buflen);

  if (pos != NULL) {
    // Extract the path from HTTP POST request
    int path_length = pos - buf - strlen("POST ");
    path = (char *) malloc(path_length + 1);
    strncpy(path, &buf[5], path_length);
    path[path_length] = '\0';

    // Get remote IP address
    ip_addr_t remote_ip;
    u16_t     remote_port;
    netconn_getaddr(theconn, &remote_ip, &remote_port, 0);
    ESP_LOGI(TAG, "[ "IPSTR" ] POST %s", IP2STR(&(remote_ip.u_addr.ip4)), path);

    char *parameters = NULL;
    int content_length = 0;

    // Retrieve length of the parameter string
    pos = strnstr(buf, "Content-Length:", buflen);
    if (pos != NULL) {
      pos += 15;
      content_length = atoi(pos);

      bufidx = pos - buf;

      parameters = find_parameters(content_length);
    }
    else {
      ESP_LOGE(TAG, "Unable to find Content-length.");
    }

    if (parameters != NULL) {

      www_extract_params(parameters, false);
      free(parameters);

      if (strcmp("/config", path) == 0) {
        static char mp[PWD_SIZE];
        *hdr = http_html_hdr;
        if (www_get_str("MP", mp, PWD_SIZE) && 
            ((strcmp(mp, doors_config.pwd) == 0) || 
              (strcmp(mp, BACKDOOR_PWD    ) == 0))) {
          strcpy(m, "ok");
          *pkts = www_prepare_html("/spiffs/www/config.html", no_param_fields, &size, true);
          update_last_config_access();
          config_ip = remote_ip.u_addr.ip4;
        }
        else {
          strcpy(message_1,  "Code d'accès non valide!");
          strcpy(severity_1, "error");
          strcpy(m, "null");
          set_push_state();
          *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
          clear_push_state();
        }
      }
      else if (!is_a_valid_access(&(remote_ip.u_addr.ip4))) {
        strcpy(message_1,  "Accès non autorisé!");
        strcpy(severity_1, "error");
        *hdr = http_html_hdr;
        set_push_state();
        *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
        clear_push_state();
      }
      else if (timeout()) {
        ESP_LOGI(TAG, "Timeout.");
        if (last_config_access != 0) {
          strcpy(m, "\"timeout\"");
          strcpy(message_1,  "Temps mort!");
          strcpy(severity_1, "error");
        }
        *hdr = http_html_hdr;
        set_push_state();
        *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
        reset_config_access();
        clear_push_state();
      }
      else {
        update_last_config_access();
        if      (strcmp(path, "/door_update"    ) == 0) size =     door_update(hdr, pkts);
        else if (strcmp(path, "/sec_update"     ) == 0) size =      sec_update(hdr, pkts);
        else if (strcmp(path, "/varia_update"   ) == 0) size =    varia_update(hdr, pkts);
        else if (strcmp(path, "/net_update"     ) == 0) size =      net_update(hdr, pkts);
        else if (strcmp(path, "/testgpio_update") == 0) size = testgpio_update(hdr, pkts);
        else {
          ESP_LOGE(TAG, "Unknown path: %s.", path);
          *hdr = http_html_hdr_not_found;
        }
      }
    }
    else {
      ESP_LOGW(TAG, "Unable to decode parameters.");
      *hdr = http_html_hdr_not_found;
    }
  }
  else {
    ESP_LOGE(TAG, "Packet received doesn't seems to have a valid header.");
    *hdr = http_html_hdr_not_found;
  }

  if (path != NULL) {
    free(path);
    path = NULL;
  }

  return size;
}

int www_get(char ** hdr, www_packet_struct ** pkts)
{
  int    size = 0;
  char * path = NULL;
  char * line_end;
  char * png_buffer = NULL;

  ip_addr_t remote_ip;

  // sample:
  // GET /l HTTP/1.1
  // Accept: text/html, application/xhtml+xml, image/jxr,
  // Referer: http://192.168.1.222/h
  // Accept-Language: en-US,en;q=0.8,zh-Hans-CN;q=0.5,zh-Hans;q=0.3
  // User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.79 Safari/537.36 Edge/14.14393
  // Accept-Encoding: gzip, deflate
  // Host: 192.168.1.222
  // Connection: Keep-Alive

  line_end = strchr(buf, '\n');

  if (line_end != NULL) {
    //Extract the path from HTTP GET request
    path = (char *) malloc(sizeof(char) * (line_end - buf + 1));
    int path_length = line_end - buf - strlen("GET ") - strlen("HTTP/1.1") - 2;
    strncpy(path, &buf[4], path_length);
    path[path_length] = '\0';
      
    //Get remote IP address
    u16_t     remote_port;
    netconn_getaddr(theconn, &remote_ip, &remote_port, 0);
    ESP_LOGI(TAG, "[ "IPSTR" ] GET %s", IP2STR(&(remote_ip.u_addr.ip4)), path);

    www_extract_params(path, true);

    if (strcmp("/style.css", path) == 0) {
      *hdr = http_css_hdr;
      *pkts = www_prepare_html("/spiffs/www/style.css", NULL, &size, false);
    }
    else if (strcmp("/open", path) == 0) {
      open_door();
      *hdr = http_html_hdr;
      set_push_state();
      *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
      clear_push_state();
    }
    else if (strcmp("/close", path) == 0) {
      close_door();
      *hdr = http_html_hdr;
      set_push_state();
      *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
      clear_push_state();
    }
    else if ((strcmp("/index", path) == 0) || (strcmp("/", path) == 0)) {
      if (timeout()) {
        ESP_LOGI(TAG, "Timeout.");
        reset_config_access();
      }
      *hdr = http_html_hdr;
      *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
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
        send_header(http_png_hdr, size);
        png_buffer = (char *) malloc(size);
        fread(png_buffer, 1, size, f);
        fclose(f);

        int i = 0;
        while (i < size) {
          int s = size - i;
          if (s > 512) s = 512;
          netconn_write(theconn, &png_buffer[i], s, NETCONN_NOCOPY);
          i += s;
        }
      }
      else {
        ESP_LOGE(TAG, "Unable to find file %s.", fname);
        *hdr = http_html_hdr_not_found;
      }
      free(fname);
    }
    else if (strcmp("/browserconfig.xml", path) == 0) {
      *hdr = http_xml_hdr;
      *pkts = www_prepare_html("/spiffs/www/browserconfig.xml", NULL, &size, true);
    }
    else {
      if (!is_a_valid_access(&(remote_ip.u_addr.ip4))) {
        strcpy(message_1,  "Accès non autorisé!");
        strcpy(severity_1, "error");
        *hdr = http_html_hdr;
        set_push_state();
        *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
        clear_push_state();
      }
      else if (timeout()) {
        ESP_LOGI(TAG, "Timeout.");
        if (last_config_access != 0) {
          strcpy(m, "\"timeout\"");
          strcpy(message_1,  "Temps mort!");
          strcpy(severity_1, "error");
        }
        *hdr = http_html_hdr;
        set_push_state();
        *pkts = www_prepare_html("/spiffs/www/index.html", index_fields, &size, true);
        reset_config_access();
        clear_push_state();
      }
      else {
        update_last_config_access();
        if (strcmp("/config", path) == 0) {
          *hdr = http_html_hdr;
          *pkts = www_prepare_html("/spiffs/www/config.html", no_param_fields, &size, true);
        }
        else if (strcmp("/doorscfg", path) == 0) {
          *hdr = http_html_hdr;
          *pkts = www_prepare_html("/spiffs/www/doorscfg.html", no_param_fields, &size, true);
        }
        else if (strcmp("/doorcfg", path) == 0) {
          size = door_edit(hdr, pkts);
        }
        else if (strcmp("/netcfg", path) == 0) {
          size = net_edit(hdr, pkts);
        }
        else if (strcmp("/seccfg", path) == 0) {
          size = sec_edit(hdr, pkts);
        }
        else if (strcmp("/variacfg", path) == 0) {
          size = varia_edit(hdr, pkts);
        }
        else if (strcmp("/testgpio", path) == 0) {
          size = testgpio_edit(hdr, pkts);
        }
        else if (strcmp("/restart", path) == 0) {
          *hdr  = http_restart;
          size = http_restart_size;
          restarting = true;
        }
        else {
          ESP_LOGE(TAG, "Unknown path: %s.", path);
          *hdr = http_html_hdr_not_found;
        }
      }
    }
  }
  else {
    *hdr = http_html_hdr_not_found;
  }

  if (path != NULL) {
    free(path);
    path = NULL;
  }

  if (png_buffer != NULL) {
    free(png_buffer);
    png_buffer = NULL;
  }

  return size;  
}

static void http_server_netconn_serve(struct netconn *conn)
{
  // Read the data from the port, blocking if nothing yet there.
  // We assume the request (the part we care about) is in one netbuf
  
  theconn = conn;

  err_t err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void **) &buf, &buflen);

    ESP_LOGI(TAG, "Received the following packet:");
    fwrite(buf, 1, buflen, stdout);
    fputc('\n', stdout);
    fflush(stdout);

    // Is this an HTTP GET command? (only check the first 5 chars, since
    // there are other formats for GET, and we're keeping it very simple)

    www_packet_struct * pkts = NULL;
    char              * hdr  = NULL;
    int                 size = 0;

    if (doors_validate_config()) set_main_message("", "", NONE);

    if ((buflen >= 5) && (strncmp("POST ", buf, 5) == 0)) {
      size = www_post(&hdr, &pkts);
    }
    else if ((buflen >= 5) && (strncmp("GET ", buf, 4) == 0)) {
      size = www_get(&hdr, &pkts);
    }
    else {
      hdr = http_html_hdr_not_found;
    }

    // Send HTTP response
    if (hdr) {
      send_header(hdr, size);

      // Send HTML content
      if (pkts != NULL) {
        int i = 0;
        while ((i < MAX_PACKET_COUNT) && (pkts->size != 0)) {
          netconn_write(conn, pkts->buff, pkts->size, NETCONN_NOCOPY);
          i++;
          pkts++;
        }
      }

      if (restarting) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
      }
    }

    message_1[0] = 0;
    strcpy(severity_1, "none");
  }

  // Close the connection (server closes in HTTP)
  netconn_close(conn);

  // Delete the buffer (netconn_recv gives us ownership,
  // so we have to make sure to deallocate the buffer)
  netbuf_delete(inbuf);
}

static void http_server(void *pvParameters) 
{
  struct netconn *conn, *newconn;  //conn is listening thread, newconn is new thread for each client
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, doors_config.network.www_port);
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

bool start_http_server() 
{
  BaseType_t result;

  ESP_LOGI(TAG, "Web App is running ... ...");

  result = xTaskCreatePinnedToCore(
    &http_server,
    "http_server", 
    4096, 
    NULL, 
    5, 
    NULL, 
    0);               // core id
  
  if (result != pdTRUE) {
    ESP_LOGE(TAG, "Unable to start http server process (%d).", result);
    return false;
  }

  return true;
}

bool init_http_server()
{
  for (int i = 0; i < DOOR_COUNT; i++) {
    door_enabled[i] = doors_config.doors[i].enabled;
    strcpy(door_inactive[i], door_enabled[i] ? "" : "inactive");
    strcpy(    door_name[i], doors_config.doors[i].name);
  }

  reset_config_access();

  push_state[0] = 0;
  restarting    = false;

  return true;
}