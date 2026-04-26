
#ifndef PACKAGES_H
#define PACKAGES_H

#include <ros/ros.h>
#include <serial/serial.h>

#include <cstdio>
#include <memory>
#include <chrono>
#include <vector>
#include <deque>
#include <stdint.h>
#include <string.h>

#include <std_msgs/Bool.h>
#include <std_msgs/Byte.h>
#include <std_msgs/ByteMultiArray.h>
#include <std_msgs/Char.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Float64.h>
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Int16.h>
#include <std_msgs/Int16MultiArray.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Int32MultiArray.h>
#include <std_msgs/Int64.h>
#include <std_msgs/Int64MultiArray.h>
#include <std_msgs/Int8.h>
#include <std_msgs/Int8MultiArray.h>
#include <std_msgs/MultiArrayDimension.h>
#include <std_msgs/MultiArrayLayout.h>
#include <std_msgs/String.h>
#include <std_msgs/Time.h>
#include <std_msgs/UInt16.h>
#include <std_msgs/UInt16MultiArray.h>
#include <std_msgs/UInt32.h>
#include <std_msgs/UInt32MultiArray.h>
#include <std_msgs/UInt64.h>
#include <std_msgs/UInt64MultiArray.h>
#include <std_msgs/UInt8.h>
#include <std_msgs/UInt8MultiArray.h>

#include "xadz_lifting_ros/lifting_ros_binary.h"

#define logs_debug(...)     do{if(ctrl.param.logs){printf("\033[33m[LIFTING] [DEBUG] "); printf(__VA_ARGS__); printf("\033[0m\n");}}while(0)
#define logs_info(...)      do{if(ctrl.param.logs){printf(        "[LIFTING] [INFO] ");  printf(__VA_ARGS__); printf("\n");       }}while(0)
#define logs_error(...)     do{if(ctrl.param.logs){printf("\033[31m[LIFTING] [ERROR] "); printf(__VA_ARGS__); printf("\033[0m\n");}}while(0)

#define logs_debug_stream(args)    do{if(ctrl.param.logs){std::cout << "\033[33m" << "[LIFTING]" << " " << "[DEBUG]" << " " << args << "\033[0m" << std::endl;}}while(0)
#define logs_info_stream(args)     do{if(ctrl.param.logs){std::cout <<               "[LIFTING]" << " " << "[INFO]"  << " " << args              << std::endl;}}while(0)
#define logs_error_stream(args)    do{if(ctrl.param.logs){std::cout << "\033[31m" << "[LIFTING]" << " " << "[ERROR]" << " " << args << "\033[0m" << std::endl;}}while(0)

#define try_catch_head    try {
#define try_catch_end(msg)    } catch(const std::exception& e) {std::cerr << e.what() << msg << '\n'; while(1);}

#define cycle_ms_to_freq(cycle)    (1.0f / (cycle * 0.001f))

#define pkgInfoDisplay(name, max_cnt, str) \
do                                         \
{                                          \
  static uint8_t name##_cnt = 0;           \
                                           \
  name##_cnt++;                            \
                                           \
  if(name##_cnt == max_cnt)                \
  {                                        \
    name##_cnt = 0;                        \
    logs_info(str);                        \
  }                                        \
}                                          \
while(0)

enum class TransmitCmd
{
  upload = 0x01,
  state = 0x02,
  ctrl = 0x03,
};

enum class ReceiveCmd
{
  upload = 0x01,
  state = 0x02,
  ctrl = 0x03,
};

enum class PkgGetCode
{
  send,
  get,
  wait,
  skip,
  timeout,
};

struct Step
{
  bool create = false;
  bool close = false;
  bool open = false;

  void clean(void)
  {
    create = false; close = false;
    open = false;
  }

  bool finish(void)
  {
    return ((create && close && open) == true) ? true : false;
  }
};

struct Port
{
  serial::Serial *ptr = nullptr;

  std::deque<uint8_t> rx;
  std::deque<std::vector<uint8_t>> tx;
};

struct Sub
{
  ros::Subscriber velocity;
  ros::Subscriber position;
  ros::Subscriber zero;
};

struct Pub
{
  ros::Publisher mode;
  ros::Publisher limit_up;
  ros::Publisher limit_down;
  ros::Publisher drive_error;
  ros::Publisher encoder_online;
  ros::Publisher position_action;
  ros::Publisher position_target;
  ros::Publisher position_real;
  ros::Publisher velocity_target;
  ros::Publisher velocity_real;
  ros::Publisher frame;
};

struct Param
{
  std::string port;
  int baudrate = 0;

  bool logs = true;
  bool original = false;
};

struct PkgFlag
{
  bool once = false;

  bool request = false;
  bool response = false;

  uint32_t timeout_cnt = 0;
};

struct PkgGet
{
  bool do_once = false;
  uint32_t timeout_ms = 0;

  PkgFlag flag;

  void setOnce(bool set) {do_once = set;}
  void setTimeout(uint32_t ms) {timeout_ms = ms;}
  void flagClean(void) {memset(&flag, 0, sizeof(PkgFlag));}

  void setResponse(void) {if(flag.request == true) {flag.response = true;}}
};

struct Pkg
{
  uint32_t cycle = 100;
  uint32_t step = 0;

  const size_t max_num = 1;

  /* get once */

  //

  /* upload */

  PkgGet upload;

  /* loop get */

  //

  /* not use */

  PkgGet get_state;

};

struct Ctrl
{
  Step step;
  Port port;
  Sub sub;
  Pub pub;
  Param param;
  Pkg pkg;
};

extern Ctrl ctrl;

void serialInit(void);
void serialReset(void);
bool serialCreate(void);
bool serialClose(void);
bool serialOpen(void);
bool serialLink(void);
void serialRead(void);
void serialWrite(void);

void pkgReceive(void);
void pkgProcess(void);

void pkgFormatFrame(TransmitCmd cmd, size_t data_len, uint8_t *data_buffe, std::vector<uint8_t> &tx_buffer);

void pkgSetUpload(bool enable);
void pkgSetVelocity(int8_t percent);
void pkgSetPosition(uint8_t percent, int16_t position);
void pkgSetZero(void);
void pkgGetState(void);

void pkgLoopInit(void);
void pkgLoopReset(void);
void pkgGetLoop(void);
PkgGetCode pkgGetCheck(PkgGet &pkg, uint32_t &step);

#endif
