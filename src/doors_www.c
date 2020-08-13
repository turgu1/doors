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

#define VERSION 1
#include "version.h"

#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "esp_http_server.h"

#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

static const char * TAG = "DOORS_WWW";
#define CHECK(a, str, goto_tag, ...)                                           \
  if (!(a)) {                                                                  \
    ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);      \
    goto goto_tag;                                                             \
  }

// Every field list must have MSG_1, SEVERITY_1, MSG_0 and SEVERITY_0 defined.

static uint8_t  door_idx;

static char   door_inactive[DOOR_COUNT][10];
static char       door_name[DOOR_COUNT][NAME_SIZE];
static bool    door_enabled[DOOR_COUNT];
static char               m[12];
static char      push_state[80];

typedef struct {
  uint32_t peer_ip;
  TickType_t last_tick_time;
  bool authorized;
  bool timedout;
} session_struct;

session_struct * sess_ctx = NULL;

static bool restarting;

www_field_struct no_param_fields [4] = {
  { &no_param_fields[1], STR, "MSG_0",      message_0     },
  { &no_param_fields[2], STR, "MSG_1",      message_1     },
  { &no_param_fields[3], STR, "SEVERITY_0", severity_0    },
  { NULL,                STR, "SEVERITY_1", severity_1    }
};

www_field_struct index_fields[17] = {
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
  { &index_fields[16], STR, "SEVERITY_1", severity_1      },
  { NULL,              STR, "VERSION",    version         }
};

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type(httpd_req_t *req, const char *filepath, bool *is_html)
{
  const char *type = "text/plain";
  *is_html = false;
  if (CHECK_FILE_EXTENSION(filepath, ".html")) {
    type = "text/html";
    *is_html = true;
  } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
    type = "application/javascript";
  } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
    type = "text/css";
  } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
    type = "image/png";
  } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
    type = "image/x-icon";
  } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
    type = "text/xml";
  }
  ESP_LOGI(TAG, "File type: %s.", type);
  return httpd_resp_set_type(req, type);
}

// Get IP v4 peer address as an unsigned integer
static uint32_t get_ip(httpd_req_t * req)
{
  int socketfd = httpd_req_to_sockfd(req);

  if (socketfd != -1) {
    struct sockaddr_in6 peer = {0};
    socklen_t peer_length = sizeof(struct sockaddr_in6);
    if(getpeername(socketfd, (struct sockaddr *)&peer, &peer_length) != -1) {
      ESP_LOGI(TAG, "Peer IP Address: %s (%08X).", inet_ntoa(peer.sin6_addr.un.u32_addr[3]), peer.sin6_addr.un.u32_addr[3]);
      return peer.sin6_addr.un.u32_addr[3];
    }
    else {
      ESP_LOGE(TAG, "Unable to get peer ip address.");
    }
  }

  return 0;
}

static bool timedout(httpd_req_t * req)
{
  return (sess_ctx == NULL) ? false : sess_ctx->timedout;
}

static void check_timeout()
{
  if (sess_ctx) {
    if (sess_ctx->authorized) {
      TickType_t elapse = xTaskGetTickCount() - sess_ctx->last_tick_time;

      if (elapse > pdMS_TO_TICKS(TIMEOUT_DURATION)) {
        ESP_LOGI(TAG, "Priviledged access timed out.");
        sess_ctx->timedout   = true;
        sess_ctx->authorized = false;
      }
    }
  }
}

static void update_last_access(httpd_req_t * req)
{
  if (sess_ctx && (get_ip(req) == sess_ctx->peer_ip)) {
    ESP_LOGI(TAG, "Updating last access.");
    if (sess_ctx->authorized) {
      sess_ctx->last_tick_time = xTaskGetTickCount();
    }
  }
}

static bool is_a_valid_access(httpd_req_t * req)
{
  if (sess_ctx) {
    return get_ip(req) == sess_ctx->peer_ip ? sess_ctx->authorized : false;
  }
  else {
    ESP_LOGW(TAG, "is_a_valid_access: No session context.");
    return false;
  } 
}

static void init_session_context()
{
  if (sess_ctx == NULL) {
    ESP_LOGI(TAG, "Initializing session context.");
    sess_ctx = malloc(sizeof(session_struct));
    sess_ctx->authorized = false;
    sess_ctx->timedout   = false;
    sess_ctx->peer_ip    = 0;
  }
}

static bool start_config_access(httpd_req_t * req)
{
  ESP_LOGI(TAG, "Starting config access.");

  if (sess_ctx == NULL) {
    ESP_LOGW(TAG, "Session context not found. Initializing...");
    init_session_context();
  }
  
  uint32_t ip = get_ip(req);
  
  if ((sess_ctx->peer_ip == 0) || (sess_ctx->peer_ip == ip) || (sess_ctx->timedout)) {
    sess_ctx->authorized = true;
    sess_ctx->timedout   = false;
    sess_ctx->peer_ip    = ip;

    update_last_access(req);
    return true;
  }
  else {
    return false;
  }
}

static void reset_config_access()
{
  if (sess_ctx) {
    ESP_LOGI(TAG, "Reseting config access.");
    sess_ctx->authorized = false;
    sess_ctx->timedout   = false;
    sess_ctx->peer_ip    = 0;
  }
  else {
    ESP_LOGW(TAG, "reset_config_access: No session context.");
  }
}

static void set_push_state()
{
  strlcpy(push_state, "history.pushState({id:'homepage'},'Gestion des portes','./index');", 80);
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
      ESP_LOGE(TAG, "Door number not valid: %d.", door_idx);
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

// ----- POST Handler ---------------------------------------------------------

static esp_err_t post_handler(httpd_req_t *req)
{
  static char filepath[256];
  www_packet_struct * pkts = NULL;
  www_field_struct  * flds = NULL;
  bool is_html = true;
  char * query = NULL;

  ESP_LOGI(TAG, "POST Handler start, URI: %s", req->uri);

  check_timeout(req);
  doors_validate_config();

  int query_size = req->content_len;
  if (query_size > 0) {
    query = (char *) malloc(query_size + 1);
    int curr_len = 0;
    while (curr_len < query_size) {
      int received = httpd_req_recv(req, query + curr_len, query_size);
      if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive post data.");
        ESP_LOGE(TAG, "Failed to receive post data.");
        free(query);
        return ESP_FAIL;
      }
      curr_len += received;
    }
    query[query_size] = '\0';
    www_extract_params(query, false);
  }
  else {
    www_clear_params();
  }

  strlcpy(filepath, "/spiffs/www", sizeof(filepath));

  if (strncmp(req->uri, "/config", 7) == 0) {
    static char mp[PWD_SIZE];
    if (www_get_str("MP", mp, PWD_SIZE) && 
        ((strcmp(mp, doors_config.pwd) == 0) || 
         (strcmp(mp, BACKDOOR_PWD    ) == 0))) {
      if (start_config_access(req)) {
        strlcat(filepath, "/config.html", sizeof(filepath));
        flds = no_param_fields;
      }
      else {
        ESP_LOGI(TAG, "Config access already in use.");
        strcpy(message_1,  "Configuration déjà en usage!");
        strcpy(severity_1, "error");
        strlcat(filepath, "/index.html", sizeof(filepath));
        flds = index_fields;
      }
    }
    else {
      ESP_LOGI(TAG, "Access code not valid.");
      strcpy(message_1,  "Code d'accès non valide!");
      strcpy(severity_1, "error");
      strlcat(filepath, "/index.html", sizeof(filepath));
      flds = index_fields;
    }
  }
  else if (timedout(req)) {
    ESP_LOGI(TAG, "Timeout.");
    strcpy(message_1,  "Temps mort!");
    strcpy(severity_1, "error");
    strlcat(filepath, "/index.html", sizeof(filepath));
    flds = index_fields;
    reset_config_access(req);
  }
  else if (!is_a_valid_access(req)) {
    ESP_LOGI(TAG, "Unauthorized access.");
    strcpy(message_1,  "Accès non autorisé!");
    strcpy(severity_1, "error");
    strlcat(filepath, "/index.html", sizeof(filepath));
    flds = index_fields;
  }
  else {
    if      (strncmp(req->uri, "/door_update"    , 12) == 0) pkts =     door_update();
    else if (strncmp(req->uri, "/sec_update"     , 11) == 0) pkts =      sec_update();
    else if (strncmp(req->uri, "/varia_update"   , 13) == 0) pkts =    varia_update();
    else if (strncmp(req->uri, "/net_update"     , 11) == 0) pkts =      net_update();
    else if (strncmp(req->uri, "/testgpio_update", 16) == 0) pkts = testgpio_update();
    else {
      ESP_LOGE(TAG, "Unknown URI: %s.", req->uri);
      is_html = false;
    }
  }

  update_last_access(req);

  if (is_html) httpd_resp_set_type(req, "text/html");
//  set_content_type(req, filepath, &is_html);

  if (is_html && (pkts == NULL)) {
    if (flds == index_fields) {
      if (sess_ctx == NULL) {
        strcpy(m, "null");
      }
      else {
        strcpy(m, is_a_valid_access(req) ? 
                      "\"ok\"" : 
                      (timedout(req) ? "\"timeout\"" : "null"));
      }
      set_push_state();
      pkts = www_prepare_html(filepath, flds, is_html);
      clear_push_state();
    }
    else {
      pkts = www_prepare_html(filepath, flds, is_html);
    }

    message_1[0] = 0;
    strcpy(severity_1, "none");
  }

  // Send content
  if (pkts != NULL) {
    int i = 0;
    while ((i < MAX_PACKET_COUNT) && (pkts->size > 0)) {
      if (httpd_resp_send_chunk(req, pkts->buff, pkts->size) != ESP_OK) {
        ESP_LOGE(TAG, "File sending failed!");
        httpd_resp_sendstr_chunk(req, NULL);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file.");
        if (query != NULL) free(query);
        return ESP_FAIL;
      }
      i++;
      pkts++;
    }
    httpd_resp_send_chunk(req, NULL, 0);
  }
  else {
    httpd_resp_send_404(req);
  }

  ESP_LOGI(TAG, "POST Handler completed.");

  if (query != NULL) free(query);
  return ESP_OK;
}

// ----- GET Handler ----------------------------------------------------------

static esp_err_t get_handler(httpd_req_t *req)
{
  static char filepath[256];
  www_packet_struct * pkts = NULL;
  www_field_struct  * flds = NULL;
  bool is_html = false;
  char * query = NULL;

  ESP_LOGI(TAG, "GET Handler start, URI: %s", req->uri);

  check_timeout();
  doors_validate_config();
  
  size_t query_size = httpd_req_get_url_query_len(req);
  if (query_size > 0) {
    query = (char *) malloc(query_size + 1);
    httpd_req_get_url_query_str(req, query, query_size + 1);
    www_extract_params(query, false);
  }
  else {
    www_clear_params();
  }

  strlcpy(filepath, "/spiffs/www", sizeof(filepath));
  if ((req->uri[strlen(req->uri) - 1] == '/') || (strncmp(req->uri, "/index", 6) == 0)) {
    strlcat(filepath, "/index.html", sizeof(filepath));
  } else if (strncmp(req->uri, "/open", 5) == 0) {
    open_door();
    strlcat(filepath, "/index.html", sizeof(filepath));
  } else if (strncmp(req->uri, "/close", 6) == 0) {
    close_door();
    strlcat(filepath, "/index.html", sizeof(filepath));
  } else if ((strncmp(req->uri, "/favicon-",           9) == 0) ||
             (strncmp(req->uri, "/browserconfig.xml", 18) == 0) ||
             (strncmp(req->uri, "/style.css",         10) == 0)) {
    strlcat(filepath, req->uri, sizeof(filepath));
  }
  else if (timedout(req)) {
    ESP_LOGI(TAG, "Timeout.");
    strcpy(message_1,  "Temps mort!");
    strcpy(severity_1, "error");
    strlcat(filepath, "/index.html", sizeof(filepath));
    flds = index_fields;
    reset_config_access(req);
  }
  else if (!is_a_valid_access(req)) {
    ESP_LOGI(TAG, "Unautorized access.");
    strcpy(message_1,  "Accès non autorisé!");
    strcpy(severity_1, "error");
    strlcat(filepath, "/index.html", sizeof(filepath));
    flds = index_fields;
  }
  else {
    if (strncmp(req->uri, "/config", 7) == 0) {
      strlcat(filepath, "/config.html", sizeof(filepath));
      flds = no_param_fields;
    }
    else if (strncmp(req->uri, "/doorscfg", 9) == 0) {
      strlcat(filepath, "/doorscfg.html", sizeof(filepath));
      flds = no_param_fields;
    }
    else if (strncmp(req->uri, "/doorcfg", 8) == 0) {
      strlcat(filepath, "/doorcfg.html", sizeof(filepath));
      pkts = door_edit();
    }
    else if (strncmp(req->uri, "/netcfg", 7) == 0) {
      strlcat(filepath, "/netcfg.html", sizeof(filepath));
      pkts = net_edit();
    }
    else if (strncmp(req->uri, "/seccfg", 7) == 0) {
      strlcat(filepath, "/seccfg.html", sizeof(filepath));
      pkts = sec_edit();
    }
    else if (strncmp(req->uri, "/variacfg", 9) == 0) {
      strlcat(filepath, "/variacfg.html", sizeof(filepath));
      pkts = varia_edit();
    }
    else if (strncmp(req->uri, "/testgpio", 9) == 0) {
      pkts = testgpio_edit();
      strlcat(filepath, "/testgpio.html", sizeof(filepath));
    }
    else if (strncmp(req->uri, "/restart", 8) == 0) {
      strlcat(filepath, "/restart.html", sizeof(filepath));
      restarting = true;
    }
    else {
      ESP_LOGE(TAG, "Unknown URI: %s.", req->uri);
    }
  }

  if (strncmp(&filepath[11], "/index", 6) == 0) {
    flds = index_fields;
  }

  update_last_access(req);

  set_content_type(req, filepath, &is_html);

  if (is_html && (pkts == NULL)) {
    if (flds == index_fields) {
      if (sess_ctx == NULL) {
        strcpy(m, "null");
      }
      else {
        strcpy(m, is_a_valid_access(req) ? 
                      "\"ok\"" : 
                      (timedout(req) ? "\"timeout\"" : "null"));
      }
      set_push_state();
      pkts = www_prepare_html(filepath, flds, is_html);
      clear_push_state();
    }
    else {
      pkts = www_prepare_html(filepath, flds, is_html);
    }

    message_1[0] = 0;
    strcpy(severity_1, "none");
  }
  else if (pkts == NULL) {
    pkts = get_raw_file(filepath);
  }
    
  // Send content
  if (pkts != NULL) {
    int i = 0;
    while ((i < MAX_PACKET_COUNT) && (pkts->size > 0)) {
      if (httpd_resp_send_chunk(req, pkts->buff, pkts->size) != ESP_OK) {
        ESP_LOGE(TAG, "File sending failed!");
        httpd_resp_sendstr_chunk(req, NULL);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file.");
        if (query != NULL) free(query);
        return ESP_FAIL;
      }
      i++;
      pkts++;
    }
    httpd_resp_send_chunk(req, NULL, 0);
  }
  else {
    httpd_resp_send_404(req);
  }

  ESP_LOGI(TAG, "GET Handler completed.");

  if (restarting) {
    ESP_LOGI(TAG, "RESTART in 5 seconds!");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
  }

  if (query != NULL) free(query);
  return ESP_OK;
}

bool start_http_server()
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.core_id = 0;

  ESP_LOGI(TAG, "Starting HTTP Server");
  CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err);

  /* GET URI handler */
  httpd_uri_t get_uri = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = get_handler,
      .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &get_uri);

  /* POST URI handler */
  httpd_uri_t post_uri = {
      .uri = "/*",
      .method = HTTP_POST,
      .handler = post_handler,
      .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &post_uri);

  return true;

err:
  return false;
}

bool init_http_server()
{
  for (int i = 0; i < DOOR_COUNT; i++) {
    door_enabled[i] = doors_config.doors[i].enabled;
    strcpy(door_inactive[i], door_enabled[i] ? "" : "inactive");
    strcpy(    door_name[i], doors_config.doors[i].name);
  }

  push_state[0] = 0;
  restarting    = false;

  return true;
}