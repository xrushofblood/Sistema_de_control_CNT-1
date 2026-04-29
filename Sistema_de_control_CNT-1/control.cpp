#include "hal.h"
#include "control.h"

// Control related constants and variables
eeprom_params_t eeprom_params;
const float TEMP_HYSTERESIS = 0.5; // Temperature hysteresis
const float HUM_HYSTERESIS = 5; // Humidity hysteresis
int control_state = OFF; // Current control status (ON or OFF)
float temp; // Current temperature
float hum; // Current humidity

// Temperature/humidity control
void control_temp_hum(void)
{
  // Read current temperature and humidity values
  temp = getTemperature();
  hum = getHumidity();

  if (!strcmp(eeprom_params.control_mode, CONTROL_MODE_ON))
  {
    setControlState(ON);
  }
  else if (!strcmp(eeprom_params.control_mode, CONTROL_MODE_TEMP_AUTO))
  {
    if (temp > eeprom_params.temp_setpoint + (TEMP_HYSTERESIS / 2))
    {
      setControlState(OFF);
    }
    else if (temp < eeprom_params.temp_setpoint - (TEMP_HYSTERESIS / 2))
    {
      setControlState(ON);
    }
  }
  else if (!strcmp(eeprom_params.control_mode, CONTROL_MODE_HUM_AUTO))
  {
    if (hum > eeprom_params.hum_setpoint + (HUM_HYSTERESIS / 2))
    {
      setControlState(ON);
    }
    else if (hum < eeprom_params.hum_setpoint - (HUM_HYSTERESIS / 2))
    {
      setControlState(OFF);
    }
  }
  else
  {
    setControlState(OFF);
  }

  control_state = getControlState();
}
