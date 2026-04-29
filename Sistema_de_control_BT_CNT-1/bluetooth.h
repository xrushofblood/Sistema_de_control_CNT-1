#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "BluetoothSerial.h"

extern BluetoothSerial SerialBT; 
extern const char BT_DEVICE_NAME[];    

void handleBluetoothCommands();

#endif
