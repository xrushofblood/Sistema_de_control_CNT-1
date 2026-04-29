// Maximum length of EEPROM strings
#define MAX_EEPROM_STR 32 

// Control modes
const char CONTROL_MODE_ON[] = "on";
const char CONTROL_MODE_OFF[] = "off";
const char CONTROL_MODE_TEMP_AUTO[] = "temp_auto";
const char CONTROL_MODE_HUM_AUTO[] = "hum_auto";

typedef struct
{
  uint32_t validation_code;
  char ssid_sta[MAX_EEPROM_STR]; // SSID as station
  char password_sta[MAX_EEPROM_STR]; // Password as station
  char name[MAX_EEPROM_STR]; // Device name
  float temp_setpoint; // Temperature setpoint
  float hum_setpoint; // Humidity setpoint
  char control_mode[MAX_EEPROM_STR]; // Control mode
} eeprom_params_t;

extern eeprom_params_t eeprom_params;

// Control related constants and variables
extern const float TEMP_HYSTERESIS; // Temperature hysteresis
extern const float HUM_HYSTERESIS; // Humidity hysteresis
extern int control_state; // Current control status (ON or OFF)
extern float temp; // Current temperature
extern float hum; // Current humidity

void control_temp_hum();