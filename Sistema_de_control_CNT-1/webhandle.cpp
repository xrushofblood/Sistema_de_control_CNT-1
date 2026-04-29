#include <Arduino.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <FS.h>
#include <SPIFFS.h>
#include "hal.h"
#include "webhandle.h"
#include "control.h"

// Path of HTML files
const char STA_HTML_PATH[] = "/index_sta_patterns.html";
const char AP_HTML_PATH[] = "/index_ap.html";
const char AP_HTML_PATH_CONFIGURED[] = "/index_ap_configured.html";

// Maximum length in bytes of strings holding HTML content (including the terminator)
const int MAX_STR_LEN = 8192;

// Variables defined in main.cpp
extern WebServer server; // Web server object
extern eeprom_params_t eeprom_params; // Current EEPROM parameters

// Strings to cache web content. Initially they are empty strings
char *html_index_sta = (char *)"\0";
char *html_index_ap = (char *)"\0";
char *html_index_ap_configured = (char *)"\0";

///////////////////////////////////////////////////////////////////////////////
// Read the content of a character file up to max_len chars.
// Return a pointer to the string in case of success and NULL otherwise.
///////////////////////////////////////////////////////////////////////////////
char *readTextFile(const char * path, int max_len){
    // Open file
    File file = SPIFFS.open(path, "r");
    if (!file)
    {   
        Serial.printf("Error. Can not open file %s\n", path);
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
        Serial.printf("Error. File %s is too long. Maximum length: %d bytes\n", path, max_len - 1);
        file.close();
        return NULL;
    }
    file.close();

    // Allocate enough memory in the heap for the string, including its terminator
    char *str = (char *)malloc(nbytes + 1);
    if (str == NULL)
    {
        Serial.printf("Error. Can not perform memory allocation\n");
        file.close();
        return NULL;
    }

    // Read file
    file = SPIFFS.open(path, "r");
    file.readBytes(str, nbytes);
    str[nbytes] = '\0';

    file.close();
    return str; 
}

///////////////////////////////////////////////////////////////////////////////
// Read the following HTML files into strings:
// For station mode:
// - STA_HTML_PATH --> html_index_sta
// For access point mode:
// - AP_HTML_PATH --> html_index_ap
// - AP_HTML_PATH_CONFIGURED --> html_index_ap_configured
///////////////////////////////////////////////////////////////////////////////
void cache_web_content(int run_mode)
{
    if (!SPIFFS.begin())
    {
        Serial.println("SPIFFS mount failed");
        return;
    }
    if (run_mode == RUN_AS_AP)
    {
        html_index_ap = readTextFile(AP_HTML_PATH, MAX_STR_LEN);
        html_index_ap_configured =  readTextFile(AP_HTML_PATH_CONFIGURED, MAX_STR_LEN);
    }
    else
    {
        html_index_sta = readTextFile(STA_HTML_PATH, MAX_STR_LEN);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Handle the server response for /index.html resource when running as AP
///////////////////////////////////////////////////////////////////////////////
void handleRoot_ap() 
{   
    // Data from the GET/POST request
    String ssid_sta;
    String password_sta;
    String name;

    Serial.println(server.args());

    // If there are POST or GET arguments
    if (server.args() > 0)
    {
        // Get the values of ssid, password and device name arguments
        for (int i = 0; i < server.args(); i++) 
        {
            if (server.argName(i).equals("ssid"))
                ssid_sta = server.arg(i);
            else if (server.argName(i).equals("password"))
                password_sta = server.arg(i);
            else if (server.argName(i).equals("name"))
                name = server.arg(i);
        }
        //  If SSID, password and device name are valid and different to their defaults
        if (!ssid_sta.equals("SSID") && !password_sta.equals("Password") && !name.equals("name") &&
            (ssid_sta.length() < MAX_EEPROM_STR) && (password_sta.length() < MAX_EEPROM_STR) && 
            (name.length() < MAX_EEPROM_STR))
        {
            // Store SSID, password and device name in EEPROM
            strcpy(eeprom_params.ssid_sta, ssid_sta.c_str());
            strcpy(eeprom_params.password_sta, password_sta.c_str());
            strcpy(eeprom_params.name, name.c_str());
            EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params_t));
            EEPROM.commit();
            
            // Write debug info
            Serial.printf("Station SSID: %s\n", eeprom_params.ssid_sta);
            Serial.printf("Station password: %s\n", eeprom_params.password_sta);
            Serial.printf("Device name: %s\n", eeprom_params.name);
            Serial.printf("Saving to EEPROM...\n");

            // Answer provided by the device once it has been configured
            server.send(200, "text/html", html_index_ap_configured);
            Serial.printf("Restarting in 2 seconds...\n");
            delay(2000);
            ESP.restart(); // Restart to boot as a WiFi station
        }
    }
 
    // Answer requesting the SSID, password and device name
    server.send(200, "text/html", html_index_ap);
}

///////////////////////////////////////////////////////////////////////////////
// Replace patterns @...@ by their values in HTML contenbt for station mode
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

    // Replace patterns in HTML string to show current state
    String str(html_index_sta);
    replace_patterns_html_sta(str);

 	// Send html_handleRoot_index_sta pag with replacements
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