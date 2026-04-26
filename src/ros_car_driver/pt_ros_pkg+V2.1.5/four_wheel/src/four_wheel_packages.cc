
#include <deque>
#include <ros/ros.h>

#include "nav_msgs/Odometry.h"
#include "tf/tf.h"

#include "four_wheel_msgs/four_wheel_drive.h"
#include "four_wheel_msgs/four_wheel_error.h"
#include "four_wheel_msgs/four_wheel_param.h"
#include "four_wheel_msgs/four_wheel_pt.h"
#include "four_wheel_msgs/four_wheel.h"

#include "four_wheel_binary.h"
#include "four_wheel_packages.h"
#include "four_wheel_serial_ports.h"
#include "four_wheel_fsm.h"
#include "four_wheel_time_interval.h"
#include "four_wheel.h"

using Packages = packages::Packages;

using TimeInterval = time_interval::TimeInterval;
using TimeIntervalType = time_interval::TimeType;

using SerialPortPtr = serial_ports::SerialPort *;

using FsmPackage = fsm::Package::Fsm;
using FsmPackageEvent = fsm::Package::Event;
using FsmPackageState = fsm::Package::State;

using FourWheelDriveMsg = four_wheel_msgs::four_wheel_drive;
using FourWheelErrorMsg = four_wheel_msgs::four_wheel_error;
using FourWheelParamMsg = four_wheel_msgs::four_wheel_param;
using FourWheelPtMsg    = four_wheel_msgs::four_wheel_pt;
using FourWheelMsg      = four_wheel_msgs::four_wheel;

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

void Packages::getPackage(FourWheelMsg &four_wheel_pkg,
                          FourWheelErrorMsg &four_wheel_error_pkg,
                          FourWheelParamMsg &four_wheel_param_pkg,
                          FourWheelDriveMsg &four_wheel_drive_pkg,
                          FourWheelPtMsg &four_wheel_pt_pkg,
                          nav_msgs::Odometry &fw_odom_pkg)
{
  four_wheel_pkg = ctrl_.receive.fw;
  four_wheel_error_pkg = ctrl_.receive.fw_error;
  four_wheel_param_pkg = ctrl_.receive.fw_param;
  four_wheel_drive_pkg = ctrl_.receive.fwd;
  four_wheel_pt_pkg = ctrl_.receive.fw_pt;
  fw_odom_pkg = ctrl_.odom.value;
}

void Packages::setPubPtr(ros::Publisher *info_ptr,
                         ros::Publisher *error_ptr,
                         ros::Publisher *param_ptr,
                         ros::Publisher *drive_ptr,
                         ros::Publisher *pt_ptr,
                         ros::Publisher *odom_ptr)
{
  ctrl_.pub.info_ptr = info_ptr;
  ctrl_.pub.error_ptr = error_ptr;
  ctrl_.pub.param_ptr = param_ptr;
  ctrl_.pub.drive_ptr = drive_ptr;
  ctrl_.pub.pt_ptr = pt_ptr;
  ctrl_.pub.odom_ptr = odom_ptr;
}

void Packages::setPrintOriginal(bool enable)
{
  ctrl_.print_original = enable;
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

  if((fault_type != 0x01) && (fault_type != 0x02) && (fault_type != 0x03) && (fault_type != 0xFF)) {return;}

  type_ptr = (uint16_t *)&data_buffer[0];

  switch(fault_type)
  {
    case 0x01: {*type_ptr = B_0000_0001; break;}    /* 驱动器 */
    case 0x02: {*type_ptr = B_0000_0010; break;}    /* 前碰撞 */
    case 0x03: {*type_ptr = B_0000_0100; break;}    /* 后碰撞 */
    case 0xFF: {*type_ptr = B_0000_0111; break;}    /* 全清除 */
    default: {break;}
  }

  setTxBuffe(TransmitCmd::fault_clean, 2, data_buffer, tx_buffer);
  ctrl_.transmit.tx_list.push_back(tx_buffer);
}

void Packages::setCharge(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[2] = {0};

  uint16_t *enable_ptr = nullptr;

  enable_ptr = (uint16_t *)&data_buffer[0];

  *enable_ptr = enable;

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
      ctrl_.receive.fw_pt.cmd_81_state.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_81_state.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

      uint16_t voltage = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t state   = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));
      uint16_t charge  = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      int16_t linear   = (int16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));
      int16_t angular  = (int16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));
      int16_t fl_speed = (int16_t)((rx_buffer[17] << 8) | (rx_buffer[16] << 0));
      int16_t fr_speed = (int16_t)((rx_buffer[19] << 8) | (rx_buffer[18] << 0));
      int16_t bl_speed = (int16_t)((rx_buffer[21] << 8) | (rx_buffer[20] << 0));
      int16_t br_speed = (int16_t)((rx_buffer[23] << 8) | (rx_buffer[22] << 0));

      FourWheelMsg *fw_ptr = &ctrl_.receive.fw;
      FourWheelErrorMsg *fw_error_ptr = &ctrl_.receive.fw_error;

      fw_ptr->drive_error = false;

      fw_ptr->voltage            = ((float)voltage) * 0.1f;
      fw_ptr->stop_button        = (((state >> 0) & 0x01) == 0x00) ? false : true;
      fw_ptr->remote_ctrl_stop   = (((state >> 1) & 0x01) == 0x00) ? false : true;
      fw_ptr->software_stop      = (((state >> 2) & 0x01) == 0x00) ? false : true;
      fw_ptr->remote_ctrl_error  = (((state >> 3) & 0x01) == 0x00) ? false : true;
      fw_ptr->drive_error       |= (((state >> 4) & 0x01) == 0x00) ? false : true;
      fw_ptr->drive_error       |= (((state >> 5) & 0x01) == 0x00) ? false : true;
      fw_ptr->drive_error       |= (((state >> 6) & 0x01) == 0x00) ? false : true;
      fw_ptr->drive_error       |= (((state >> 7) & 0x01) == 0x00) ? false : true;
      fw_ptr->drive_error       |= (((state >> 8) & 0x01) == 0x00) ? false : true;
      fw_ptr->drive_error       |= (((state >> 9) & 0x01) == 0x00) ? false : true;
      fw_ptr->front_collision    = (((state >> 10) & 0x01) == 0x00) ? false : true;
      fw_ptr->back_collision     = (((state >> 11) & 0x01) == 0x00) ? false : true;
      fw_ptr->charge_enable      = (((state >> 12) & 0x01) == 0x00) ? false : true;
      fw_ptr->charge_state       = (uint8_t)charge;
      fw_ptr->linear_velocity    = ((float)linear) * 0.001f;
      fw_ptr->angular_velocity   = ((float)angular) * 0.001f;
      fw_ptr->front_left_speed   = fl_speed;
      fw_ptr->front_right_speed  = fr_speed;
      fw_ptr->back_left_speed    = bl_speed;
      fw_ptr->back_right_speed   = br_speed;

      fw_error_ptr->remote_ctrl_lost = (((state >> 3) & 0x01) == 0x00) ? false : true;
      fw_error_ptr->front_drive_lost = (((state >> 4) & 0x01) == 0x00) ? false : true;
      fw_error_ptr->back_drive_lost  = (((state >> 5) & 0x01) == 0x00) ? false : true;
      fw_error_ptr->front_left_drive_error  = (((state >> 6) & 0x01) == 0x00) ? false : true;
      fw_error_ptr->front_right_drive_error = (((state >> 7) & 0x01) == 0x00) ? false : true;
      fw_error_ptr->back_left_drive_error   = (((state >> 8) & 0x01) == 0x00) ? false : true;
      fw_error_ptr->back_left_drive_error   = (((state >> 9) & 0x01) == 0x00) ? false : true;

      if((logs_display == true) && (ctrl_.print_original == true))
      {
        printf("voltage:           %f\n", fw_ptr->voltage);
        printf("stop_button:       %s\n", fw_ptr->stop_button       ? "true" : "false");
        printf("remote_ctrl_stop:  %s\n", fw_ptr->remote_ctrl_stop  ? "true" : "false");
        printf("software_stop:     %s\n", fw_ptr->software_stop     ? "true" : "false");
        printf("remote_ctrl_error: %s\n", fw_ptr->remote_ctrl_error ? "true" : "false");
        printf("drive_error:       %s\n", fw_ptr->drive_error       ? "true" : "false");
        printf("front_collision:   %s\n", fw_ptr->front_collision   ? "true" : "false");
        printf("back_collision:    %s\n", fw_ptr->back_collision    ? "true" : "false");
        printf("charge_enable:     %s\n", fw_ptr->charge_enable     ? "true" : "false");
        printf("charge_state:      %d\n", fw_ptr->charge_state);
        printf("linear_velocity:   %f\n", fw_ptr->linear_velocity);
        printf("angular_velocity:  %f\n", fw_ptr->angular_velocity);
        printf("front_left_speed:  %d\n", fw_ptr->front_left_speed);
        printf("front_right_speed: %d\n", fw_ptr->front_right_speed);
        printf("back_left_speed:   %d\n", fw_ptr->back_left_speed);
        printf("back_right_speed:  %d\n", fw_ptr->back_right_speed);

        printf("\n");

        printf("remote_ctrl_lost:        %s\n", fw_error_ptr->remote_ctrl_lost        ? "true" : "false");
        printf("front_drive_lost:        %s\n", fw_error_ptr->front_drive_lost        ? "true" : "false");
        printf("back_drive_lost:         %s\n", fw_error_ptr->back_drive_lost         ? "true" : "false");
        printf("front_left_drive_error:  %s\n", fw_error_ptr->front_left_drive_error  ? "true" : "false");
        printf("front_right_drive_error: %s\n", fw_error_ptr->front_right_drive_error ? "true" : "false");
        printf("back_left_drive_error:   %s\n", fw_error_ptr->back_left_drive_error   ? "true" : "false");
        printf("back_left_drive_error:   %s\n", fw_error_ptr->back_left_drive_error   ? "true" : "false");

        printf("\n");
      }

      ctrl_.pub.info_ptr->publish(ctrl_.receive.fw);
      ctrl_.pub.error_ptr->publish(ctrl_.receive.fw_error);

      odom_calculation(fw_ptr->linear_velocity, fw_ptr->angular_velocity);

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
    case ReceiveCmd::drive_full:
    {
      ctrl_.receive.fw_pt.cmd_B0_drive_full.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_B0_drive_full.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

      uint16_t fl_error = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t fr_error = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));
      uint16_t bl_error = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      uint16_t br_error = (uint16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));

      uint16_t fl_state = (uint16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));
      uint16_t fr_state = (uint16_t)((rx_buffer[17] << 8) | (rx_buffer[16] << 0));
      uint16_t bl_state = (uint16_t)((rx_buffer[19] << 8) | (rx_buffer[18] << 0));
      uint16_t br_state = (uint16_t)((rx_buffer[21] << 8) | (rx_buffer[20] << 0));

      int16_t fl_current = (int16_t)((rx_buffer[23] << 8) | (rx_buffer[22] << 0));
      int16_t fr_current = (int16_t)((rx_buffer[25] << 8) | (rx_buffer[24] << 0));
      int16_t bl_current = (int16_t)((rx_buffer[27] << 8) | (rx_buffer[26] << 0));
      int16_t br_current = (int16_t)((rx_buffer[29] << 8) | (rx_buffer[28] << 0));

      uint16_t fl_max_current = (uint16_t)((rx_buffer[31] << 8) | (rx_buffer[30] << 0));
      uint16_t fr_max_current = (uint16_t)((rx_buffer[33] << 8) | (rx_buffer[32] << 0));
      uint16_t bl_max_current = (uint16_t)((rx_buffer[35] << 8) | (rx_buffer[34] << 0));
      uint16_t br_max_current = (uint16_t)((rx_buffer[37] << 8) | (rx_buffer[36] << 0));

      uint16_t front_temperature = (uint16_t)((rx_buffer[39] << 8) | (rx_buffer[38] << 0));
      uint16_t back_temperature = (uint16_t)((rx_buffer[41] << 8) | (rx_buffer[40] << 0));

      uint16_t front_voltage = (uint16_t)((rx_buffer[43] << 8) | (rx_buffer[42] << 0));
      uint16_t back_voltage = (uint16_t)((rx_buffer[45] << 8) | (rx_buffer[44] << 0));

      FourWheelDriveMsg *fwd_ptr = &ctrl_.receive.fwd;

      /* front left */

      fwd_ptr->front_left.current = ((float)fl_current) * 0.1f;
      fwd_ptr->front_left.max_current = ((float)fl_max_current) * 0.1f;

      fwd_ptr->front_left.state.state_code = fl_state;
      fwd_ptr->front_left.error.error_code = fl_error;

      fwd_ptr->front_left.error.undervoltage = (fl_error >> 0) & 0x01;
      fwd_ptr->front_left.error.position_fault = (fl_error >> 1) & 0x01;
      fwd_ptr->front_left.error.hall_fault = (fl_error >> 2) & 0x01;
      fwd_ptr->front_left.error.overcurrent = (fl_error >> 3) & 0x01;
      fwd_ptr->front_left.error.overload = (fl_error >> 4) & 0x01;
      fwd_ptr->front_left.error.overheating = (fl_error >> 7) & 0x01;
      fwd_ptr->front_left.error.speed_deviation = (fl_error >> 10) & 0x01;
      fwd_ptr->front_left.error.free_wheeling_fault = (fl_error >> 13) & 0x01;
      fwd_ptr->front_left.error.overheating |= (fl_error >> 14) & 0x01;

      fwd_ptr->front_left.state.start = (fl_state >> 0) & 0x01;
      fwd_ptr->front_left.state.running = (fl_state >> 1) & 0x01;
      fwd_ptr->front_left.state.speed_reach = (fl_state >> 3) & 0x01;
      fwd_ptr->front_left.state.position_reach = (fl_state >> 4) & 0x01;
      fwd_ptr->front_left.state.braking_output = (fl_state >> 7) & 0x01;
      fwd_ptr->front_left.state.exceeded_overload = (fl_state >> 9) & 0x01;
      fwd_ptr->front_left.state.error_warning = (fl_state >> 10) & 0x01;
      fwd_ptr->front_left.state.reverse_stall = (fl_state >> 12) & 0x01;
      fwd_ptr->front_left.state.forward_stall = (fl_state >> 13) & 0x01;

      /* front right */

      fwd_ptr->front_right.current = ((float)fr_current) * 0.1f;
      fwd_ptr->front_right.max_current = ((float)fr_max_current) * 0.1f;

      fwd_ptr->front_right.state.state_code = fr_state;
      fwd_ptr->front_right.error.error_code = fr_error;

      fwd_ptr->front_right.error.undervoltage = (fr_error >> 0) & 0x01;
      fwd_ptr->front_right.error.position_fault = (fr_error >> 1) & 0x01;
      fwd_ptr->front_right.error.hall_fault = (fr_error >> 2) & 0x01;
      fwd_ptr->front_right.error.overcurrent = (fr_error >> 3) & 0x01;
      fwd_ptr->front_right.error.overload = (fr_error >> 4) & 0x01;
      fwd_ptr->front_right.error.overheating = (fr_error >> 7) & 0x01;
      fwd_ptr->front_right.error.speed_deviation = (fr_error >> 10) & 0x01;
      fwd_ptr->front_right.error.free_wheeling_fault = (fr_error >> 13) & 0x01;
      fwd_ptr->front_right.error.overheating |= (fr_error >> 14) & 0x01;

      fwd_ptr->front_right.state.start = (fr_state >> 0) & 0x01;
      fwd_ptr->front_right.state.running = (fr_state >> 1) & 0x01;
      fwd_ptr->front_right.state.speed_reach = (fr_state >> 3) & 0x01;
      fwd_ptr->front_right.state.position_reach = (fr_state >> 4) & 0x01;
      fwd_ptr->front_right.state.braking_output = (fr_state >> 7) & 0x01;
      fwd_ptr->front_right.state.exceeded_overload = (fr_state >> 9) & 0x01;
      fwd_ptr->front_right.state.error_warning = (fr_state >> 10) & 0x01;
      fwd_ptr->front_right.state.reverse_stall = (fr_state >> 12) & 0x01;
      fwd_ptr->front_right.state.forward_stall = (fr_state >> 13) & 0x01;

      /* back left */

      fwd_ptr->back_left.current = ((float)bl_current) * 0.1f;
      fwd_ptr->back_left.max_current = ((float)bl_max_current) * 0.1f;

      fwd_ptr->back_left.state.state_code = bl_state;
      fwd_ptr->back_left.error.error_code = bl_error;

      fwd_ptr->back_left.error.undervoltage = (bl_error >> 0) & 0x01;
      fwd_ptr->back_left.error.position_fault = (bl_error >> 1) & 0x01;
      fwd_ptr->back_left.error.hall_fault = (bl_error >> 2) & 0x01;
      fwd_ptr->back_left.error.overcurrent = (bl_error >> 3) & 0x01;
      fwd_ptr->back_left.error.overload = (bl_error >> 4) & 0x01;
      fwd_ptr->back_left.error.overheating = (bl_error >> 7) & 0x01;
      fwd_ptr->back_left.error.speed_deviation = (bl_error >> 10) & 0x01;
      fwd_ptr->back_left.error.free_wheeling_fault = (bl_error >> 13) & 0x01;
      fwd_ptr->back_left.error.overheating |= (bl_error >> 14) & 0x01;

      fwd_ptr->back_left.state.start = (bl_state >> 0) & 0x01;
      fwd_ptr->back_left.state.running = (bl_state >> 1) & 0x01;
      fwd_ptr->back_left.state.speed_reach = (bl_state >> 3) & 0x01;
      fwd_ptr->back_left.state.position_reach = (bl_state >> 4) & 0x01;
      fwd_ptr->back_left.state.braking_output = (bl_state >> 7) & 0x01;
      fwd_ptr->back_left.state.exceeded_overload = (bl_state >> 9) & 0x01;
      fwd_ptr->back_left.state.error_warning = (bl_state >> 10) & 0x01;
      fwd_ptr->back_left.state.reverse_stall = (bl_state >> 12) & 0x01;
      fwd_ptr->back_left.state.forward_stall = (bl_state >> 13) & 0x01;

      /* back right */

      fwd_ptr->back_right.current = ((float)br_current) * 0.1f;
      fwd_ptr->back_right.max_current = ((float)br_max_current) * 0.1f;

      fwd_ptr->back_right.state.state_code = br_state;
      fwd_ptr->back_right.error.error_code = br_error;

      fwd_ptr->back_right.error.undervoltage = (br_error >> 0) & 0x01;
      fwd_ptr->back_right.error.position_fault = (br_error >> 1) & 0x01;
      fwd_ptr->back_right.error.hall_fault = (br_error >> 2) & 0x01;
      fwd_ptr->back_right.error.overcurrent = (br_error >> 3) & 0x01;
      fwd_ptr->back_right.error.overload = (br_error >> 4) & 0x01;
      fwd_ptr->back_right.error.overheating = (br_error >> 7) & 0x01;
      fwd_ptr->back_right.error.speed_deviation = (br_error >> 10) & 0x01;
      fwd_ptr->back_right.error.free_wheeling_fault = (br_error >> 13) & 0x01;
      fwd_ptr->back_right.error.overheating |= (br_error >> 14) & 0x01;

      fwd_ptr->back_right.state.start = (br_state >> 0) & 0x01;
      fwd_ptr->back_right.state.running = (br_state >> 1) & 0x01;
      fwd_ptr->back_right.state.speed_reach = (br_state >> 3) & 0x01;
      fwd_ptr->back_right.state.position_reach = (br_state >> 4) & 0x01;
      fwd_ptr->back_right.state.braking_output = (br_state >> 7) & 0x01;
      fwd_ptr->back_right.state.exceeded_overload = (br_state >> 9) & 0x01;
      fwd_ptr->back_right.state.error_warning = (br_state >> 10) & 0x01;
      fwd_ptr->back_right.state.reverse_stall = (br_state >> 12) & 0x01;
      fwd_ptr->back_right.state.forward_stall = (br_state >> 13) & 0x01;

      /* front */

      fwd_ptr->front.temperature = ((float)front_temperature) * 0.1f;
      fwd_ptr->front.voltage = ((float)front_voltage) * 0.1f;

      /* back */

      fwd_ptr->back.temperature = ((float)back_temperature) * 0.1f;
      fwd_ptr->back.voltage = ((float)back_voltage) * 0.1f;

      ctrl_.pub.drive_ptr->publish(ctrl_.receive.fwd);

      ctrl_.timeout.get_drive_full = false;

      logs_info("get drive full package");

      break;
    }
    case ReceiveCmd::drive_base:
    {
      ctrl_.receive.fw_pt.cmd_B1_drive_base.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_B1_drive_base.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

      int16_t fl_current = (int16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      int16_t fr_current = (int16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));
      int16_t bl_current = (int16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      int16_t br_current = (int16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));

      uint16_t front_voltage = (uint16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));
      uint16_t back_voltage = (uint16_t)((rx_buffer[17] << 8) | (rx_buffer[16] << 0));

      ctrl_.receive.fw.front_left_current = ((float)fl_current) * 0.1f;
      ctrl_.receive.fw.front_right_current = ((float)fr_current) * 0.1f;
      ctrl_.receive.fw.back_left_current = ((float)bl_current) * 0.1f;
      ctrl_.receive.fw.back_right_current = ((float)br_current) * 0.1f;

      ctrl_.receive.fw.front_voltage = ((float)front_voltage) * 0.1f;
      ctrl_.receive.fw.back_voltage = ((float)back_voltage) * 0.1f;

      if((logs_display == true) && (ctrl_.print_original == true))
      {
        printf("front_left_current:  %f\n", ctrl_.receive.fw.front_left_current);
        printf("front_right_current: %f\n", ctrl_.receive.fw.front_right_current);
        printf("back_left_current:   %f\n", ctrl_.receive.fw.back_left_current);
        printf("back_right_current:  %f\n", ctrl_.receive.fw.back_right_current);
        printf("front_voltage:       %f\n", ctrl_.receive.fw.front_voltage);
        printf("back_voltage:        %f\n", ctrl_.receive.fw.back_voltage);

        printf("\n");
      }

      /* ctrl_.pub.info_ptr->publish(ctrl_.receive.fw); */

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
      ctrl_.receive.fw_pt.cmd_87_software.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_87_software.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

      ctrl_.timeout.get_software = false;

      logs_info("get software package");

      break;
    }
    case ReceiveCmd::hardware:
    {
      ctrl_.receive.fw_pt.cmd_88_hardware.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_88_hardware.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

      ctrl_.timeout.get_hardware = false;

      logs_info("get hardware package");

      break;
    }
    case ReceiveCmd::param:
    {
      ctrl_.receive.fw_pt.cmd_89_param.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_89_param.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

      uint16_t track_width    = (uint16_t)((rx_buffer[7] << 8) | (rx_buffer[6] << 0));
      uint16_t wheel_base     = (uint16_t)((rx_buffer[9] << 8) | (rx_buffer[8] << 0));
      uint16_t wheel_diameter = (uint16_t)((rx_buffer[11] << 8) | (rx_buffer[10] << 0));
      uint16_t gear_ratio     = (uint16_t)((rx_buffer[13] << 8) | (rx_buffer[12] << 0));
      uint16_t encoder_line   = (uint16_t)((rx_buffer[15] << 8) | (rx_buffer[14] << 0));

      ctrl_.receive.fw_param.track_width = track_width;
      ctrl_.receive.fw_param.wheel_base = wheel_base;
      ctrl_.receive.fw_param.wheel_diameter = wheel_diameter;
      ctrl_.receive.fw_param.gear_ratio = gear_ratio;
      ctrl_.receive.fw_param.encoder_line = encoder_line;

      if((logs_display == true) && (ctrl_.print_original == true))
      {
        printf("track_width:    %d\n", ctrl_.receive.fw_param.track_width);
        printf("wheel_base:     %d\n", ctrl_.receive.fw_param.wheel_base);
        printf("wheel_diameter: %d\n", ctrl_.receive.fw_param.wheel_diameter);
        printf("gear_ratio:     %d\n", ctrl_.receive.fw_param.gear_ratio);
        printf("encoder_line:   %d\n", ctrl_.receive.fw_param.encoder_line);

        printf("\n");
      }

      ctrl_.pub.param_ptr->publish(ctrl_.receive.fw_param);

      ctrl_.timeout.get_param = false;

      logs_info("get param package");

      break;
    }
    case ReceiveCmd::date:
    {
      ctrl_.receive.fw_pt.cmd_8A_date.clear();

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n"); printf("[four wheel] [info] \n\n");}

      for(size_t loop = 0; loop < rx_buffer.at(5); loop++)
      {
        ctrl_.receive.fw_pt.cmd_8A_date.push_back(rx_buffer.at(loop));

        if((logs_display == true) && (ctrl_.print_original == true))
        {printf("%02X ", rx_buffer.at(loop));}
      }

      if((logs_display == true) && (ctrl_.print_original == true))
      {printf("\n\n");}

      ctrl_.pub.pt_ptr->publish(ctrl_.receive.fw_pt);

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
