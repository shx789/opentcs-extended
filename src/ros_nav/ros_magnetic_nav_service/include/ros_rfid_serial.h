#ifndef __ROS_RFID_SERIAL_H_
#define __ROS_RFID_SERIAL_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
 #include <fstream>
#include <mutex>
#include <string.h>
#include <queue>
#include <serial/serial.h>                //ROS已经内置了的串口\E5\8C?
 #include <unistd.h>
 #include <ros/ros.h>
  #include <std_msgs/UInt16.h>


/*************************************************************************/
typedef unsigned char      u8;
typedef unsigned short int u16;
typedef short int          s16;
typedef int                s32;
/*************************************************************************/

using namespace std;

//下发轮子速度结构体
union 
{
   u8 data[6];
   struct
   {
      u8 Header1;
      u8 Header2;
      u8 hid;
      u8 lid;
      u8 sum;
      u8 end;
   }prot;
}RxRfidData;

void get_serial_data();

#endif


