#include <stdio.h>

#include "doors.h"
#include "doors_config.h"
#include "doors_net.h"
#include "doors_www.h"

#define GLOBAL 1
#include "doors_global.h"

static const char * TAG = "MAIN";

bool doors_initializations()
{
  ESP_LOGI(TAG, "Initializations...");

  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 30,
    .format_if_mount_failed = false
  };

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem");
    } 
    else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } 
    else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return false;
  }

  size_t total = 0, used = 0;

  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
  } 
  else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

	ESP_ERROR_CHECK(nvs_flash_init());

  init_http_server();
  
  doors_get_config();

  return true;
}

void app_main(void)
{
  printf("App Main\n");

  // Print chip information

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU cores, WiFi%s%s, ",
         CONFIG_IDF_TARGET,
         chip_info.cores,
         (chip_info.features & CHIP_FEATURE_BT ) ? "/BT"  : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("silicon revision %d, ", chip_info.revision);

  printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  printf("Free heap: %d\n", esp_get_free_heap_size());

  doors_initializations();
  
  if (start_network()) {
    ESP_LOGI(TAG, "Network started.");
  }
  else {
    ESP_LOGE(TAG, "Unable to start network. Software issue...");
  }
 
  start_http_server();

  // printf("Restarting in 500 seconds ");
  // for (int i = 500; i >= 0; i--) {
  //   printf(".");
  //   fflush(stdout);
  //   vTaskDelay(1000 / portTICK_PERIOD_MS);
  // }

  // printf("\nRestarting now.\n");
  // fflush(stdout);

  // esp_restart();
}