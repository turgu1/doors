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

static void send_header(struct netconn *conn, char * hdr, int size)
{
  static char buff[256];

  snprintf(buff, 256, hdr, size);
  netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
}

static uint8_t door_idx;

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

      char * parameters = strstr(buf, "\r\n\r\n");
      if ((parameters == NULL) || (parameters[4] == 0)) {
        netbuf_delete(inbuf);
        err = netconn_recv(conn, &inbuf);

        if ((err == ERR_OK) && (inbuf != NULL)) {
          netbuf_data(inbuf, (void **) &buf, &buflen);

          ESP_LOGI(TAG, "Received the following packet:");
          fwrite(buf, 1, buflen, stdout);
          fputc('\n', stdout);
          fflush(stdout);

          parameters = buf;
        }
        else {
          parameters = NULL;
        }
      } 
      else {
        parameters += 4;
      }

      if (parameters != NULL) {
        extract_params(parameters, false);

        if      (strcmp(path, "/door_update" ) == 0) size =  door_update(&hdr, &pkts);
        else if (strcmp(path, "/sec_update"  ) == 0) size =   sec_update(&hdr, &pkts);
        else if (strcmp(path, "/varia_update") == 0) size = varia_update(&hdr, &pkts);
        else if (strcmp(path, "/net_update"  ) == 0) size =   net_update(&hdr, &pkts);
        else if (strcmp(path, "/testgpio_update" ) == 0) size =  testgpio_update(&hdr, &pkts);
        else {
          ESP_LOGE(TAG, "Unknown path: %s.", path);
          hdr = http_html_hdr_not_found;
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

        extract_params(path, true);

        if (strcmp("/config", path) == 0) {
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/config.html", no_param_fields, &size);
        }
        else if (strcmp("/doorscfg", path) == 0) {
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/doorscfg.html", no_param_fields, &size);
        }
        else if (strcmp("/doorcfg", path) == 0) {
          size = door_edit(&hdr, &pkts);
        }
        else if (strcmp("/netcfg", path) == 0) {
          size = net_edit(&hdr, &pkts);
        }
        else if (strcmp("/seccfg", path) == 0) {
          size = sec_edit(&hdr, &pkts);
        }
        else if (strcmp("/variacfg", path) == 0) {
          size = varia_edit(&hdr, &pkts);
        }
        else if (strcmp("/testgpio", path) == 0) {
          size = testgpio_edit(&hdr, &pkts);
        }
        else if (strcmp("/style.css", path) == 0) {
          hdr = http_css_hdr;
          pkts = prepare_html("/spiffs/www/style.css", NULL, &size);
        }
        else if (strcmp("/open", path) == 0) {
          if (get_byte("door", &door_idx)) {
            ESP_LOGI(TAG, "Open door %d.", door_idx);
            if (door_idx < DOOR_COUNT) {
              strcpy(message_1, "Ouverture de ");
              strcat(message_1, doors_config.doors[door_idx].name);
              strcpy(severity_1, "info");
              // open_door(idx)
              add_relay_command(door_idx, RELAY_OPEN);
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
          hdr = http_html_hdr;
          pkts = prepare_html("/spiffs/www/index.html", index_fields, &size);
        }
        else if (strcmp("/close", path) == 0) {
          if (get_byte("door", &door_idx)) {
            ESP_LOGI(TAG, "Close door %d.", door_idx);
            if (door_idx < DOOR_COUNT) {
              strcpy(message_1, "Fermeture de ");
              strcat(message_1, doors_config.doors[door_idx].name);
              strcpy(severity_1, "info");
              // close_door(idx)
              add_relay_command(door_idx, RELAY_CLOSE);
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
        else if (strcmp("/restart", path) == 0) {
          esp_restart();
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

void start_http_server() 
{
  ESP_LOGI(TAG, "Web App is running ... ...");

  xTaskCreate(&http_server, "http_server", 4096, NULL, 5, NULL);
}

void init_http_server()
{
  message_0[0] = 0;
  message_1[0] = 0;
  strcpy(severity_0, "none");
  strcpy(severity_1, "none");
}