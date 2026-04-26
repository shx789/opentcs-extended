#ifndef ROS_PT_MSG_H
#define ROS_PT_MSG_H

#include <ros/ros.h>

#define HEADER    0xDEED		//数据头

typedef char s8;
typedef unsigned char      u8;
typedef unsigned short int u16;
typedef short int    s16;
typedef unsigned  int u32;
typedef int s32;

#define HEADE_BYTE_NUM    (2)     /* 帧/包头   占2byte */
#define LEN_BYTE_NUM      (1)     /* 帧/包长度 占1byte */
#define CMD_BYTE_NUM      (1)     /* 命令码    占1byte */
#define DATA_BYTE_NUM     (21)    /* 数据域     */
#define CHECK_BYTE_NUM    (2)     /* 校验码    占2byte */

// #define RX_BUFFER_SIZE    (HEADE_BYTE_NUM + LEN_BYTE_NUM + CMD_BYTE_NUM + DATA_BYTE_NUM + CHECK_BYTE_NUM)    /* 串口收数据缓存大小 */
// #define TX_BUFFER_SIZE    (HEADE_BYTE_NUM + LEN_BYTE_NUM + CMD_BYTE_NUM + DATA_BYTE_NUM + CHECK_BYTE_NUM)    /* 串口发数据缓存大小 */

#define RX_BUFFER_SIZE   (28)
// #define TX_BUFFER_SIZE   (18)

#define SERIAL_PACK_HEAD_1    (0xDE)
#define SERIAL_PACK_HEAD_2    (0xED)

#define SERIAL_NAME "/dev/ttyUSB0"	//usb

// struct serial_msg_check_s
// {
//         u8 tx_buffer[TX_BUFFER_SIZE];
// };
// typedef struct serial_msg_check_s serial_msg_check_t;
// serial_msg_check_t serial_msg_check;

struct return_robot_data_s
{
        u8 rx_buffer[RX_BUFFER_SIZE];
};
typedef struct return_robot_data_s return_robot_data_t;
return_robot_data_t return_robot_data;

union OpenRobot20ms
{
        u8 data[10];
        struct 
        {
               u16 Header;
               u8 Len;
               u8 Type;
               u8 Cmd;
               u8 Num;
               u16 Data;
               u16 Check;
        }prot;       
};
OpenRobot20ms four_OpenRobot20ms;

union OpenGoCharge
{
        u8 data[10];
        struct 
        {
               u16 Header;
               u8 Len;
               u8 Type;
               u8 Cmd;
               u8 Num;
               u16 Data;
               u16 Check;
        }prot;       
};
OpenGoCharge four_OpenGoCharge;


union TxRobotData1
{
   u8 data[18];
   struct
   {
      u16 Header;
      u8  Len;
      u8  Type;
      u8  Cmd;
      u8  Num;
      s16 FLSpeed;
      s16 FRSpeed;
      s16 BLSpeed;
      s16 BRSpeed;
      u16 StopCon;
      u16 Check;
   }prot;
};
TxRobotData1 four_TXRobotData1;  //下发速度结构体

union TxRobotData2
{
   u8 data[18];
   struct
   {
      u16 Header;
      u8  Len;
      u8  Type;
      u8  Cmd;
      u8  Num;
      s16 Vx;
      s16 Vz;
      s16 temp1;
      s16 temp2;
      u16 StopCon;
      u16 Check;
   }prot;
};
TxRobotData2 four_TXRobotData2;  //下发速度结构体

void pt_control1(s16 flspeed,s16 frspeed,s16 blspeed,s16 brspeed,s16 stop);

#endif // ROS_PT_MSG_H
