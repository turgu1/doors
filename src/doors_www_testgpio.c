#include "doors.h"
#include "doors_config.h"
#include "doors_www.h"
#include "www_support.h"
#include "secure.h"

#include <stdlib.h>
#include "driver/gpio.h"

#define DOORS_WWW_TESTGPIO 1
#include "doors_www_testgpio.h"

static const char * TAG = "DOORS_WWW_TESTGPIO";

static uint8_t gpio = 0;
static bool    active_low;
static int     duration;
static char    onclick[40];
static char    return_url[12];
static char    return_label[14];

static www_field_struct testgpio_fields[10] = {
  { &testgpio_fields[1], BYTE, "GPIO",       &gpio         },
  { &testgpio_fields[2], BOOL, "ACTIVELOW",  &active_low   },
  { &testgpio_fields[3], INT,  "DURATION",   &duration     },
  { &testgpio_fields[4], STR,  "ONCLICK",     onclick      },
  { &testgpio_fields[5], STR,  "RETURNURL",   return_url   },
  { &testgpio_fields[6], STR,  "RETURNLABEL", return_label },
  { &testgpio_fields[7], STR,  "MSG_0",       message_0    },
  { &testgpio_fields[8], STR,  "MSG_1",       message_1    },
  { &testgpio_fields[9], STR,  "SEVERITY_0",  severity_0   },
  { NULL,                STR,  "SEVERITY_1",  severity_1   }
};

int testgpio_update(char ** hdr, www_packet_struct ** pkts)
{
  char * field = NULL;

  if (!www_get_byte("GPIO",      &gpio)                         ) field = "GPIO";
  if (!www_get_bool("ACTIVELOW", &active_low) && (field == NULL)) field = "Active Low";
  if (!www_get_int( "DURATION",  &duration  ) && (field == NULL)) field = "Durée";

  if (field == NULL) {
    ESP_LOGI(TAG, "Fields OK. Executing task.");
    
  	//GPIO initialization
	  gpio_pad_select_gpio(gpio);
	  gpio_set_direction(gpio, GPIO_MODE_OUTPUT);

    if (state.main_state == RUN) set_main_state(STOP);

    gpio_set_level(gpio, active_low ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    gpio_set_level(gpio, active_low ? 0 : 1);
    vTaskDelay(pdMS_TO_TICKS(duration));

    gpio_set_level(gpio, active_low ? 1 : 0);

    strcpy(message_1,  "Complété.");
    strcpy(severity_1, "info");
  }
  else {
    ESP_LOGW(TAG, "Some field in error: %s.", field);

    strcpy(message_1,  "Champ en erreur: ");
    strcat(message_1,  field);
    strcpy(severity_1, "error");
  }

  strcpy(return_url,   "./restart");
  strcpy(return_label, "Redémarrage");
  strcpy(onclick,      "onclick=\"return confirmation()\"");

  int size;

  *hdr = http_html_hdr;
  *pkts = www_prepare_html("/spiffs/www/testgpio.html", testgpio_fields, &size, true);         
  
  return size;
}

int testgpio_edit(char ** hdr, www_packet_struct ** pkts)
{
  int size;

  strcpy(return_url,   "./config");
  strcpy(return_label, "Retour");
  onclick[0] = 0;

  if (gpio == 0) {
    gpio = 2;
    active_low = false;
    duration = 1000;
  }

  *hdr  = http_html_hdr;
  *pkts = www_prepare_html("/spiffs/www/testgpio.html", testgpio_fields, &size, true);

  return size;
}
