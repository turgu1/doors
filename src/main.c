#include <stdio.h>

#define DOORS 1
#include "doors.h"

#include "doors_config.h"
#include "doors_net.h"
#include "doors_www.h"
#include "doors_control.h"

#include "driver/gpio.h"

static const char * TAG = "MAIN";

void set_main_state(MAIN_STATE new_state)
{
  state.main_state = new_state;
}

void set_state_led_on()
{
  gpio_set_level(GPIO_LED_MAIN_STATE, 1);
}

void set_state_led_off()
{
  gpio_set_level(GPIO_LED_MAIN_STATE, 0);
}

void set_error_led_on()
{
  gpio_set_level(GPIO_LED_MAIN_STATE, 1);
}

void set_error_led_off()
{
  gpio_set_level(GPIO_LED_MAIN_STATE, 0);
}

void startup_blinking_error(uint8_t count)
{
  while (true) {
    for (int i = 0; i < count; i++) {
      set_error_led_on();
      vTaskDelay(pdMS_TO_TICKS(300));
      set_error_led_off();
      vTaskDelay(pdMS_TO_TICKS(300));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void set_main_message(char * msg1, char * msg2, SEVERITY severity)
{
  strcpy(message_0, msg1);
  strcat(message_0, msg2);

  switch (severity) {
    case NONE:
      severity_0[0] = 0;
      break;
    case INFO:
      strcpy(severity_0, "info");
      break;
    case WARNING:
      strcpy(severity_0, "warning");
      break;
    case ERROR:
      strcpy(severity_0, "error");
      break;
  }
  gpio_set_level(GPIO_LED_ERROR, severity == ERROR ? 1 : 0);
}

bool doors_initializations()
{
  ESP_LOGI(TAG, "Initializations...");

  state.main_state = RUN;

  gpio_pad_select_gpio(GPIO_LED_MAIN_STATE);
  gpio_set_direction(GPIO_LED_MAIN_STATE,  GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_LED_ERROR);
  gpio_set_direction(GPIO_LED_ERROR,  GPIO_MODE_OUTPUT);

  vTaskDelay(pdMS_TO_TICKS(10));

  gpio_set_level(GPIO_LED_MAIN_STATE, 0);
  gpio_set_level(GPIO_LED_ERROR,      0);

  message_0[0] = 0;
  message_1[0] = 0;
  strcpy(severity_0, "none");
  strcpy(severity_1, "none");

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
      ESP_LOGE(TAG, "Failed to mount filesystem.");
      startup_blinking_error(1);
    } 
    else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition.");
      startup_blinking_error(2);
    } 
    else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s).", esp_err_to_name(ret));
      startup_blinking_error(3);
    }
    return false;
  }

  size_t total = 0, used = 0;

  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s).", esp_err_to_name(ret));
    startup_blinking_error(4);
  } 
  else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d.", total, used);
  }

	ret = nvs_flash_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialise NVS Flash (%s).", esp_err_to_name(ret));
    startup_blinking_error(5);
  } 

  if (!doors_get_config()) {
    ESP_LOGE(TAG, "Failed to setup or read config file.");
    startup_blinking_error(6);
  }

  if (!init_http_server()) {
    ESP_LOGE(TAG, "Unable to initialize HTTP server.");
    startup_blinking_error(7);
  }

  if (!init_doors_control()) {
    ESP_LOGE(TAG, "Unable to initialize doors control.");
    startup_blinking_error(8);
  }

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
    if (!start_http_server()) {
      ESP_LOGE(TAG, "Http Server not started.");
      startup_blinking_error(9);
    };
  }
  else {
    ESP_LOGE(TAG, "Unable to start network. Software issue.");
    startup_blinking_error(10);

  }
 
  if (!start_doors_control()) {
    ESP_LOGE(TAG, "Unable to start door contol processes.");
    startup_blinking_error(11);
  }

  doors_validate_config(); // will trigger message_0 if any config issue
  
  set_state_led_on();
  
  // vTaskStartScheduler();

  // while (true) ;

  // printf("Restarting in 500 seconds ");
  // for (int i = 500; i >= 0; i--) {
  //   printf(".");
  //   fflush(stdout);
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }

  // printf("\nRestarting now.\n");
  // fflush(stdout);

  // esp_restart();
}