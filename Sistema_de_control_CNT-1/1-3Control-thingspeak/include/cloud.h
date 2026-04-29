#ifndef CLOUD_H 
#define CLOUD_H

// Source of cloud writes
#define BOARD_SOURCE 0   // Coming from the board
#define REMOTE_SOURCE 1  // Coming from a remote computer

bool read_cloud(char new_control_mode[], float &new_temp_setpoint, float &new_hum_setpoint);
void write_cloud(float temp, float hum, int control_state, char control_mode[], float temp_setpoint, float hum_setpoint);

#endif