
#include <deque>
#include <ros/ros.h>

#include "nav_msgs/Odometry.h"
#include "tf/tf.h"

#include "agv_msgs/agv_drive.h"
#include "agv_msgs/agv_error.h"
#include "agv_msgs/agv_param.h"
#include "agv_msgs/agv_dt.h"
#include "agv_msgs/agv.h"

#include "agv_binary.h"
#include "agv_packages.h"
#include "agv_serial_ports.h"
#include "agv_fsm.h"
#include "agv_time_interval.h"
#include "agv.h"

using Packages = packages::Packages;

using TimeInterval = time_interval::TimeInterval;
using TimeIntervalType = time_interval::TimeType;

using SerialPortPtr = serial_ports::SerialPort *;

using FsmPackage = fsm::Package::Fsm;
using FsmPackageEvent = fsm::Package::Event;
using FsmPackageState = fsm::Package::State;

using AgvDriveMsg = agv_msgs::agv_drive;
using AgvErrorMsg = agv_msgs::agv_error;
using AgvParamMsg = agv_msgs::agv_param;
using AgvDtMsg    = agv_msgs::agv_dt;
using AgvMsg      = agv_msgs::agv;

boost::array<double, 36> cov_array = {1e-3, 0, 0, 0, 0, 0,
                                      0, 1e-3, 0, 0, 0, 0,
                                      0, 0, 1e6, 0, 0, 0,
                                      0, 0, 0, 1e6, 0, 0,
                                      0, 0, 0, 0, 1e6, 0,
                                      0, 0, 0, 0, 0, 1e3};

Packages::Packages(SerialPortPtr ptr)
{
  serial_port_ptr_ = ptr;

  fsm_pkg_ptr_ = new FsmPackage();
  fsm_pkg_ptr_->start();

  ctrl_.auto_restart = true;

  ctrl_.odom.last_time = ros::Time::now();
}

void Packages::scan(void)
{
  /* logs_info_stream(fsm_pkg_ptr_->getNowState()); */

  #if 0
  static bool start = false;
  static TimeInterval interval;

  if(start == false)
  {
    start = true;
    interval.start();
  }
  else
  {
    if(interval.timeout(TimeIntervalType::seconds, 1) == true)
    {
      interval.start();

      //logs_debug_stream(fsm_pkg_ptr_->getNowState());

      //std::deque<uint8_t> &rx_buffer = serial_port_ptr_->getReadBuffer();
      //logs_debug("rx buffe size: %d\n", (int)rx_buffer.size());

      //logs_debug("tx list size: %d\n", (int)ctrl_.transmit.tx_list.size());
    }
  }
  #endif

  receive();

  transmit();


  begin();

  init();

  readDrive();


  idle();

  package();

  timeout();
}

void Packages::getPackage(AgvMsg &agv_pkg,
                          AgvErrorMsg &agv_error_pkg,
                          AgvParamMsg &agv_param_pkg,
                          AgvDriveMsg &agv_drive_pkg,
                          AgvDtMsg &agv_pt_pkg,
                          nav_msgs::Odometry &fw_odom_pkg)
{
  agv_pkg = ctrl_.receive.agv;
  agv_error_pkg = ctrl_.receive.agv_error;
  agv_param_pkg = ctrl_.receive.agv_param;
  agv_drive_pkg = ctrl_.receive.agv_drive;
  agv_pt_pkg = ctrl_.receive.agv_dt;
  fw_odom_pkg = ctrl_.odom.value;
}

void Packages::setPubPtr(ros::Publisher *info_ptr,
                         ros::Publisher *error_ptr,
                         ros::Publisher *param_ptr,
                         ros::Publisher *drive_ptr,
                         ros::Publisher *dt_ptr,
                         ros::Publisher *odom_ptr)
{
  ctrl_.pub.info_ptr = info_ptr;
  ctrl_.pub.error_ptr = error_ptr;
  ctrl_.pub.param_ptr = param_ptr;
  ctrl_.pub.drive_ptr = drive_ptr;
  ctrl_.pub.dt_ptr = dt_ptr;
  ctrl_.pub.odom_ptr = odom_ptr;
}

void Packages::uploadState(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *enable_ptr = nullptr;

  enable_ptr = (uint16_t *)&data_buffer[0];

  *enable_ptr = enable;

  setTxBuffe(TransmitCmd::state_upload, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  if(ctrl_.timeout.get_state == false) {ctrl_.timeout.get_state = true;}
}

void Packages::setVelocity(double linear, double radian)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint16_t *mode_ptr = nullptr;
  int16_t *linear_ptr = nullptr;
  float *radian_ptr = nullptr;

  mode_ptr = (uint16_t *)&data_buffer[0];
  linear_ptr = (int16_t *)&data_buffer[2];
  radian_ptr = (float *)&data_buffer[4];

  *mode_ptr = 0x0000;
  *linear_ptr = (int16_t)(linear * 1000);
  *radian_ptr = radian;

  setTxBuffe(TransmitCmd::set_velocity, 8, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::setVelocity(double linear, float angular)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint16_t *mode_ptr = nullptr;
  int16_t *linear_ptr = nullptr;
  float *radian_ptr = nullptr;

  mode_ptr = (uint16_t *)&data_buffer[0];
  linear_ptr = (int16_t *)&data_buffer[2];
  radian_ptr = (float *)&data_buffer[4];

  *mode_ptr = 0x0000;
  *linear_ptr = (int16_t)(linear * 1000);
  *radian_ptr = angle_to_rad(angular);

  setTxBuffe(TransmitCmd::set_velocity, 8, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::setSpeed(int16_t left_speed, int16_t right_speed)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint16_t *mode_ptr = nullptr;
  int16_t *left_speed_ptr = nullptr;
  int16_t *right_speed_ptr = nullptr;

  mode_ptr = (uint16_t *)&data_buffer[0];
  left_speed_ptr = (int16_t *)&data_buffer[2];
  right_speed_ptr = (int16_t *)&data_buffer[4];

  *mode_ptr = 0x0001;
  *left_speed_ptr = left_speed;
  *right_speed_ptr = right_speed;

  setTxBuffe(TransmitCmd::set_velocity, 8, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::setStop(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *enable_ptr = nullptr;

  enable_ptr = (uint16_t *)&data_buffer[0];

  *enable_ptr = enable;

  setTxBuffe(TransmitCmd::set_stop, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::faultClean(uint8_t fault_type)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *type_ptr = nullptr;

  // if((fault_type != 0x01) && (fault_type != 0x02) && (fault_type != 0x03)) {return;}

  type_ptr = (uint16_t *)&data_buffer[0];

  switch(fault_type)
  {
    case 1: {*type_ptr = B_0000_0001; break;}    /* 驱动器 */
    case 2: {*type_ptr = B_0000_0010; break;}    /* 前碰撞 */
    case 3: {*type_ptr = B_0000_0100; break;}    /* 后碰撞 */
    default: {break;}
  }

  *type_ptr = B_0000_0010 |  B_0000_0100;

  setTxBuffe(TransmitCmd::fault_clean, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::setCharge(uint8_t charge_type)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *enable_ptr = nullptr;

  if((charge_type != 0x00) && (charge_type != 0x01) && (charge_type != 0x02)) {return;}

  enable_ptr = (uint16_t *)&data_buffer[0];

  *enable_ptr = charge_type;

  setTxBuffe(TransmitCmd::set_charge, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::getSoftwareVersion(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  setTxBuffe(TransmitCmd::get_software, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  if(ctrl_.timeout.get_software == false) {ctrl_.timeout.get_software = true;}
}

void Packages::getHardwareVersion(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  setTxBuffe(TransmitCmd::get_hardware, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  if(ctrl_.timeout.get_hardware == false) {ctrl_.timeout.get_hardware = true;}
}

void Packages::getParam(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  setTxBuffe(TransmitCmd::get_param, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  if(ctrl_.timeout.get_param == false) {ctrl_.timeout.get_param = true;}
}

void Packages::getDate(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  setTxBuffe(TransmitCmd::get_date, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  if(ctrl_.timeout.get_date == false) {ctrl_.timeout.get_date = true;}
}

void Packages::getDrive(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *enable_ptr = nullptr;

  enable_ptr = (uint16_t *)&data_buffer[0];

  *enable_ptr = 0x0001;

  setTxBuffe(TransmitCmd::get_drive, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  /* if(ctrl_.timeout.get_drive_full == false) {ctrl_.timeout.get_drive_full = true;} */

  if((ctrl_.timeout.get_drive_full == false) && (ctrl_.timeout.get_drive_full_pass == true))
  {
    ctrl_.timeout.get_drive_full = true;
    ctrl_.timeout.get_drive_full_pass = false;
  }
}

void Packages::uploadDrive(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *enable_ptr = nullptr;

  enable_ptr = (uint16_t *)&data_buffer[0];

  *enable_ptr = enable;

  setTxBuffe(TransmitCmd::drive_upload, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);

  if(ctrl_.timeout.get_state == false) {ctrl_.timeout.get_state = true;}
}

void Packages::odomEnable(bool enable)
{
  ctrl_.odom.enable = enable;

  /* if(enable == false) {odomClean();} */
}

void Packages::odomClean(void)
{
  ctrl_.odom.x = 0;
  ctrl_.odom.y = 0;
  ctrl_.odom.yaw = 0;

  ctrl_.odom.value.pose.pose.position.x = ctrl_.odom.x;
  ctrl_.odom.value.pose.pose.position.y = ctrl_.odom.y;

  ctrl_.odom.last_time = ros::Time::now();
}

void Packages::setTxBuffe(TransmitCmd cmd, size_t data_len, uint8_t *data_buffe, std::vector<uint8_t> &tx_buffer)
{
  size_t loop = 0;
  uint16_t sum_value = 0;

  tx_buffer.clear();

  tx_buffer.push_back(0xED);
  tx_buffer.push_back(0xDE);
  tx_buffer.push_back(0x00);
  tx_buffer.push_back(0x00);
  tx_buffer.push_back((uint8_t)cmd);
  tx_buffer.push_back(10 + data_len);

  for(loop = 0; loop < data_len; loop++)
  {
    tx_buffer.push_back(data_buffe[loop]);
  }

  for(loop = 0; loop < tx_buffer.size(); loop++)
  {
    sum_value = sum_value + tx_buffer.at(loop);
  }

  tx_buffer.push_back((sum_value >> 0) & 0xFF);
  tx_buffer.push_back((sum_value >> 8) & 0xFF);

  tx_buffer.push_back(0xBD);
  tx_buffer.push_back(0xBC);

  #if 0
  printf("tx buffe: ");
  for(loop = 0; loop < tx_buffer.size(); loop++)
  {
    printf("%02X ", tx_buffer.at(loop));
  }
  printf("\n");
  #endif
}

void Packages::odom_calculation(double linear, double radian)
{
  if(ctrl_.odom.enable == true)
  {
    ctrl_.odom.value.header.stamp = ros::Time::now();
    ctrl_.odom.value.header.frame_id = "odom";
    ctrl_.odom.value.child_frame_id = "base_link";

    double vx = linear;
    double vy = 0;
    double vyaw = radian;
    double dt = (ros::Time::now() - ctrl_.odom.last_time).toSec();
    ctrl_.odom.last_time = ros::Time::now();

    double dx = (vx * cos(ctrl_.odom.yaw) - vy * sin(ctrl_.odom.yaw)) * dt;
    double dy = (vx * sin(ctrl_.odom.yaw) + vy * cos(ctrl_.odom.yaw)) * dt;
    double dyaw = vyaw * dt;

    ctrl_.odom.x += dx;
    ctrl_.odom.y += dy;
    ctrl_.odom.yaw += dyaw;

    geometry_msgs::Quaternion q = tf::createQuaternionMsgFromYaw(ctrl_.odom.yaw);

    ctrl_.odom.value.pose.pose.position.x = ctrl_.odom.x;
    ctrl_.odom.value.pose.pose.position.y = ctrl_.odom.y;
    ctrl_.odom.value.pose.pose.position.z = 0;
    ctrl_.odom.value.pose.pose.orientation = q;
    ctrl_.odom.value.twist.twist.linear.x = vx;
    ctrl_.odom.value.twist.twist.angular.z = vyaw;
    ctrl_.odom.value.pose.covariance = cov_array;
    ctrl_.odom.value.twist.covariance = cov_array;

    ctrl_.pub.odom_ptr->publish(ctrl_.odom.value);
  }
}

bool Packages::receiveFlagCheck(void)
{
  /* return true = package not exist */
  /* return false = package exist */

  if(ctrl_.timeout.get_state == true)
  {
    if(ctrl_.timeout.state_upload_begin == false)
    {
      ctrl_.timeout.state_upload_begin = true;
      ctrl_.timeout.state_upload_interval.clear();
      ctrl_.timeout.state_upload_interval.start();
    }

    if(ctrl_.timeout.state_upload_interval.timeout(TimeIntervalType::milliseconds, TIMEOUR_STATE_UPLOAD_TIME) == true)
    {
      ctrl_.timeout.state_upload_begin = false;
      ctrl_.timeout.state_upload_interval.clear();

      ctrl_.timeout.state_timeout = true;
    }
    else
    {
      ctrl_.timeout.state_timeout = false;
    }
  }
  else
  {
    ctrl_.timeout.state_upload_begin = false;
    ctrl_.timeout.state_upload_interval.clear();

    ctrl_.timeout.state_timeout = false;
  }

  if(ctrl_.timeout.get_drive_base == true)
  {
    if(ctrl_.timeout.drive_upload_begin == false)
    {
      ctrl_.timeout.drive_upload_begin = true;
      ctrl_.timeout.drive_upload_interval.clear();
      ctrl_.timeout.drive_upload_interval.start();
    }

    if(ctrl_.timeout.drive_upload_interval.timeout(TimeIntervalType::milliseconds, TIMEOUR_DRIVE_UPLOAD_TIME) == true)
    {
      ctrl_.timeout.drive_upload_begin = false;
      ctrl_.timeout.drive_upload_interval.clear();

      ctrl_.timeout.drive_timeout = true;
    }
    else
    {
      ctrl_.timeout.drive_timeout = false;
    }
  }
  else
  {
    ctrl_.timeout.drive_upload_begin = false;
    ctrl_.timeout.drive_upload_interval.clear();

    ctrl_.timeout.drive_timeout = false;
  }

  if((ctrl_.timeout.get_drive_full == true) || (ctrl_.timeout.get_software == true) || \
     (ctrl_.timeout.get_hardware == true) || (ctrl_.timeout.get_param == true) || \
     (ctrl_.timeout.get_date == true) || (ctrl_.timeout.state_timeout == true) || (ctrl_.timeout.drive_timeout == true))
  {
    if(ctrl_.timeout.get_drive_full == false) {ctrl_.timeout.get_drive_full_pass = true;}

    return true;
  }
  else
  {
    if(ctrl_.timeout.get_state == false) {ctrl_.timeout.get_state = true;}
    if(ctrl_.timeout.get_drive_base == false) {ctrl_.timeout.get_drive_base = true;}

    if(ctrl_.timeout.get_drive_full == false) {ctrl_.timeout.get_drive_full_pass = true;}

    return false;
  }
}

void Packages::receiveParse(void)
{
  std::deque<uint8_t> &rx_buffer = serial_port_ptr_->getReadBuffer();

  switch((ReceiveCmd)rx_buffer.at(4))
  {
    case ReceiveCmd::state:
    {
      ctrl_.receive.agv_dt.cmd_81_state.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_81_state.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      uint16_t voltage    = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t state      = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));
      uint16_t charge     = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      int16_t linear      = (uint16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));
      int16_t angular     = (uint16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));
      int16_t left_speed  = (uint16_t)((rx_buffer[17] << 8) | (rx_buffer[16] << 0));
      int16_t right_speed = (uint16_t)((rx_buffer[19] << 8) | (rx_buffer[18] << 0));

      AgvMsg *agv_ptr = &ctrl_.receive.agv;
      AgvErrorMsg *agv_error_ptr = &ctrl_.receive.agv_error;

      agv_ptr->drive_error = false;

      agv_ptr->voltage            = ((float)voltage) * 0.1f;
      agv_ptr->stop_button        = (((state >> 0) & 0x01) == 0x00) ? false : true;
      agv_ptr->remote_ctrl_stop   = (((state >> 1) & 0x01) == 0x00) ? false : true;
      agv_ptr->software_stop      = (((state >> 2) & 0x01) == 0x00) ? false : true;
      agv_ptr->remote_ctrl_error  = (((state >> 3) & 0x01) == 0x00) ? false : true;
      agv_ptr->drive_error       |= (((state >> 4) & 0x01) == 0x00) ? false : true;
      /* agv_ptr->drive_error       |= (((state >> 5) & 0x01) == 0x00) ? false : true; */
      agv_ptr->drive_error       |= (((state >> 6) & 0x01) == 0x00) ? false : true;
      agv_ptr->drive_error       |= (((state >> 7) & 0x01) == 0x00) ? false : true;
      /* agv_ptr->drive_error       |= (((state >> 8) & 0x01) == 0x00) ? false : true; */
      /* agv_ptr->drive_error       |= (((state >> 9) & 0x01) == 0x00) ? false : true; */
      agv_ptr->front_collision    = (((state >> 10) & 0x01) == 0x00) ? false : true;
      agv_ptr->back_collision     = (((state >> 11) & 0x01) == 0x00) ? false : true;
      agv_ptr->charge_enable      = (((state >> 12) & 0x01) == 0x00) ? false : true;
      agv_ptr->charge_io          = (((state >> 15) & 0x01) == 0x00) ? false : true;
      agv_ptr->charge_state       = (uint8_t)charge;
      agv_ptr->linear_velocity    = ((float)linear) * 0.001f;
      agv_ptr->angular_velocity   = ((float)angular) * 0.001f;
      agv_ptr->left_speed         = left_speed;
      agv_ptr->right_speed        = right_speed;

      agv_error_ptr->remote_ctrl_lost = (((state >> 3) & 0x01) == 0x00) ? false : true;
      agv_error_ptr->drive_lost       = (((state >> 4) & 0x01) == 0x00) ? false : true;
      /* agv_error_ptr->drive_lost = (((state >> 5) & 0x01) == 0x00) ? false : true; */
      agv_error_ptr->left_drive_error  = (((state >> 6) & 0x01) == 0x00) ? false : true;
      agv_error_ptr->right_drive_error = (((state >> 7) & 0x01) == 0x00) ? false : true;
      /* agv_error_ptr->back_left_drive_error = (((state >> 8) & 0x01) == 0x00) ? false : true; */
      /* agv_error_ptr->back_left_drive_error = (((state >> 9) & 0x01) == 0x00) ? false : true; */

      ctrl_.pub.info_ptr->publish(ctrl_.receive.agv);
      ctrl_.pub.error_ptr->publish(ctrl_.receive.agv_error);

      odom_calculation(agv_ptr->linear_velocity, agv_ptr->angular_velocity);

      ctrl_.timeout.get_state = false;

      static uint8_t cnt = 0;

      cnt++;

      if(cnt == 100)
      {
        cnt = 0;
        logs_info("get state package");
      }

      break;
    }
    case ReceiveCmd::drive_fdk_full:
    {
      ctrl_.receive.agv_dt.cmd_B0_drive_full.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_B0_drive_full.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      uint16_t left_error = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t right_error = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));

      uint16_t left_state = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      uint16_t right_state = (uint16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));

      int16_t left_current = (int16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));
      int16_t right_current = (int16_t)((rx_buffer[17] << 8) | (rx_buffer[16] << 0));

      uint16_t left_max_current = (uint16_t)((rx_buffer[19] << 8) | (rx_buffer[18] << 0));
      uint16_t right_max_current = (uint16_t)((rx_buffer[21] << 8) | (rx_buffer[20] << 0));

      uint16_t temperature = (uint16_t)((rx_buffer[23] << 8) | (rx_buffer[22] << 0));

      uint16_t voltage = (uint16_t)((rx_buffer[25] << 8) | (rx_buffer[24] << 0));

      AgvDriveMsg *agv_drive_ptr = &ctrl_.receive.agv_drive;

      /* left */

      agv_drive_ptr->left.current = ((float)left_current) * 0.1f;
      agv_drive_ptr->left.max_current = ((float)left_max_current) * 0.1f;

      agv_drive_ptr->left.state.state_code = left_state;
      agv_drive_ptr->left.error.error_code = left_error;

      agv_drive_ptr->left.error.undervoltage = (left_error >> 0) & 0x01;
      agv_drive_ptr->left.error.position_fault = (left_error >> 1) & 0x01;
      agv_drive_ptr->left.error.hall_fault = (left_error >> 2) & 0x01;
      agv_drive_ptr->left.error.overcurrent = (left_error >> 3) & 0x01;
      agv_drive_ptr->left.error.overload = (left_error >> 4) & 0x01;
      agv_drive_ptr->left.error.overheating = (left_error >> 7) & 0x01;
      agv_drive_ptr->left.error.speed_deviation = (left_error >> 10) & 0x01;
      agv_drive_ptr->left.error.free_wheeling_fault = (left_error >> 13) & 0x01;
      agv_drive_ptr->left.error.overheating |= (left_error >> 14) & 0x01;

      agv_drive_ptr->left.state.start = (left_state >> 0) & 0x01;
      agv_drive_ptr->left.state.running = (left_state >> 1) & 0x01;
      agv_drive_ptr->left.state.speed_reach = (left_state >> 3) & 0x01;
      agv_drive_ptr->left.state.position_reach = (left_state >> 4) & 0x01;
      agv_drive_ptr->left.state.braking_output = (left_state >> 7) & 0x01;
      agv_drive_ptr->left.state.exceeded_overload = (left_state >> 9) & 0x01;
      agv_drive_ptr->left.state.error_warning = (left_state >> 10) & 0x01;
      agv_drive_ptr->left.state.reverse_stall = (left_state >> 12) & 0x01;
      agv_drive_ptr->left.state.forward_stall = (left_state >> 13) & 0x01;

      /* right */

      agv_drive_ptr->right.current = ((float)right_current) * 0.1f;
      agv_drive_ptr->right.max_current = ((float)right_max_current) * 0.1f;

      agv_drive_ptr->right.state.state_code = right_state;
      agv_drive_ptr->right.error.error_code = right_error;

      agv_drive_ptr->right.error.undervoltage = (right_error >> 0) & 0x01;
      agv_drive_ptr->right.error.position_fault = (right_error >> 1) & 0x01;
      agv_drive_ptr->right.error.hall_fault = (right_error >> 2) & 0x01;
      agv_drive_ptr->right.error.overcurrent = (right_error >> 3) & 0x01;
      agv_drive_ptr->right.error.overload = (right_error >> 4) & 0x01;
      agv_drive_ptr->right.error.overheating = (right_error >> 7) & 0x01;
      agv_drive_ptr->right.error.speed_deviation = (right_error >> 10) & 0x01;
      agv_drive_ptr->right.error.free_wheeling_fault = (right_error >> 13) & 0x01;
      agv_drive_ptr->right.error.overheating |= (right_error >> 14) & 0x01;

      agv_drive_ptr->right.state.start = (right_state >> 0) & 0x01;
      agv_drive_ptr->right.state.running = (right_state >> 1) & 0x01;
      agv_drive_ptr->right.state.speed_reach = (right_state >> 3) & 0x01;
      agv_drive_ptr->right.state.position_reach = (right_state >> 4) & 0x01;
      agv_drive_ptr->right.state.braking_output = (right_state >> 7) & 0x01;
      agv_drive_ptr->right.state.exceeded_overload = (right_state >> 9) & 0x01;
      agv_drive_ptr->right.state.error_warning = (right_state >> 10) & 0x01;
      agv_drive_ptr->right.state.reverse_stall = (right_state >> 12) & 0x01;
      agv_drive_ptr->right.state.forward_stall = (right_state >> 13) & 0x01;


      agv_drive_ptr->temperature = ((float)temperature) * 0.1f;
      agv_drive_ptr->voltage = ((float)voltage) * 0.1f;

      agv_drive_ptr->drive_type = "FDK";

      ctrl_.pub.drive_ptr->publish(ctrl_.receive.agv_drive);

      ctrl_.timeout.get_drive_full = false;

      logs_info("get drive full package");

      break;
    }
    case ReceiveCmd::drive_fdk_base:
    {
      ctrl_.receive.agv_dt.cmd_B1_drive_base.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_B1_drive_base.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      int16_t left_current = (int16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      int16_t right_current = (int16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));

      uint16_t drive_voltage = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));

      ctrl_.receive.agv.left_current = ((float)left_current) * 0.1f;
      ctrl_.receive.agv.right_current = ((float)right_current) * 0.1f;

      ctrl_.receive.agv.drive_voltage = ((float)drive_voltage) * 0.1f;

      /* ctrl_.pub.info_ptr->publish(ctrl_.receive.agv); */

      ctrl_.timeout.get_drive_base = false;

      static uint8_t cnt = 0;

      cnt++;

      if(cnt == 100)
      {
        cnt = 0;
        logs_info("get drive base package");
      }

      break;
    }
    case ReceiveCmd::drive_sdfz_full:
    {
      ctrl_.receive.agv_dt.cmd_C0_drive_full.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_C0_drive_full.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      uint16_t left_error = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t right_error = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));

      /*
      uint16_t left_state = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      uint16_t right_state = (uint16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));
      */

      int16_t left_current = (int16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));
      int16_t right_current = (int16_t)((rx_buffer[17] << 8) | (rx_buffer[16] << 0));

      uint16_t left_max_current = (uint16_t)((rx_buffer[19] << 8) | (rx_buffer[18] << 0));
      uint16_t right_max_current = (uint16_t)((rx_buffer[21] << 8) | (rx_buffer[20] << 0));

      uint16_t temperature = (uint16_t)((rx_buffer[23] << 8) | (rx_buffer[22] << 0));

      uint16_t voltage = (uint16_t)((rx_buffer[25] << 8) | (rx_buffer[24] << 0));

      AgvDriveMsg *agv_drive_ptr = &ctrl_.receive.agv_drive;

      /* left */

      agv_drive_ptr->left.current = ((float)left_current) * 0.1f;
      agv_drive_ptr->left.max_current = ((float)left_max_current) * 0.1f;

      agv_drive_ptr->left.error.error_code = left_error;

      agv_drive_ptr->left.error.encoder_fault = (left_error >> 1) & 0x01;
      agv_drive_ptr->left.error.hall_fault = (left_error >> 2) & 0x01;
      agv_drive_ptr->left.error.encoder_fault |= (left_error >> 3) & 0x01;
      agv_drive_ptr->left.error.overheating = (left_error >> 4) & 0x01;
      agv_drive_ptr->left.error.overvoltage = (left_error >> 5) & 0x01;
      agv_drive_ptr->left.error.undervoltage = (left_error >> 6) & 0x01;
      agv_drive_ptr->left.error.short_circuit = (left_error >> 7) & 0x01;
      agv_drive_ptr->left.error.overload = (left_error >> 11) & 0x01;
      agv_drive_ptr->left.error.speed_deviation = (left_error >> 12) & 0x01;

      /*
      agv_drive_ptr->left.state.start = (left_state >> 0) & 0x01;
      agv_drive_ptr->left.state.running = (left_state >> 1) & 0x01;
      agv_drive_ptr->left.state.speed_reach = (left_state >> 3) & 0x01;
      agv_drive_ptr->left.state.position_reach = (left_state >> 4) & 0x01;
      agv_drive_ptr->left.state.braking_output = (left_state >> 7) & 0x01;
      agv_drive_ptr->left.state.exceeded_overload = (left_state >> 9) & 0x01;
      agv_drive_ptr->left.state.error_warning = (left_state >> 10) & 0x01;
      agv_drive_ptr->left.state.reverse_stall = (left_state >> 12) & 0x01;
      agv_drive_ptr->left.state.forward_stall = (left_state >> 13) & 0x01;
      */

      /* right */

      agv_drive_ptr->right.current = ((float)right_current) * 0.1f;
      agv_drive_ptr->right.max_current = ((float)right_max_current) * 0.1f;

      agv_drive_ptr->right.error.error_code = right_error;

      agv_drive_ptr->right.error.encoder_fault = (right_error >> 1) & 0x01;
      agv_drive_ptr->right.error.hall_fault = (right_error >> 2) & 0x01;
      agv_drive_ptr->right.error.encoder_fault |= (right_error >> 3) & 0x01;
      agv_drive_ptr->right.error.overheating = (right_error >> 4) & 0x01;
      agv_drive_ptr->right.error.overvoltage = (right_error >> 5) & 0x01;
      agv_drive_ptr->right.error.undervoltage = (right_error >> 6) & 0x01;
      agv_drive_ptr->right.error.short_circuit = (right_error >> 7) & 0x01;
      agv_drive_ptr->right.error.overload = (right_error >> 11) & 0x01;
      agv_drive_ptr->right.error.speed_deviation = (right_error >> 12) & 0x01;

      /*
      agv_drive_ptr->right.state.start = (right_state >> 0) & 0x01;
      agv_drive_ptr->right.state.running = (right_state >> 1) & 0x01;
      agv_drive_ptr->right.state.speed_reach = (right_state >> 3) & 0x01;
      agv_drive_ptr->right.state.position_reach = (right_state >> 4) & 0x01;
      agv_drive_ptr->right.state.braking_output = (right_state >> 7) & 0x01;
      agv_drive_ptr->right.state.exceeded_overload = (right_state >> 9) & 0x01;
      agv_drive_ptr->right.state.error_warning = (right_state >> 10) & 0x01;
      agv_drive_ptr->right.state.reverse_stall = (right_state >> 12) & 0x01;
      agv_drive_ptr->right.state.forward_stall = (right_state >> 13) & 0x01;
      */


      agv_drive_ptr->temperature = ((float)temperature) * 0.1f;
      agv_drive_ptr->voltage = ((float)voltage) * 0.1f;

      agv_drive_ptr->drive_type = "SDFZ";

      ctrl_.pub.drive_ptr->publish(ctrl_.receive.agv_drive);

      ctrl_.timeout.get_drive_full = false;

      logs_info("get drive full package");

      break;
    }
    case ReceiveCmd::drive_sdfz_base:
    {
      ctrl_.receive.agv_dt.cmd_C1_drive_base.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_C1_drive_base.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      int16_t left_current = (int16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      int16_t right_current = (int16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));

      uint16_t drive_voltage = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));

      ctrl_.receive.agv.left_current = ((float)left_current) * 0.1f;
      ctrl_.receive.agv.right_current = ((float)right_current) * 0.1f;

      ctrl_.receive.agv.drive_voltage = ((float)drive_voltage) * 0.1f;

      /* ctrl_.pub.info_ptr->publish(ctrl_.receive.agv); */

      ctrl_.timeout.get_drive_base = false;

      static uint8_t cnt = 0;

      cnt++;

      if(cnt == 100)
      {
        cnt = 0;
        logs_info("get drive base package");
      }

      break;
    }
    case ReceiveCmd::software:
    {
      ctrl_.receive.agv_dt.cmd_87_software.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_87_software.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      ctrl_.timeout.get_software = false;

      logs_info("get software package");

      break;
    }
    case ReceiveCmd::hardware:
    {
      ctrl_.receive.agv_dt.cmd_88_hardware.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_88_hardware.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      ctrl_.timeout.get_hardware = false;

      logs_info("get hardware package");

      break;
    }
    case ReceiveCmd::param:
    {
      ctrl_.receive.agv_dt.cmd_89_param.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_89_param.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      uint16_t track_width    = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t wheel_base     = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));
      uint16_t wheel_diameter = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      uint16_t gear_ratio     = (uint16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));
      uint16_t encoder_line   = (uint16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));

      ctrl_.receive.agv_param.track_width    = track_width;
      ctrl_.receive.agv_param.wheel_base     = wheel_base;
      ctrl_.receive.agv_param.wheel_diameter = wheel_diameter;
      ctrl_.receive.agv_param.gear_ratio     = gear_ratio;
      ctrl_.receive.agv_param.encoder_line   = encoder_line;

      ctrl_.pub.param_ptr->publish(ctrl_.receive.agv_param);

      ctrl_.timeout.get_param = false;

      logs_info("get param package");

      break;
    }
    case ReceiveCmd::date:
    {
      ctrl_.receive.agv_dt.cmd_8A_date.clear();

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.agv_dt.cmd_8A_date.push_back(rx_buffer.at(loop));
      }

      ctrl_.pub.dt_ptr->publish(ctrl_.receive.agv_dt);

      ctrl_.timeout.get_date = false;

      logs_info("get date package");

      break;
    }
    default:
    {
      break;
    }
  }

  for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
  {
    rx_buffer.pop_front();
  }

  /* std::cout << "rx buffe size: " << rx_buffer.size() << std::endl; */
}

void Packages::receive(void)
{
  std::deque<uint8_t> &rx_buffer = serial_port_ptr_->getReadBuffer();

  while(rx_buffer.size() > 5)
  {
    if(rx_buffer.at(0) != 0xED) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(1) != 0xDE) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.size() < rx_buffer.at(5)) {return;}

    if((rx_buffer.at(5) < 10) || (rx_buffer.at(5) > 100)) {rx_buffer.pop_front(); continue;}

    uint16_t sum = 0;

    for(size_t loop = 0; loop < (rx_buffer.at(5) - 4); loop++) {sum = sum + rx_buffer.at(loop);}

    if(rx_buffer.at(rx_buffer.at(5) - 4) != ((sum >> 0) & 0xFF)) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(rx_buffer.at(5) - 3) != ((sum >> 8) & 0xFF)) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(rx_buffer.at(5) - 2) != 0xBD) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(rx_buffer.at(5) - 1) != 0xBC) {rx_buffer.pop_front(); continue;}

    #if 0
    printf("rx buffe: ");
    for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
    {
      printf("%02X ", rx_buffer.at(loop));
    }
    printf("\n");
    #endif

    receiveParse();

    if(receiveFlagCheck() == true)
    {
      ctrl_.timeout.pkg_exist = false;

    }
    else
    {
      ctrl_.timeout.receive_begin = false;
      ctrl_.timeout.receive_interval.clear();

      ctrl_.timeout.pkg_exist = true;
    }
  }
}

void Packages::transmit(void)
{
  if(ctrl_.transmit.tx_list.size() == 0) {return;}

  std::deque<uint8_t> &tx_buffer = serial_port_ptr_->getWriteBuffer();

  if(tx_buffer.size() != 0) {return;}

  if(ctrl_.timeout.tx_list_begin == false)
  {
    ctrl_.timeout.tx_list_begin = true;
    ctrl_.timeout.tx_list_interval.clear();
    ctrl_.timeout.tx_list_interval.start();

    return;
  }

  if(ctrl_.timeout.tx_list_begin == true)
  {
    if(ctrl_.timeout.tx_list_interval.timeout(TimeIntervalType::milliseconds, TIMEOUT_TX_LIST_TIME) == true)
    {
      ctrl_.timeout.tx_list_begin = false;
      ctrl_.timeout.tx_list_interval.clear();

      std::vector<uint8_t> &tx_value = ctrl_.transmit.tx_list.at(0);

      for(size_t loop = 0; loop < tx_value.size(); loop++)
      {
        tx_buffer.push_back(tx_value.at(loop));
      }

      ctrl_.transmit.tx_list.erase(ctrl_.transmit.tx_list.begin());
    }
  }
}

void Packages::begin(void)
{
  if(fsm_pkg_ptr_->getNowState() == "begin")
  {
    if((serial_port_ptr_->getFsmState() == "idle") || \
       (serial_port_ptr_->getFsmState() == "read") || \
       (serial_port_ptr_->getFsmState() == "read success") || \
       (serial_port_ptr_->getFsmState() == "write") || \
       (serial_port_ptr_->getFsmState() == "write success") || \
       (serial_port_ptr_->getFsmState() == "link") || \
       (serial_port_ptr_->getFsmState() == "link success"))
    {
      fsm_pkg_ptr_->dispatch(FsmPackageEvent::Init());
    }

    return;
  }
}

void Packages::init(void)
{
  if(fsm_pkg_ptr_->getNowState() == "init")
  {
    static size_t delay_sth = 0;
    static size_t send_cnt = 0;

    delay_sth++;

    if(delay_sth == 50)
    {
      delay_sth = 0;

      if     (send_cnt == 0) {send_cnt++; getSoftwareVersion();}
      else if(send_cnt == 1) {send_cnt++; getHardwareVersion();}
      else if(send_cnt == 2) {send_cnt++; getParam();}
      else if(send_cnt == 3) {send_cnt++; getDate();}
      else if(send_cnt == 4) {send_cnt++; getDrive();}
      else if(send_cnt == 5) {send_cnt++; uploadState(true);}
      else if(send_cnt == 6) {send_cnt++; uploadDrive(true);}

      if(send_cnt == 7)
      {
        send_cnt = 0;
        delay_sth = 0;
        fsm_pkg_ptr_->dispatch(FsmPackageEvent::Idle());
      }
    }

    #if 0
    getSoftwareVersion();
    getHardwareVersion();
    getParam();
    getDate();
    getDrive();

    uploadState(true);
    uploadDrive(true);

    fsm_pkg_ptr_->dispatch(FsmPackageEvent::Idle());
    #endif

    return;
  }
}

void Packages::idle(void)
{
  if(fsm_pkg_ptr_->getNowState() == "idle")
  {
    fsm_pkg_ptr_->dispatch(FsmPackageEvent::ReadDrive());

    return;
  }
}

void Packages::readDrive(void)
{
  if(fsm_pkg_ptr_->getNowState() == "read drive")
  {
    if(ctrl_.timeout.read_drive_begin == false)
    {
      ctrl_.timeout.read_drive_begin = true;
      ctrl_.timeout.read_drive_interval.clear();
      ctrl_.timeout.read_drive_interval.start();
    }
    else if(ctrl_.timeout.read_drive_begin == true)
    {
      if(ctrl_.timeout.read_drive_interval.timeout(TimeIntervalType::seconds, TIMEOUT_GET_DRIVE_TIME) == true)
      {
        ctrl_.timeout.read_drive_begin = false;
        ctrl_.timeout.read_drive_interval.clear();

        getDrive();
      }
    }

    fsm_pkg_ptr_->dispatch(FsmPackageEvent::PackageCheck());

    return;
  }
}

void Packages::package(void)
{
  if(fsm_pkg_ptr_->getNowState() == "package check")
  {
    if(ctrl_.timeout.pkg_exist == true) {fsm_pkg_ptr_->dispatch(FsmPackageEvent::PackageExist());}
    if(ctrl_.timeout.pkg_exist == false) {fsm_pkg_ptr_->dispatch(FsmPackageEvent::PackageNotExist());}

    return;
  }
}

void Packages::timeout(void)
{
  if(fsm_pkg_ptr_->getNowState() == "package timeout check")
  {
    if(ctrl_.timeout.receive_begin == false)
    {
      ctrl_.timeout.receive_begin = true;
      ctrl_.timeout.receive_interval.clear();
      ctrl_.timeout.receive_interval.start();

      return;
    }

    if(ctrl_.timeout.receive_begin == true)
    {
      if(ctrl_.timeout.receive_interval.timeout(TimeIntervalType::seconds, TIMEOUT_RECEIVE_TIME) == true)
      {
        ctrl_.timeout.receive_begin = false;
        ctrl_.timeout.receive_interval.clear();

        if(ctrl_.timeout.get_drive_full == true) {logs_error("get drive full info package timeout.");}
        if(ctrl_.timeout.get_software == true)   {logs_error("get software version package timeout.");}
        if(ctrl_.timeout.get_hardware == true)   {logs_error("get hardware version package timeout.");}
        if(ctrl_.timeout.get_param == true)      {logs_error("get param package timeout.");}
        if(ctrl_.timeout.get_date == true)       {logs_error("get date package timeout.");}
        if(ctrl_.timeout.state_timeout == true)  {logs_error("get state package timeout.");}
        if(ctrl_.timeout.drive_timeout == true)  {logs_error("get drvie base info package timeout.");}

        if(ctrl_.auto_restart == true)
        {
          fsm_pkg_ptr_->dispatch(FsmPackageEvent::Restart());
        }

        return;
      }

      fsm_pkg_ptr_->dispatch(FsmPackageEvent::Idle());
    }

    return;
  }

  if(fsm_pkg_ptr_->getNowState() == "restart")
  {
    if(ctrl_.timeout.restart_begin == false)
    {
      logs_info("get package timeout. restart.");

      ctrl_.timeout.restart_begin = true;
      ctrl_.timeout.restart_interval.clear();
      ctrl_.timeout.restart_interval.start();

      return;
    }

    if(ctrl_.timeout.restart_begin == true)
    {
      if(ctrl_.timeout.restart_interval.timeout(TimeIntervalType::seconds, 2) == true)
      {
          ctrl_.timeout.restart_begin = false;
          ctrl_.timeout.restart_interval.clear();

          Timeout timeout;
          ctrl_.timeout = timeout;

          fsm_pkg_ptr_->dispatch(FsmPackageEvent::Begin());
      }

      return;
    }

    return;
  }
}
