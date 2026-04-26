
#ifndef PACKAGES_H
#define PACKAGES_H

#include <ros/ros.h>
#include <vector>

#include "nav_msgs/Odometry.h"
#include "tf/tf.h"

#include "agv_msgs/agv_drive.h"
#include "agv_msgs/agv_error.h"
#include "agv_msgs/agv_param.h"
#include "agv_msgs/agv_dt.h"
#include "agv_msgs/agv.h"

#include "agv_fsm.h"
#include "agv_serial_ports.h"
#include "agv_time_interval.h"

using SerialPortPtr = serial_ports::SerialPort *;

using AgvDriveMsg = agv_msgs::agv_drive;
using AgvErrorMsg = agv_msgs::agv_error;
using AgvParamMsg = agv_msgs::agv_param;
using AgvDtMsg    = agv_msgs::agv_dt;
using AgvMsg      = agv_msgs::agv;

namespace packages
{
  #define TIMEOUT_RECEIVE_TIME         (10)     /* s */
  #define TIMEOUR_STATE_UPLOAD_TIME    (500)    /* ms */
  #define TIMEOUR_DRIVE_UPLOAD_TIME    (500)    /* ms */
  #define TIMEOUT_GET_DRIVE_TIME       (1)      /* s */
  #define TIMEOUT_TX_LIST_TIME         (5)      /* ms */

  #define angle_to_rad(angle)    ((float)(((angle) * 3.14159265f) / 180.0f))
  #define rad_to_angle(rad)      ((float)(((rad) * 180.0f) / 3.14159265f))

  enum class TransmitCmd
  {
    state_upload = 0x01,
    set_velocity = 0x02,
    set_stop     = 0x03,
    fault_clean  = 0x05,
    set_charge   = 0x06,
    get_software = 0x07,
    get_hardware = 0x08,
    get_param    = 0x09,
    get_date     = 0x0A,
    get_drive    = 0xB0,
    drive_upload = 0xB1,
  };

  enum class ReceiveCmd
  {
    state = 0x81,
    drive_fdk_full = 0xB0,
    drive_sdfz_full = 0xC0,
    drive_fdk_base = 0xB1,
    drive_sdfz_base = 0xC1,
    software = 0x87,
    hardware = 0x88,
    param = 0x89,
    date = 0x8A,
  };

  struct Transmit
  {
    std::vector<std::vector<uint8_t>> tx_list;
  };

  struct Receive
  {
    AgvDriveMsg agv_drive;
    AgvErrorMsg agv_error;
    AgvParamMsg agv_param;
    AgvDtMsg agv_dt;
    AgvMsg agv;
  };

  struct Timeout
  {
    bool get_state = false;
    bool get_drive_full = false;
    bool get_drive_base = false;
    bool get_software = false;
    bool get_hardware = false;
    bool get_param = false;
    bool get_date = false;

    bool get_drive_full_pass = true;

    bool state_timeout = false;
    bool drive_timeout = false;

    bool pkg_exist = false;

    bool read_drive_begin = false;
    time_interval::TimeInterval read_drive_interval;

    bool state_upload_begin = false;
    time_interval::TimeInterval state_upload_interval;

    bool drive_upload_begin = false;
    time_interval::TimeInterval drive_upload_interval;

    bool tx_list_begin = false;
    time_interval::TimeInterval tx_list_interval;

    bool receive_begin = false;
    time_interval::TimeInterval receive_interval;

    bool restart_begin = false;
    time_interval::TimeInterval restart_interval;
  };

  struct Odom
  {
    bool enable = true;

    double x = 0;
    double y = 0;
    double yaw = 0;
    ros::Time last_time;
    nav_msgs::Odometry value;
  };

  struct PubPtr
  {
    ros::Publisher *info_ptr = nullptr;
    ros::Publisher *error_ptr = nullptr;
    ros::Publisher *param_ptr = nullptr;
    ros::Publisher *drive_ptr = nullptr;
    ros::Publisher *dt_ptr = nullptr;
    ros::Publisher *odom_ptr = nullptr;
  };

  struct Ctrl
  {
    bool exit = false;
    bool auto_restart = false;

    Transmit transmit;
    Receive receive;
    Timeout timeout;
    Odom odom;
    PubPtr pub;
  };

  class Packages
  {
    public:

      Packages(SerialPortPtr ptr);

      void scan(void);
      void getPackage(AgvMsg &agv_pkg,
                      AgvErrorMsg &agv_error_pkg,
                      AgvParamMsg &agv_param_pkg,
                      AgvDriveMsg &agv_drive_pkg,
                      AgvDtMsg &agv_dt_pkg,
                      nav_msgs::Odometry &agv_odom_pkg);

      void setPubPtr(ros::Publisher *info_ptr,
                     ros::Publisher *error_ptr,
                     ros::Publisher *param_ptr,
                     ros::Publisher *drive_ptr,
                     ros::Publisher *dt_ptr,
                     ros::Publisher *odom_ptr);

      void uploadState(bool enable);
      void setVelocity(double linear, double radian);
      void setVelocity(double linear, float angular);
      void setSpeed(int16_t left_speed, int16_t right_speed);
      void setStop(bool enable);
      void faultClean(uint8_t fault_type);
      void setCharge(uint8_t charge_type);
      void getSoftwareVersion(void);
      void getHardwareVersion(void);
      void getParam(void);
      void getDate(void);
      void getDrive(void);
      void uploadDrive(bool enable);

      void odomEnable(bool enable);
      void odomClean(void);

    private:

      SerialPortPtr serial_port_ptr_ = nullptr;
      fsm::Package::Fsm *fsm_pkg_ptr_ = nullptr;

      Ctrl ctrl_;

      void setTxBuffe(TransmitCmd cmd, size_t data_len, uint8_t *data_buffe, std::vector<uint8_t> &tx_buffer);
      void odom_calculation(double linear, double radian);

      bool receiveFlagCheck(void);
      void receiveParse(void);
      void receive(void);
      void transmit(void);

      void begin(void);
      void init(void);
      void idle(void);
      void readDrive(void);

      void package(void);
      void timeout(void);
  };

}

#endif
