/*
 * Hardware Abstraction Layer (HAL)
 */

 #include "hal.h"
 #include <EEPROM.h>
 #include "control.h"
 
 #define EEPROM_SIZE sizeof(eeprom_params_t)
 #define EEPROM_ADDRESS 0 
 
 static int controlState = OFF;
 
 DHT dht(DHTPIN, DHTTYPE);
 
 // Initialize temperature/humidity sensor
 void initTempHumiditySensor()
 {
     dht.begin(); 
 
     EEPROM.begin(EEPROM_SIZE);
 
     int storedState = EEPROM.read(EEPROM_ADDRESS);
     if (storedState == ON || storedState == OFF)
     {
         controlState = storedState;
     }
     else
     {
         controlState = OFF;
     }
 
     pinMode(CONTROL_PIN, OUTPUT);
     digitalWrite(CONTROL_PIN, LOW);
 }
 
 // Return temperature in celsius
 float getTemperature()
 {
     return dht.readTemperature();
 }
 
 // Return relative humidity
 float getHumidity()
 {
     return dht.readHumidity();    
 }
 
 // Get push button state ON/OFF
 int getPushButtonState()
 {
     static bool firstTime = true;
 
     // Set the pin as input on the first execution
     if (firstTime)
     {
         pinMode(PUSH_BUTTON_PIN, INPUT);
         firstTime = false;
     } 
 
     if (digitalRead(PUSH_BUTTON_PIN) == BUTTON_RELEASED)
         return OFF;
     else
         return ON;
 }
 
 // Set control state ON/OFF and save to EEPROM
 void setControlState(int state)
 {
     controlState = state;
 
     if (state == ON)
     {
         digitalWrite(CONTROL_PIN, HIGH);
     }
     else
     {
         digitalWrite(CONTROL_PIN, LOW);
     }
 
     EEPROM.write(EEPROM_ADDRESS, controlState);
     EEPROM.commit(); 
 }
 
 // Get heater/dehumidifier control state ON/OFF
 int getControlState()
 {
     return controlState;
 }
 
 // Set the status LED on the ESP32 board to ON or OFF states
 void setStatusLed(int state)
 {
     static bool firstTime = true;
 
     if (firstTime)
     {
         pinMode(CONTROL_PIN, OUTPUT);
         firstTime = false;
     }
 
     if (state == ON)
         return digitalWrite(CONTROL_PIN, HIGH);
     else
         return digitalWrite(CONTROL_PIN, LOW);  	
 }
 