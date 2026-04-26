#ifndef LASER_COMMANDS_
#define LASER_COMMANDS_

const char CMD_STOP_STREAM_DATA[]            = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_SET_MAINTENANCE_ACCESS_MODE[] = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_REBOOT[]                      = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_READ_IDENTIFY[]               = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_READ_SERIAL_NUMBER[]          = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_READ_FIRMWARE_VERSION[]       = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_READ_DEVICE_STATE[]           = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};
const char CMD_START_STREAM_DATA[]           = {(char)0xFE, (char)0x5A, (char)0xA5, (char)0x55, (char)0x00, (char)0x02, (char)0x01, (char)0x01};


#define SIZE_OF_CMD sizeof(CMD_START_STREAM_DATA)


#endif //laser_COMMANDS_
