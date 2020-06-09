#include "doors.h"

#include "esp32/rom/crc.h"
#include "cJSON.h"

#define DOORS_CONFIG 1
#include "doors_config.h"

static const char * TAG = "DOORS_CONFIG";

static const uint64_t GPIO_BUTTONS  = 0x0000009FFEEFE034LL; // 2, 4, 5, 13 .. 19, 21 .. 23, 25 .. 36, 39
static const uint64_t GPIO_RELAYS   = 0x00000003FEEFF034LL; // 2, 4, 5, 12 .. 19, 21 .. 23, 25 .. 33

static const uint64_t GPIO_ALL = (GPIO_BUTTONS | GPIO_RELAYS);

// Check if the gpio is valid and not already in used. The *all* mask is updated to reflect
// that the gpio is in use.
static bool check(uint8_t gpio, uint64_t * all, uint64_t gpios)
{
  uint64_t mask = 1 << gpio;

  if (((mask & gpios) == 0) || ((mask & *all) == 0)) return false;
  *all = *all & ~mask;
  return true;
}

#define VERIF0(b, msg) if (!b) { sprintf(error_msg, msg); return false; }
#define VERIF1(b, msg, p1) if (!b) { sprintf(error_msg, msg, p1); return false; }
#define VERIF2(b, msg, p1, p2) if (!b) { sprintf(error_msg, msg, p1, p2); return false; }

// Validate the configuration. Check for the following:
//
// - Config structure is of the proper version
// 
// - No gpio usage conflict
// - All doors have valid open/close sequences
// - All doors have proper names
//
// - Network SSID and password are present
// - Config password access is present

bool doors_validate_config()
{
  int i;
  uint64_t all = GPIO_ALL;

  VERIF1((doors_config.version == DOORS_CONFIG_VERSION), "Numéro de version de configuration non valide: %d", doors_config.version)

  for (i = 0; i < DOOR_COUNT; i++) {
    if (doors_config.doors[i].enabled) {
      VERIF2(check(doors_config.doors[i].gpio_button_open,  &all, GPIO_BUTTONS), "GPIO %d déjà en usage ou non valide. Porte %d, Bouton Ouvrir.", doors_config.doors[i].gpio_button_open,  i + 1)
      VERIF2(check(doors_config.doors[i].gpio_button_close, &all, GPIO_BUTTONS), "GPIO %d déjà en usage ou non valide. Porte %d, Bouton Fermer.", doors_config.doors[i].gpio_button_close, i + 1)
      VERIF2(check(doors_config.doors[i].gpio_relay_open,   &all, GPIO_BUTTONS), "GPIO %d déjà en usage ou non valide. Porte %d, Relais Ouvrir.", doors_config.doors[i].gpio_relay_open,   i + 1)
      VERIF2(check(doors_config.doors[i].gpio_relay_close,  &all, GPIO_BUTTONS), "GPIO %d déjà en usage ou non valide. Porte %d, Relais Fermer.", doors_config.doors[i].gpio_relay_close,  i + 1)
      VERIF1((doors_config.doors[i].seq_open[0]  != 255), "Porte %d: Pas de séquence d'ouverture.", i + 1)
      VERIF1((doors_config.doors[i].seq_close[0] != 255), "Porte %d: Pas de séquence de fermeture.", i + 1)
      VERIF1((doors_config.doors[i].name[0] == 0), "Porte %d: Nom de la porte absent.", i + 1)
    }
  }

  VERIF0((doors_config.network.ssid[0] == 0), "SSID Absent.")
  VERIF0((doors_config.network.psw[0] == 0),  "Mot de passe WiFi absent.")

  VERIF0((doors_config.psw[0] != 0), "Le mot de passe d'accès à la configuration est absent.")

  return true;
}

static void doors_init_config()
{
  ESP_LOGW(TAG, "Config file initialized from default values.");

  memset(&doors_config, 0, sizeof(doors_config));

  doors_config.setup = true;
  strcpy(doors_config.psw, ""); // No Temporary password

  // Network

  strcpy(doors_config.network.ssid, "DOORS");
  strcpy(doors_config.network.psw,  "DOORS");

  strcpy(doors_config.network.ip,     DEFAULT_IP);
  strcpy(doors_config.network.mask,   DEFAULT_MASK);
  strcpy(doors_config.network.gw,     DEFAULT_GW);

  // Doors

  for (int i = 0; i < DOOR_COUNT; i++) {
    doors_config.doors[i].enabled           = false;
    doors_config.doors[i].gpio_button_open  = 2;
    doors_config.doors[i].gpio_button_close = 2;
    doors_config.doors[i].gpio_relay_open   = 2;
    doors_config.doors[i].gpio_relay_close  = 2;
    doors_config.doors[i].seq_open[0]       = 255;
    doors_config.doors[i].seq_close[0]      = 255;
  }
  
  doors_save_config();
}

// Retrieve configuration from SPIFFS config.json file.
// If crc32 is not OK or file does not exists, initialize the config and return FALSE.
// If crc32 is OK, return TRUE

#define GET_STR(obj, var, ident, size) \
  if ((item = cJSON_GetObjectItem(obj, ident)) == NULL) { ESP_LOGE(TAG, "Item [%s] not found.", ident); goto fin; } \
  if (!cJSON_IsString(item)) { ESP_LOGE(TAG, "ITEM [%s] not a string.", ident); goto fin; } \
  if (strlen(item->valuestring) > size) { ESP_LOGE(TAG, "String too long item [%s].", ident); goto fin; } \
  strcpy(var, item->valuestring);

#define GET_VAL(obj, var, ident) \
  if ((item = cJSON_GetObjectItem(obj, ident)) == NULL) { ESP_LOGE(TAG, "Item [%s] not found.", ident); goto fin; } \
  if (!cJSON_IsNumber(item)) { ESP_LOGE(TAG, "ITEM [%s] not a number.", ident); goto fin; } \
  var = item->valueint;

#define GET_VAL_ITEM(obj, var, ident, index) \
  if ((item = cJSON_GetArrayItem(obj, index)) == NULL) { ESP_LOGE(TAG, "Item %s[%d] not found.", ident, index); goto fin; } \
  if (!cJSON_IsNumber(item)) { ESP_LOGE(TAG, "Item %s[%d] not a number.", ident, index); goto fin; } \
  var = item->valueint;

#define GET_BOOL(obj, var, ident) \
  if ((item = cJSON_GetObjectItem(obj, ident)) == NULL) { ESP_LOGE(TAG, "Item [%s] not found.", ident); goto fin; } \
  if (!cJSON_IsBool(item)) { ESP_LOGE(TAG, "ITEM [%s] not a boolean.", ident); goto fin; } \
  var = cJSON_IsTrue(item);

#define GET_ARRAY(obj, var, ident, size) \
  if ((item = cJSON_GetObjectItem(obj, ident)) == NULL) { ESP_LOGE(TAG, "Item [%s] not found.", ident); goto fin; } \
  if (!cJSON_IsArray(item)) { ESP_LOGE(TAG, "ITEM [%s] not an array.", ident); goto fin; } \
  if (cJSON_GetArraySize(item) != size) { ESP_LOGE(TAG, "Wrong array size for %s (%d).", ident, cJSON_GetArraySize(item)); goto fin; } \
  var = item;

#define GET_OBJECT(obj, var, ident) \
  if ((item = cJSON_GetObjectItem(obj, ident)) == NULL) { ESP_LOGE(TAG, "Item [%s] not found.", ident); goto fin; } \
  if (!cJSON_IsObject(item)) { ESP_LOGE(TAG, "ITEM [%s] not a group.", ident); goto fin; } \
  var = item;

#define GET_OBJECT_ITEM(obj, var, ident, index) \
  if ((item = cJSON_GetArrayItem(obj, index)) == NULL) { ESP_LOGE(TAG, "Item %s[%d] not found.", ident, index); goto fin; } \
  if (!cJSON_IsObject(item)) { ESP_LOGE(TAG, "Item [%d] not a valid object.", index); goto fin; } \
  var = item;

bool parse_seq(uint8_t * seq, char * str, int max_size)
{
  char *save = str;
  while (*str == ' ') str++;
  while (*str && (max_size > 0)) {
    *seq = 0;
    while (isdigit(*str)) { *seq = (*seq * 10) + (*str - '0'); str++; }
    while (*str == ' ') str++;
    if (*str != ',') break;
    str++;
    while (*str == ' ') str++;
    max_size--;
    seq++;
  }
  if (max_size > 0) *seq = 255;
  if (*str != 0) ESP_LOGE(TAG, "Sequence not valid: [%s]", save);
  return *str == 0;
}

static bool doors_get_config_from_file(char * filename)
{
  int i;
  char * tmp = malloc(121);

  FILE *f = fopen(filename, "r");

  if (f == NULL) {
    ESP_LOGW(TAG, "Config file %s not found.", filename);
    return false;
  }
  else {
    ESP_LOGI(TAG, "Reading JSON config file %s.", filename);

    fseek(f, 0L, SEEK_END);
    int buff_size = ftell(f);
    rewind(f);
    char * buff = (char *) malloc(buff_size + 1);
    fread(buff, 1, buff_size, f);
    buff[buff_size] = 0;
    fclose(f);

    memset(&doors_config, 0, sizeof(doors_config));

    cJSON * root = cJSON_Parse(buff);
    cJSON * item;
    cJSON * net, * doors, * door;

    bool completed = false;

    GET_VAL(root, doors_config.version, "version");

    if (doors_config.version != DOORS_CONFIG_VERSION) {
      ESP_LOGE(TAG, "File %s:rong config version: %d.", filename, doors_config.version);
      return false;
    }

    GET_VAL(root, doors_config.crc32, "crc32");

    GET_STR(root,  doors_config.psw,   "psw", PSW_SIZE - 1);
    GET_BOOL(root, doors_config.setup, "setup");
    GET_BOOL(root, doors_config.valid, "valid");
    
    GET_OBJECT(root, net, "network");
    GET_STR(net, doors_config.network.ssid, "ssid", SSID_SIZE - 1);
    GET_STR(net, doors_config.network.psw,  "psw",  PSW_SIZE  - 1);
    GET_STR(net, doors_config.network.ip,   "ip",   IP_SIZE   - 1);
    GET_STR(net, doors_config.network.mask, "mask", IP_SIZE   - 1);
    GET_STR(net, doors_config.network.gw,   "gw",   IP_SIZE   - 1);

    GET_ARRAY(root, doors, "doors", DOOR_COUNT);
    for (i = 0; i < DOOR_COUNT; i++) {
      GET_OBJECT_ITEM(doors, door, "door", i);
      GET_BOOL(door, doors_config.doors[i].enabled,          "enabled");
      GET_STR(door, doors_config.doors[i].name,              "name", NAME_SIZE - 1);
      GET_VAL(door, doors_config.doors[i].gpio_button_open,  "gpio_button_open");
      GET_VAL(door, doors_config.doors[i].gpio_button_close, "gpio_button_close");
      GET_VAL(door, doors_config.doors[i].gpio_relay_open,   "gpio_relay_open");
      GET_VAL(door, doors_config.doors[i].gpio_relay_close,  "gpio_relay_close");
      GET_STR(door, tmp, "seq_open",  120);
      if (!parse_seq(doors_config.doors[i].seq_open, tmp, SEQ_SIZE)) goto fin;
      GET_STR(door, tmp, "seq_close", 120);
      if (!parse_seq(doors_config.doors[i].seq_close, tmp, SEQ_SIZE)) goto fin;
    }

    completed = true;
fin:
    cJSON_Delete(root);
    free(buff);
    free(tmp);

    uint32_t crc32 = crc32_le(0, (uint8_t const *) &doors_config, sizeof(doors_config) - 4);
    if (!completed || (doors_config.crc32 != crc32)) return false;
  }

  return true;
}

void seq_to_str(uint8_t * seq, char * str, int max_size)
{
  int cnt = SEQ_SIZE;
  bool first = true;

  while ((cnt > 0) && (max_size > 0) && (*seq != 255)) {
    if (!first) { *str++ = ','; if (--max_size <= 0) break; }
    first = false;
    
    int val = *seq++;

    int i = 0;
    char tmp[5];

    do {
      tmp[i++] = '0' + (val % 10);
      val = val / 10;
    } while (val > 0);
    do {
      *str++ = tmp[--i];
      if (--max_size <= 0) break;
    } while (i > 0);

    cnt--;
  }
  *str = 0;
}

static bool doors_save_config_to_file(char * filename)
{
  int i;
  char * tmp = (char *) malloc(121);

  uint32_t crc32 = crc32_le(0, (uint8_t const *) &doors_config, sizeof(doors_config) - 4);
  doors_config.crc32 = crc32;

  cJSON * root = cJSON_CreateObject();
  cJSON * net, * doors, * door;
  
  cJSON_AddNumberToObject(root, "version", doors_config.version);
  cJSON_AddStringToObject(root, "psw",     doors_config.psw);
  cJSON_AddBoolToObject  (root, "setup",   doors_config.setup);
  cJSON_AddBoolToObject  (root, "valid",   doors_config.valid);
    
  cJSON_AddItemToObject  (root, "network", net = cJSON_CreateObject());
  cJSON_AddStringToObject(net,  "ssid",    doors_config.network.ssid);
  cJSON_AddStringToObject(net,  "psw",     doors_config.network.psw);
  cJSON_AddStringToObject(net,  "ip",      doors_config.network.ip);
  cJSON_AddStringToObject(net,  "mask",    doors_config.network.mask);
  cJSON_AddStringToObject(net,  "gw",      doors_config.network.gw);

  cJSON_AddItemToObject(root, "doors", doors = cJSON_CreateArray());

  for (i = 0; i < DOOR_COUNT; i++) {
    cJSON_AddItemToArray(doors, door = cJSON_CreateObject());

    cJSON_AddBoolToObject  (door, "enabled", doors_config.doors[i].enabled);
    cJSON_AddStringToObject(door, "name",    doors_config.doors[i].name   );
    
    cJSON_AddNumberToObject(door, "gpio_button_open",  doors_config.doors[i].gpio_button_open );
    cJSON_AddNumberToObject(door, "gpio_button_close", doors_config.doors[i].gpio_button_close);
    cJSON_AddNumberToObject(door, "gpio_relay_open",   doors_config.doors[i].gpio_relay_open  );
    cJSON_AddNumberToObject(door, "gpio_relay_close",  doors_config.doors[i].gpio_relay_close );
    
    seq_to_str(doors_config.doors[i].seq_open, tmp, 120);
    cJSON_AddStringToObject(door, "seq_open", tmp);

    seq_to_str(doors_config.doors[i].seq_close, tmp, 120);
    cJSON_AddStringToObject(door, "seq_close", tmp);
  }

  cJSON_AddNumberToObject(root, "crc32", doors_config.crc32);

  FILE *f = fopen(filename, "w");
  bool completed = false;

  if (f == NULL) {
    ESP_LOGE(TAG, "Not able to create config file %s.", filename);
  }
  else {

    char * str  = cJSON_Print(root);
    int    size = strlen(str);

    completed = fwrite(str, 1, size, f) == size;

    free(tmp);
    free(str);
    cJSON_Delete(root);

    fclose(f);

    if (completed) {
      ESP_LOGI(TAG, "Config file %s saved.", filename);
    }
    else {
      ESP_LOGE(TAG, "Unable to write config into file %s.", filename);
    }
  }

  return completed;
}

bool doors_get_config()
{
  if (doors_get_config_from_file("/spiffs/config.json")) {
    return true;
  }
  else if (doors_get_config_from_file("/spiffs/config_back_1.json")) {
    doors_save_config_to_file("/spiffs/config.json");
    return true;
  }
  else if (doors_get_config_from_file("/spiffs/config_back_2.json")) {
    doors_save_config_to_file("/spiffs/config.json");
    doors_save_config_to_file("/spiffs/config_back_1.json");
    return true;
  }
  else {
    doors_init_config();
    return doors_save_config();
  }
}

bool doors_save_config()
{
  if (!doors_save_config_to_file("/spiffs/config.json")) return false;
  if (!doors_save_config_to_file("/spiffs/config_back_1.json")) return false;
  if (!doors_save_config_to_file("/spiffs/config_back_2.json")) return false;
  return true;
}
