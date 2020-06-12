#include "doors.h"
#include "www_support.h"

#define DOORS_WWW 1
#include "doors_www.h"

#include "driver/gpio.h"

#include "tcpip_adapter.h"

#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

static const char * TAG = "DOORS_WWW";

int state   = 0;
int LED_PIN = 2;

// Build http header
const static char http_html_hdr[] =
  "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

static void http_server_netconn_serve(struct netconn *conn)
{
  struct netbuf * inbuf;
  char          * buf;
  uint16_t        buflen;
  err_t           err;

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

		if (buflen >= 5 && strncmp("GET ", buf, 4) == 0) {

			// sample:
			// 	GET /l HTTP/1.1
      //
      // Accept: text/html, application/xhtml+xml, image/jxr,
      // Referer: http://192.168.1.222/h
      // Accept-Language: en-US,en;q=0.8,zh-Hans-CN;q=0.5,zh-Hans;q=0.3
      // User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.79 Safari/537.36 Edge/14.14393
      // Accept-Encoding: gzip, deflate
      // Host: 192.168.1.222
      // Connection: Keep-Alive

			//Parse URL
			
      char * path     = NULL;
			char * line_end = strchr(buf, '\n');

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

      packet_struct * pkts = NULL;

			if (path != NULL) {

				if (strcmp("/config.html", path) == 0) {
          pkts = prepare_html("/spiffs/www/config.html", NULL);
				}
				else if (strcmp("/portes.html", path) == 0) {
          pkts = prepare_html("/spiffs/www/portes.html", NULL);
				}
				else if (strcmp("/net.html", path) == 0) {
          pkts = prepare_html("/spiffs/www/net.html", NULL);
				}
				else if (strcmp("/sec.html", path) == 0) {
          pkts = prepare_html("/spiffs/www/sec.html", NULL);
				}
				else if (strcmp("/style.css", path) == 0) {
          pkts = prepare_html("/spiffs/www/style.css", NULL);
				}

				free(path);
				path = NULL;
			}
      else {
        pkts = prepare_html("/spiffs/www/index.html", NULL);
      }

			// Send HTTP response header
			netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);

			// Send HTML content
      if (pkts != NULL) {
        int i = 0;
        while ((i < MAX_PACKET_COUNT) && (pkts->size != 0)) {
          netconn_write(conn, pkts->buff, pkts->size, NETCONN_NOCOPY);
          i++;
          pkts++;
        }
      }
		}
	}

	// Close the connection (server closes in HTTP)
	netconn_close(conn);

	// Delete the buffer (netconn_recv gives us ownership,
	// so we have to make sure to deallocate the buffer)
	netbuf_delete(inbuf);
}

static void http_server(void *pvParameters) 
{
	struct netconn *conn, *newconn;	//conn is listening thread, newconn is new thread for each client
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
