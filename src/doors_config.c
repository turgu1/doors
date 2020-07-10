#include "doors.h"
#include "secure.h"
#include "doors_www.h"

#include "esp32/rom/crc.h"
#include "cJSON.h"

#define DOORS_CONFIG 1
#include "doors_config.h"

static const char * TAG = "DOORS_CONFIG";

#define BUTTON_CONNECTIONS 0x1F
#define RELAY_CONNECTIONS  0xFF

// Check if the gpio is valid and not already in used. The *all* mask is updated to reflect
// that the gpio is in use.
static bool check(uint8_t connection, uint8_t * all, uint8_t connections)
{
  uint8_t mask = 1 << connection;

  if (((mask & connections) == 0) || ((mask & *all) == 0)) return false;
  *all = *all & ~mask;
  return true;
}

static int seq_length(seq_t * seq)
{
  int i = 0;
  while (seq[i] != 0) i++;
  return i;
}

#define VERIF0(b, msg) if (!b) { ESP_LOGE(TAG, msg); goto err; }
#define VERIF1(b, msg, p1) if (!b) { ESP_LOGE(TAG, msg, p1); goto err; }
#define VERIF2(b, msg, p1, p2) if (!b) { ESP_LOGE(TAG, msg, p1, p2); goto err; }

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
  int       i, len;
  uint8_t   all_buttons = BUTTON_CONNECTIONS;
  uint8_t   all_relays  = RELAY_CONNECTIONS;
  char    * msg = NULL;
  bool      at_least_one_door = false;

  msg = "VERSION CONFIG?";
  VERIF1((doors_config.version == DOORS_CONFIG_VERSION), 
         "Config version not valid: %d", 
         doors_config.version)

  for (i = 0; i < DOOR_COUNT; i++) {
    if (doors_config.doors[i].enabled) {
      at_least_one_door = true;
      msg = "CONFLIT CONN. BOUTONS!";
      VERIF2(check(doors_config.doors[i].conn_buttons,  &all_buttons, BUTTON_CONNECTIONS), 
             "Button Connection %d already in use or not valid. Door %d, Open Button.",  
             doors_config.doors[i].conn_buttons,  i + 1)
      msg = "CONFLIT CONN. RELAIS!";
      VERIF2(check(doors_config.doors[i].conn_relays,   &all_relays, RELAY_CONNECTIONS ), 
             "Relay Connection %d already in use or not valid. Door %d, Open Relay.",
             doors_config.doors[i].conn_relays,   i + 1)
      msg = "SEQ. OUVERTURE VIDE!";
      VERIF1((doors_config.doors[i].seq_open[0]  != 0), "Door %d: No opening sequence.", i + 1)
      msg = "SEQ. OUVERTURE NON VALIDE!";
      VERIF2((len = seq_length(doors_config.doors[i].seq_open) & 1), "Door %d: Opening sequence length is even: %d", i + 1, len);
      msg = "SEQ. FERMETURE VIDE!";
      VERIF1((doors_config.doors[i].seq_close[0] != 0), "Door %d: No closing sequence.", i + 1)
      msg = "SEQ. FERMETURE NON VALIDE!";
      VERIF2((len = seq_length(doors_config.doors[i].seq_open) & 1), "Door %d: Closing sequence length is even: %d", i + 1, len);
      msg = "NOM PORTE!";
      VERIF1((doors_config.doors[i].name[0] != 0), "Door %d: Name empty.", i + 1)
    }
  }

  msg = "AUCUNE PORTE ACTIVE!";
  VERIF0((at_least_one_door), "No active door.")

  msg = "CONFIG WIFI!";
  VERIF0((doors_config.network.ssid[0] != 0), "SSID empty.")
  VERIF0((doors_config.network.pwd[0] != 0),  "WiFi password empty.")

  msg = "MOT DE PASSE!";
  VERIF0((doors_config.pwd[0] != 0), "Configuration password empty.")

  set_main_message("", "", NONE);
  return true;

err:
  set_main_message("ERREUR:", msg, ERROR);
  return false;
}

static void doors_init_config(struct config_struct * cfg)
{
  ESP_LOGW(TAG, "Config file initialized from default values.");

  memset(cfg, 0, sizeof(struct config_struct));

  cfg->version = DOORS_CONFIG_VERSION;
  
  strcpy(cfg->pwd, BACKDOOR_PWD);

  // Network

  strcpy(cfg->network.ssid,   "toto" /*"wifi ssid"*/);
  strcpy(cfg->network.pwd,    "snoopy1967" /*"wifi pwd"*/);

  strcpy(cfg->network.ip,     "");
  strcpy(cfg->network.mask,   "255.255.255.0");
  strcpy(cfg->network.gw,     "192.168.1.1");

  cfg->network.www_port = 80;

  // Doors

  for (int i = 0; i < DOOR_COUNT; i++) {
    cfg->doors[i].enabled           = i == 0; /* false; */
    strcpy(cfg->doors[i].name, "Porte ");
    cfg->doors[i].name[6]           = '1' + i;
    cfg->doors[i].name[7]           = 0;
    cfg->doors[i].conn_buttons      = i;
    cfg->doors[i].conn_relays       = i;
    cfg->doors[i].seq_open[0]       = 500;
    cfg->doors[i].seq_close[0]      = 500;
  }

  cfg->relay_abort_length = 250; // ms
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

#define GET_DOUBLE(obj, var, ident) \
  if ((item = cJSON_GetObjectItem(obj, ident)) == NULL) { ESP_LOGE(TAG, "Item [%s] not found.", ident); goto fin; } \
  if (!cJSON_IsNumber(item)) { ESP_LOGE(TAG, "ITEM [%s] not a number.", ident); goto fin; } \
  var = item->valuedouble;

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

bool config_parse_seq(seq_t * seq, char * str, int max_size)
{
  char *save = str;
  while (*str == ' ') str++;
  while (*str && (max_size > 0)) {
    *seq = 0;
    while (isdigit(*str)) { *seq = (*seq * 10) + (*str - '0'); str++; }
    seq++;
    max_size--;

    while (*str == ' ') str++;
    if (*str != ',') break;
    str++;
    while (*str == ' ') str++;
  }
  if (max_size > 0) *seq = 0;
  if (*str != 0) ESP_LOGE(TAG, "Sequence not valid: [%s]", save);
  return *str == 0;
}

static bool doors_get_config_from_file(char * filename)
{
  int i;
  char * tmp = malloc(181);

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
      ESP_LOGE(TAG, "File %s: Wrong config version: %d.", filename, doors_config.version);
      return false;
    }

    double read_crc;
    GET_DOUBLE(root, read_crc, "crc32");
    doors_config.crc32 = (uint32_t) read_crc;

    GET_STR(root,  doors_config.pwd,   "pwd", PWD_SIZE);
    
    GET_VAL (root, doors_config.relay_abort_length,  "relay_abort_length" );
    
    GET_OBJECT(root, net, "network");
    GET_STR(net, doors_config.network.ssid,     "ssid", SSID_SIZE);
    GET_STR(net, doors_config.network.pwd,      "pwd",  PWD_SIZE );
    GET_STR(net, doors_config.network.ip,       "ip",   IP_SIZE  );
    GET_STR(net, doors_config.network.mask,     "mask", IP_SIZE  );
    GET_STR(net, doors_config.network.gw,       "gw",   IP_SIZE  );
    GET_VAL(net, doors_config.network.www_port, "port");

    GET_ARRAY(root, doors, "doors", DOOR_COUNT);
    for (i = 0; i < DOOR_COUNT; i++) {
      GET_OBJECT_ITEM(doors, door, "door", i);
      GET_BOOL(door, doors_config.doors[i].enabled,     "enabled"        );
      GET_STR(door, doors_config.doors[i].name,         "name", NAME_SIZE);
      GET_VAL(door, doors_config.doors[i].conn_buttons, "conn_buttons"   );
      GET_VAL(door, doors_config.doors[i].conn_relays,  "conn_relays"    );
      GET_STR(door, tmp, "seq_open",  181);
      if (!config_parse_seq(doors_config.doors[i].seq_open, tmp, SEQ_SIZE)) goto fin;
      GET_STR(door, tmp, "seq_close", 181);
      if (!config_parse_seq(doors_config.doors[i].seq_close, tmp, SEQ_SIZE)) goto fin;
    }

    completed = true;
fin:
    cJSON_Delete(root);
    free(buff);

    uint32_t crc32 = crc32_le(0, (uint8_t const *) &doors_config, sizeof(doors_config) - 4);
    ESP_LOGI(TAG, "Retrieved config: computed CRC: %u.", crc32);
    if (!completed) {
      ESP_LOGE(TAG, "Unable to complete reading configuration parameters on file %s.", filename);
      return false;
    }
    else if (doors_config.crc32 != crc32) {
      ESP_LOGE(TAG, "Configuration CRC error on file %s (%u vs %u).", 
                    filename, doors_config.crc32, crc32);
      return false;
    }
  }

  free(tmp);
  return true;
}

void config_seq_to_str(seq_t * seq, char * str, int max_size)
{
  int cnt = SEQ_SIZE;
  bool first = true;

  while ((cnt > 0) && (max_size > 0) && (*seq != 0)) {
    if (!first) { *str++ = ','; if (--max_size <= 1) break; }
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
      if (--max_size <= 1) break;
    } while (i > 0);

    cnt--;
  }
  *str = 0;
}

static bool doors_save_config_to_file(char * filename)
{
  int i;
  char * tmp = (char *) malloc(181);

  doors_config.crc32 = crc32_le(0, (uint8_t const *) &doors_config, sizeof(doors_config) - 4);
  ESP_LOGI(TAG, "Saved config computed CRC: %u.", doors_config.crc32);

  cJSON * root = cJSON_CreateObject();
  cJSON * net, * doors, * door;
  
  cJSON_AddNumberToObject(root, "version",             doors_config.version);
  cJSON_AddStringToObject(root, "pwd",                 doors_config.pwd    );

  cJSON_AddNumberToObject(root, "relay_abort_length",  doors_config.relay_abort_length );
  
    
  cJSON_AddItemToObject  (root, "network", net = cJSON_CreateObject()   );
  cJSON_AddStringToObject(net,  "ssid",    doors_config.network.ssid    );
  cJSON_AddStringToObject(net,  "pwd",     doors_config.network.pwd     );
  cJSON_AddStringToObject(net,  "ip",      doors_config.network.ip      );
  cJSON_AddStringToObject(net,  "mask",    doors_config.network.mask    );
  cJSON_AddStringToObject(net,  "gw",      doors_config.network.gw      );
  cJSON_AddNumberToObject(net,  "port",    doors_config.network.www_port);

  cJSON_AddItemToObject(root, "doors", doors = cJSON_CreateArray());

  for (i = 0; i < DOOR_COUNT; i++) {
    cJSON_AddItemToArray(doors, door = cJSON_CreateObject());

    cJSON_AddBoolToObject  (door, "enabled", doors_config.doors[i].enabled);
    cJSON_AddStringToObject(door, "name",    doors_config.doors[i].name   );
    
    cJSON_AddNumberToObject(door, "conn_buttons",  doors_config.doors[i].conn_buttons );
    cJSON_AddNumberToObject(door, "conn_relays",   doors_config.doors[i].conn_relays  );
    
    config_seq_to_str(doors_config.doors[i].seq_open, tmp, 181);
    cJSON_AddStringToObject(door, "seq_open", tmp);

    config_seq_to_str(doors_config.doors[i].seq_close, tmp, 181);
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

  free(tmp);
  return completed;
}

bool doors_get_config()
{
  if (doors_get_config_from_file("/spiffs/config.json")) {
    return true;
  }
  else {
    if (doors_get_config_from_file("/spiffs/config_back_1.json")) {
      doors_save_config_to_file("/spiffs/config.json");
      return true;
    }
    else if (doors_get_config_from_file("/spiffs/config_back_2.json")) {
      doors_save_config_to_file("/spiffs/config.json");
      doors_save_config_to_file("/spiffs/config_back_1.json");
      return true;
    }
    else {
      doors_init_config(&doors_config);
      return doors_save_config();
    }
  }
}

bool doors_save_config()
{
  doors_validate_config();
  if (!doors_save_config_to_file("/spiffs/config.json")) return false;
  if (!doors_save_config_to_file("/spiffs/config_back_1.json")) return false;
  if (!doors_save_config_to_file("/spiffs/config_back_2.json")) return false;
  return true;
}
