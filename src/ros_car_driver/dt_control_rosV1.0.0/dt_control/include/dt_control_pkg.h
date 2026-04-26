
#ifndef PACKAGES_H
#define PACKAGES_H

#include <ros/ros.h>
#include <vector>
#include <deque>
#include <serial/serial.h>

#include "nav_msgs/Odometry.h"
#include "tf/tf.h"

#include "dt_control_msgs/dt_buff.h"
#include "dt_control_msgs/dt_charge.h"
#include "dt_control_msgs/dt_current.h"
#include "dt_control_msgs/dt_date.h"
#include "dt_control_msgs/dt_drive_error.h"
#include "dt_control_msgs/dt_hardware_version.h"
#include "dt_control_msgs/dt_parameter.h"
#include "dt_control_msgs/dt_remote_ctrl.h"
#include "dt_control_msgs/dt_software_version.h"
#include "dt_control_msgs/dt_state.h"

#include "dt_control_binary.h"
#include "dt_control.h"

#define logs_debug(...)     do{if(ctrl.param.logs){printf("\033[33m[DT] [DEBUG] "); printf(__VA_ARGS__); printf("\033[0m\n");}}while(0)
#define logs_info(...)      do{if(ctrl.param.logs){printf(        "[DT] [INFO] ");  printf(__VA_ARGS__); printf("\n");       }}while(0)
#define logs_error(...)     do{if(ctrl.param.logs){printf("\033[31m[DT] [ERROR] "); printf(__VA_ARGS__); printf("\033[0m\n");}}while(0)

#define logs_debug_stream(args)    do{if(ctrl.param.logs){std::cout << "\033[33m" << "[DT]" << " " << "[DEBUG]" << " " << args << "\033[0m" << std::endl;}}while(0)
#define logs_info_stream(args)     do{if(ctrl.param.logs){std::cout <<               "[DT]" << " " << "[INFO]"  << " " << args              << std::endl;}}while(0)
#define logs_error_stream(args)    do{if(ctrl.param.logs){std::cout << "\033[31m" << "[DT]" << " " << "[ERROR]" << " " << args << "\033[0m" << std::endl;}}while(0)

#define try_catch_head    try {
#define try_catch_end(msg)    } catch(const std::exception& e) {std::cerr << e.what() << msg << '\n'; while(1);}

#define pkgInfoDisplay(max_cnt, str) \
do                           \
{                            \
  static uint8_t cnt = 0;    \
                             \
  cnt++;                     \
                             \
  if(cnt == max_cnt)         \
  {                          \
    cnt = 0;                 \
    logs_info(str);          \
  }                          \
}                            \
while(0)

using DtBuffMsg            = dt_control_msgs::dt_buff;
using DtChargeMsg          = dt_control_msgs::dt_charge;
using DtCurrentMsg         = dt_control_msgs::dt_current;
using DtDateMsg            = dt_control_msgs::dt_date;
using DtDriveErrorMsg      = dt_control_msgs::dt_drive_error;
using DtHardwareVersionMsg = dt_control_msgs::dt_hardware_version;
using DtParameterMsg       = dt_control_msgs::dt_parameter;
using DtRemoteCtrlMsg      = dt_control_msgs::dt_remote_ctrl;
using DtSoftwareVersionMsg = dt_control_msgs::dt_software_version;
using DtStateMsg           = dt_control_msgs::dt_state;

enum class TransmitCmd
{
  state_upload       = 0x01,
  current_upload     = 0x02,
  charge_upload      = 0x03,
  remote_ctrl_upload = 0x04,

  set_velocity    = 0x20,
  set_speed       = 0x21,
  set_stop        = 0x22,
  collision_clean = 0x23,
  fault_clean     = 0x24,
  set_charge      = 0x25,

  get_state       = 0x40,
  get_current     = 0x41,
  get_charge      = 0x42,
  get_remote_ctrl = 0x43,
  get_drive_error = 0x44,

  get_parameter   = 0x45,
  get_software    = 0x46,
  get_hardware    = 0x47,
  get_date        = 0x48,
};

enum class ReceiveCmd
{
  state       = 0x80,
  current     = 0x81,
  charge      = 0x82,
  remote_ctrl = 0x83,

  drive_error = 0xA0,
  parameter   = 0xA1,
  software    = 0xA2,
  hardware    = 0xA3,
  date        = 0xA4,
};

enum class DtPkgGetCode
{
  send,
  get,
  wait,
  skip,
  timeout,
};

struct SerialStep
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

struct SerialPort
{
  serial::Serial *ptr = nullptr;

  std::deque<uint8_t> rx;
  std::deque<std::vector<uint8_t>> tx;
};

struct DtMsg
{
  DtBuffMsg            buff;
  DtChargeMsg          charge;
  DtCurrentMsg         current;
  DtDateMsg            date;
  DtDriveErrorMsg      drive_error;
  DtHardwareVersionMsg hardware_version;
  DtParameterMsg       parameter;
  DtRemoteCtrlMsg      remote_ctrl;
  DtSoftwareVersionMsg software_version;
  DtStateMsg           state;
};

struct DtSub
{
  ros::Subscriber velocity;
  ros::Subscriber stop;
  ros::Subscriber collision;
  ros::Subscriber fault;
  ros::Subscriber charge;
  ros::Subscriber odom_clean;
};

struct DtPub
{
  ros::Publisher buff;
  ros::Publisher charge;
  ros::Publisher current;
  ros::Publisher date;
  ros::Publisher drive_error;
  ros::Publisher hardware_version;
  ros::Publisher parameter;
  ros::Publisher remote_ctrl;
  ros::Publisher software_version;
  ros::Publisher state;
  ros::Publisher odom;
};

struct DtParam
{
  std::string port;
  int baudrate = 0;

  bool odom = true;
  bool logs = true;
  bool original = false;

  bool info = false;
  int info_time = 2;
};

struct DtOdom
{
  double x = 0;
  double y = 0;
  double yaw = 0;
  ros::Time last_time;
  nav_msgs::Odometry value;

  void clean(void)
  {
    x = 0; y = 0; yaw = 0;
    value.pose.pose.position.x = x;
    value.pose.pose.position.y = y;
    last_time = ros::Time::now();
  }
};

struct DtPkgFlag
{
  bool once = false;

  bool request = false;
  bool response = false;

  uint32_t timeout_cnt = 0;
};

struct DtPkgGet
{
  bool do_once = false;
  uint32_t timeout_ms = 0;

  DtPkgFlag flag;

  void setOnce(bool set) {do_once = set;}
  void setTimeout(uint32_t ms) {timeout_ms = ms;}
  void flagClean(void) {memset(&flag, 0, sizeof(DtPkgFlag));}

  void setResponse(void) {if(flag.request == true) {flag.response = true;}}
};

struct DtPkg
{
  uint32_t cycle = 100;
  uint32_t step = 0;

  const size_t max_num = 8;

  /* get once */

  DtPkgGet get_software_version;
  DtPkgGet get_hardware_version;
  DtPkgGet get_parameter;
  DtPkgGet get_date;

  /* upload */

  DtPkgGet state_upload;
  DtPkgGet charge_upload;
  DtPkgGet remote_ctrl_upload;

  /* loop get */

  DtPkgGet get_drive_error;

  /* not use */

  DtPkgGet current_upload;

  DtPkgGet get_state;
  DtPkgGet get_current;
  DtPkgGet get_charge;
  DtPkgGet get_remote_ctrl;

};

struct DtCtrl
{
  SerialStep step;
  SerialPort port;
  DtMsg msg;
  DtSub sub;
  DtPub pub;
  DtParam param;
  DtOdom odom;
  DtPkg pkg;
};

extern DtCtrl ctrl;

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

void pkgOdomCalculation(double linear, double radian);
void pkgFormatFrame(TransmitCmd cmd, size_t data_len, uint8_t *data_buffe, std::vector<uint8_t> &tx_buffer);

void pkgSetStateUpload(bool enable);
void pkgSetCurrentUpload(bool enable);
void pkgSetChargeUpload(bool enable);
void pkgSetRemoteCtrlUpload(bool enable);

void pkgSetVelocity(double linear, double radian);
void pkgSetSpeed(int16_t left_speed, int16_t right_speed);
void pkgSetStop(bool enable);
void pkgCollisionClean(bool enable);
void pkgFaultClean(bool enable);
void pkgSetCharge(uint8_t charge_type);

void pkgGetState(void);
void pkgGetCurrent(void);
void pkgGetCharge(void);
void pkgGetRemoteCtrl(void);
void pkgGetDriveError(void);

void pkgGetParameter(void);
void pkgGetSoftwareVersion(void);
void pkgGetHardwareVersion(void);
void pkgGetDate(void);

void pkgLoopInit(void);
void pkgLoopReset(void);
void pkgGetLoop(void);
DtPkgGetCode pkgGetCheck(DtPkgGet &pkg, uint32_t &step);

#endif
