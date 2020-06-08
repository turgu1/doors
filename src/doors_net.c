#include "doors.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "doors_config.h"

#define  DOORS_NET 1
#include "doors_net.h"

#define TAG "DOORS_NET"

// ----- AP MODE -------------------------------------------------------------------

#define MAX_STA_CONN       3
#define AP_SSID  "DOORS"
#define AP_PSW   "DOORS"

static void ap_event_handler(void            * arg, 
                             esp_event_base_t  event_base,
                             int32_t           event_id, 
                             void            * event_data)
{
  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
             MAC2STR(event->mac), event->aid);
  } 
  else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
             MAC2STR(event->mac), event->aid);
  }
}

static bool wifi_init_ap()
{
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL));

  wifi_config_t wifi_config = {
    .ap = {
      .ssid = AP_SSID,
      .ssid_len = strlen(AP_SSID),
      .password = AP_PSW,
      .max_connection = MAX_STA_CONN,
      .authmode = WIFI_AUTH_WPA_WPA2_PSK
    },
  };

  if (strlen(AP_PSW) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
          AP_SSID, AP_PSW);

  return true;
}

// ----- STA MODE ------------------------------------------------------------------

#define ESP_MAXIMUM_RETRY  5

// FreeRTOS event group to signal when we are connected

static EventGroupHandle_t wifi_event_group;

// The event group allows multiple bits for each event, but we 
// only care about two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void sta_event_handler(void            * arg, 
                              esp_event_base_t  event_base,
                              int32_t           event_id, 
                              void            * event_data)
{
  if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
    esp_wifi_connect();
  } 
  else if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
    if (s_retry_num < ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } 
    else {
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG,"connect to the AP fail");
  } 
  else if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP)) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static bool wifi_init_sta(void)
{
  bool connected = false;

  wifi_event_group = xEventGroupCreate();

  tcpip_adapter_init();

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    &sta_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &sta_event_handler, NULL));

  wifi_config_t wifi_config = {
    .sta = {
      .pmf_cfg = {
        .capable = true,
        .required = false
      },
    },
  };

  strcpy((char *) wifi_config.sta.ssid,     doors_config.network.ssid);
  strcpy((char *) wifi_config.sta.password, doors_config.network.psw);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) 
  // or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). 
  // The bits are set by event_handler() (see above)

  EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

  // xEventGroupWaitBits() returns the bits before the call returned, 
  // hence we can test which event actually happened.

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
             doors_config.network.ssid, doors_config.network.psw);
    connected = true;
  } 
  else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
             doors_config.network.ssid, doors_config.network.psw);
  }
  else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   IP_EVENT_STA_GOT_IP, &sta_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,    &sta_event_handler));

  vEventGroupDelete(wifi_event_group);

  return connected;
}

bool start_network()
{
  if (!wifi_init_sta()) return wifi_init_ap();
  return true;
}

bool check_network()
{
  return true;
}