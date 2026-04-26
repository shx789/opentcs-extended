#ifndef __ROA_AGV_MSG_H_
#define __ROA_AGV_MSG_H_
#include <ros/ros.h>    

/*************************************************************************/
#define HEADER    0xDEED		//数据头

#define SWAP16(X)  (((X & 0XFF00) >> 8) | ((X & 0XFF) << 8))
#define SWAP32(X)  ((X & 0XFF) << 24 | (X & 0XFF00) << 8 | (X & 0XFF0000) >> 8 | (X >> 24) & 0XFF)
/*************************************************************************/
typedef unsigned char      u8;
typedef unsigned short int u16;
typedef short int          s16;
typedef int                s32;
/*************************************************************************/
using namespace std;
#pragma pack(1)
/*************************************************************************/
//打开20ms数据上传结构体
union OPen20MsData
{
   u8 data[10];
   struct
   {
      u16 Header;
      u8  Len;
      u8  Type;
      u8  Cmd;
      u8  Num;
      u16 Data;
      u16 Check;
   }prot;
}Open20MsData,OpenGoCharge;
/*************************************************************************/
//打开20ms数据上传结构体
union LedStopSet
{
   u8 data[14];
   struct
   {
      u16 Header;
      u8  Len;
      u8  Type;
      u8  Cmd;
      u8  Num;
      u16 Led12;
      u16 Led34;
      u16 Stop;
      u16 Check;
   }prot;
}LedStopSet;
/*************************************************************************/

union RESMode1
{
   u8 data[40];
   struct
   {
      s32   Vx;         //
      float Vz;         //
      float AccX;       //
      float AccY;       //
      float AccZ;       //
      float GyrX;       //
      float GyrY;       //
      float GyrZ;       //
      u16   Voltage;    //
      u16     State;    //
      u16   Light12;    //
      u16   Light34;    //
   }prot;
}RXMode1;

union RESMode2
{
   u8 data[40];
   struct
   {
      s16   LSpeed;     //
      s16   RSpeed;     //
      s16   LAddEN;     //
      s16   RAddEN;     //
      float AccX;       //
      float AccY;       //
      float AccZ;       //
      float GyrX;       //
      float GyrY;       //
      float GyrZ;       //
      u16   Voltage;    //
      u16     State;    //
      u16   Light12;    //
      u16   Light34;    //
   }prot;
}RXMode2;


union RxRobotData20MS
{
   u8 data[48];
   struct
   {
      u16 Header;       
      u8  Len;       
      u8  Type;         
      u8  Cmd;       
      u8  Num;       
      u8  data[40];     
      u16 Check;        
   }prot;
}RXRobotData20MS;

/*************************************************************************/
//下发轮子速度结构体
union TXRobotData1
{
   u8 data[16];
   struct
   {
      u16 Header;
      u8  Len;
      u8  Type;
      u8  Cmd;
      u8  Num; 
      s16 Mode;
      s16 Vx;
      float Vz;   
      u16 Check;
   }prot;
}TXRobotData1;

//下发轮子速度结构体
union TXRobotData2
{
   u8 data[16];
   struct
   {
      u16 Header;
      u8  Len;
      u8  Type;
      u8  Cmd;
      u8  Num; 
      s16 Mode;
      s16 LSpeed;
      s16 RSpeed;
      u16 NC;  
      u16 Check;
   }prot;
}TXRobotData2;
/*************************************************************************/
bool software_stop_flag = false;
#pragma pack(4)

#endif
