#include "bluetooth.h"
#include "control.h"
#include <EEPROM.h>

BluetoothSerial SerialBT;

void handleBluetoothCommands()
{
    // Stores user input across function calls
    static String command = "";  
    // Used to catch if the user is entering a password
    static bool isPasswordMode = false;  

    while (SerialBT.available()) 
    {
        char c = SerialBT.read();

        // Handle backspace: allow deleting characters but not the prompt //

        // Backspace and delete keys in ASCII
        if (c == 8 || c == 127) { 
            //If there are characters inside the command line
            if (command.length() > 0) {
                // Remove last character 
                command.remove(command.length() - 1);
                // Erase character by substituting a space and the cursor moving back
                SerialBT.print("\b \b"); 
            }
            // Skip the processing of backspace
            continue; 
        }

        // Detect if the user is entering a password
        if (command.startsWith("setpaswd ")) {
            isPasswordMode = true;
        } else {
            isPasswordMode = false;
        }

        // Mask password characters
        if (isPasswordMode && c != '\n' && c != '\r') {
            // Show '*' to hide the password
            SerialBT.print('*'); 
        } else {
            // Show normal characters 
            SerialBT.write(c); 
        }

        // Process command when ENTER is pressed
        if (c == '\n' || c == '\r') 
        {   
            // Remove spaces
            command.trim(); 

            // Ignore empty input
            if (command.length() == 0) { 
                SerialBT.print("\r\n> ");
                return;
            }

            // Process SSID command
            if (command.startsWith("setssid ")) 
            {
                String ssid = command.substring(8);
                ssid.toCharArray(eeprom_params.ssid_sta, MAX_EEPROM_STR);
                SerialBT.printf("\r\nSSID set: %s\r\n", eeprom_params.ssid_sta);
            } 
            // Process Password command (hidden input)
            else if (command.startsWith("setpaswd ")) 
            {
                String password = command.substring(9);
                password.toCharArray(eeprom_params.password_sta, MAX_EEPROM_STR);
                SerialBT.println("\r\nPassword set: ******\r\n"); 
            } 
            // Process Device Name command
            else if (command.startsWith("setname ")) 
            {
                String name = command.substring(8);
                name.toCharArray(eeprom_params.name, MAX_EEPROM_STR);
                SerialBT.printf("\r\nDevice name set: %s\r\n", eeprom_params.name);
            } 
            // Process Reset command
            else if (command == "reset") 
            {
                EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params));
                EEPROM.commit();
                SerialBT.println("\r\nConfiguration saved! Restarting in 2 seconds..\r\n.");
                delay(2000);
                ESP.restart();
            } 
            else 
            {
                SerialBT.println("\r\nInvalid command. Try again.\r\n");
            }

            // Show prompt again
            SerialBT.print("\r\n> ");
            // Reset command buffer
            command = ""; 
            // Reset password mode after processing
            isPasswordMode = false; 
        } 
        else 
        {
            // Append character to command buffer
            command += c; 
        }
    }
}
