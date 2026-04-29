#include <Arduino.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <FS.h>
#include <SPIFFS.h>
#include "hal.h"
#include "webhandle.h"
#include "control.h"

// Path of the HTML file for station mode
const char STA_HTML_PATH[] = "/index_sta_patterns.html";

// Maximum length in bytes of strings holding HTML content (including the terminator)
const int MAX_STR_LEN = 8192;

// Variables defined in main.cpp
extern WebServer server; // Web server object
extern eeprom_params_t eeprom_params; // Current EEPROM parameters

// String to cache web content for STA mode
char *html_index_sta = (char *)"\0";

///////////////////////////////////////////////////////////////////////////////
// Read the content of a character file up to max_len chars.
// Return a pointer to the string in case of success and NULL otherwise.
///////////////////////////////////////////////////////////////////////////////
char *readTextFile(const char * path, int max_len){
    // Open file
    File file = SPIFFS.open(path, "r");
    if (!file)
    {   
        Serial.printf("Error. Cannot open file %s\n", path);
        return NULL;
    }

    // Get file size in bytes
    int nbytes = 0;
    while (file.available())
    {
        file.read();
        nbytes++;
    }
    if (nbytes >= max_len)
    {
        Serial.printf("Error. File %s is too large. Maximum allowed: %d bytes\n", path, max_len - 1);
        file.close();
        return NULL;
    }
    file.close();

    // Allocate enough memory in the heap for the string, including its terminator
    char *str = (char *)malloc(nbytes + 1);
    if (str == NULL)
    {
        Serial.printf("Error. Memory allocation failed\n");
        file.close();
        return NULL;
    }

    // Read file content
    file = SPIFFS.open(path, "r");
    file.readBytes(str, nbytes);
    str[nbytes] = '\0';

    file.close();
    return str; 
}

///////////////////////////////////////////////////////////////////////////////
// Read the HTML file for station mode and cache it
///////////////////////////////////////////////////////////////////////////////
void cache_web_content(int run_mode)
{
    if (!SPIFFS.begin())
    {
        Serial.println("SPIFFS mount failed");
        return;
    }

    if (run_mode == RUN_AS_STA)
    {
        html_index_sta = readTextFile(STA_HTML_PATH, MAX_STR_LEN);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Replace patterns @...@ by their values in the HTML content for station mode
///////////////////////////////////////////////////////////////////////////////
void replace_patterns_html_sta(String &str)
{
    char temp_str[16];
    char hum_str[16];
    char temp_setpoint_str[16];
    char hum_setpoint_str[16];
    
    sprintf(temp_str, "%2.1f", temp);
    sprintf(hum_str, "%2.0f", hum);
    sprintf(temp_setpoint_str, "%2.1f", eeprom_params.temp_setpoint);
    sprintf(hum_setpoint_str, "%2.0f", eeprom_params.hum_setpoint);
    
    str.replace("@temp@", temp_str);
    str.replace("@hum@", hum_str);
    
    if (control_state == ON)
    {
        str.replace("@cnt@", "ON");
        str.replace("@cnt_colour@", "red");
    }
    else
    {
        str.replace("@cnt@", "OFF");
        str.replace("@cnt_colour@", "blue");
    }

    str.replace("@dev_name@", eeprom_params.name);
    str.replace("@temp_setpoint@", temp_setpoint_str);
    str.replace("@hum_setpoint@", hum_setpoint_str);

    if (!strcmp(eeprom_params.control_mode, "on"))
        str.replace("@selected_on@", "selected");
    else if  (!strcmp(eeprom_params.control_mode, "off"))
        str.replace("@selected_off@", "selected");
    else if  (!strcmp(eeprom_params.control_mode, "temp_auto"))
        str.replace("@selected_temp_auto@", "selected");
    else if  (!strcmp(eeprom_params.control_mode, "hum_auto"))
        str.replace("@selected_hum_auto@", "selected");
}

///////////////////////////////////////////////////////////////////////////////
// Handle the server response for /index.html resource when running as station
///////////////////////////////////////////////////////////////////////////////
void handleRoot_sta() 
{
    // If there are POST or GET arguments
    if (server.args() > 0)
    {
        bool changes = false;

        // Get the value of control arguments
        for (int i = 0; i < server.args(); i++) 
        {
            if (server.argName(i).equals("temp_setpoint"))
            {
                float temp_setpoint = server.arg(i).toFloat();
                if (temp_setpoint != eeprom_params.temp_setpoint)
                {
                    eeprom_params.temp_setpoint = temp_setpoint;
                    changes = true;
                }
            }
            else if (server.argName(i).equals("hum_setpoint"))
            {
                float hum_setpoint = server.arg(i).toFloat();
                if (hum_setpoint != eeprom_params.hum_setpoint)
                {
                    eeprom_params.hum_setpoint = hum_setpoint;
                    changes = true;
                }
            }
            else if (server.argName(i).equals("control_mode"))
            {
                if (strcmp(server.arg(i).c_str(), eeprom_params.control_mode))
                {
                    strcpy(eeprom_params.control_mode, server.arg(i).c_str());
                    changes = true;
                }
            }
        }

        // Save in EEPROM when setpoints or control mode have changed
        if (changes)
        {
            EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params));
            EEPROM.commit();
            Serial.println("Saving new control parameters in EEPROM");
        }
    }

    // Replace patterns in HTML string to show the current state
    String str(html_index_sta);
    replace_patterns_html_sta(str);

    // Send the HTML page with replacements
    server.send(200, "text/html", str.c_str());
}

///////////////////////////////////////////////////////////////////////////////
// Handle the server response for a not found resource
///////////////////////////////////////////////////////////////////////////////
void handleNotFound() 
{
    String message = "File not found\n\n";
    server.send(404, "text/plain", message);
}
