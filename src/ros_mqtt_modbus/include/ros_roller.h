#ifndef __ROS_ROLLER_H_
#define __ROS_ROLLER_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <string.h>
#include <queue>
#include <serial/serial.h>                //ROS已经内置了的串口\E5\8C?
#include "ros_modbus.h"
#include "ros_mqtt.h"


/*************************************************************************/
typedef unsigned char      u8;
typedef unsigned short int u16;
typedef short int          s16;
typedef int                s32;
/*************************************************************************/

#define HEADER 0xDEED
using namespace std;

struct Roller_Data
{
    u16 Header;
    u8  Len;
    u8  Type;
    u8  Cmd;
    u8  Num;
    u8 Moto;
    u8 Ligt_In;
    u8 Ligt_Out;
    u16 Check;
};

#pragma pack(1)
union Set_Roller_Data
{
   unsigned char data[8];
   struct
   {
        u16 Header;
        u8  Len;
        u8  Type;
        u8  Cmd;
        u8  Num;
        u16 Check;
   }prot;
};
#pragma pack(4)

class RDSModbusSlave;

void get_serial_data();
void set_serial_data(unsigned char cmd);
void roller_get_modbus_cmd(int addr);
void deal_modbus_cmd();


#endif


