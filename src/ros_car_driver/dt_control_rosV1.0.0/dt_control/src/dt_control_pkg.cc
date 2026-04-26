
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
#include "dt_control_pkg.h"
#include "dt_control.h"

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

boost::array<double, 36> cov_array = {1e-3, 0, 0, 0, 0, 0,
                                      0, 1e-3, 0, 0, 0, 0,
                                      0, 0, 1e6, 0, 0, 0,
                                      0, 0, 0, 1e6, 0, 0,
                                      0, 0, 0, 0, 1e6, 0,
                                      0, 0, 0, 0, 0, 1e3};

void serialInit(void)
{
  if(ctrl.step.finish() == true) {return;}

  ctrl.step.create = serialCreate();
  if(ctrl.step.create == false) {serialReset(); return;}

  ctrl.step.close = serialClose();
  if(ctrl.step.close == false) {serialReset(); return;}

  ctrl.step.open = serialOpen();
  if(ctrl.step.open == false) {serialReset(); return;}
}

void serialReset(void)
{
    delete ctrl.port.ptr;
    ctrl.port.ptr = nullptr;
    ctrl.step.clean();
    ctrl.port.rx.clear();
    for(auto &item : ctrl.port.tx) {item.clear();}
    ctrl.port.tx.clear();
    pkgLoopReset();
}

bool serialCreate(void)
{
  try
  {
    logs_info("serial port try create.");

    ctrl.port.ptr = new serial::Serial(ctrl.param.port, ctrl.param.baudrate, serial::Timeout::simpleTimeout(1000), \
                                        serial::bytesize_t::eightbits, serial::parity_t::parity_none, \
                                        serial::stopbits_t::stopbits_one, serial::flowcontrol_t::flowcontrol_none);
  }
  catch(const std::exception& e)
  {
    logs_error("serial port create fail.");

    logs_error_stream(e.what());

    return false;
  }

  logs_info("serial port create succeed.");

  return true;
}

bool serialClose(void)
{
  try
  {
    logs_info("serial port try close.");

    ctrl.port.ptr->close();
  }
  catch(const std::exception& e)
  {
    logs_error("serial port close fail.");

    logs_error_stream(e.what());

    return false;
  }

  logs_info("serial port close succeed.");

  return true;
}

bool serialOpen(void)
{
  try
  {
    logs_info("serial port try open.");

    ctrl.port.ptr->open();
  }
  catch(const std::exception& e)
  {
    logs_error("serial port open fail.");

    logs_error_stream(e.what());

    return false;
  }

  logs_info("serial port open succeed.");

  return true;
}

bool serialLink(void)
{
  int link_state = 0;

  link_state = access(ctrl.param.port.c_str(), F_OK);

  if(link_state == 0) {return true;}

  if(link_state == -1) {return false;}

  return false;
}

void serialRead(void)
{
  if(ctrl.step.finish() != true) {return;}

  size_t available_num = 0;

  try
  {
    /* logs_info("serial port try get available."); */

    available_num = ctrl.port.ptr->available();
  }
  catch(const std::exception& e)
  {
    logs_error("serial port get available fail.");

    logs_error_stream(e.what());

    serialReset();

    return;
  }

  size_t read_byte = 0;
  uint8_t array[available_num] = {0};

  try
  {
    /* logs_info("serial port try read."); */

    read_byte = ctrl.port.ptr->read(array, available_num);
  }
  catch(const std::exception& e)
  {
    logs_error("serial port read fail.");

    logs_error_stream(e.what());

    serialReset();

    return;
  }

  /* logs_info("serial port read succeed."); */

  for(size_t loop = 0; loop < read_byte; loop++)
  {
    ctrl.port.rx.push_back(array[loop]);
  }

  return;
}

void serialWrite(void)
{
  /* write cycle use 1ms */

  if(ctrl.step.finish() != true) {return;}

  if(ctrl.port.tx.size() == 0) {return;}

  size_t tx_size = 0;

  tx_size = ctrl.port.tx.at(0).size();

  if(tx_size == 0) {return;}

  size_t write_byte = 0;
  uint8_t array[tx_size] = {0};

  for(size_t loop = 0; loop < tx_size; loop++)
  {
    array[loop] = ctrl.port.tx.at(0).at(loop);
  }

  try
  {
    /* logs_info("serial port try write."); */

    write_byte = ctrl.port.ptr->write(array, tx_size);
  }
  catch(const std::exception& e)
  {
    logs_error("serial port write fail.");

    logs_error_stream(e.what());

    serialReset();

    return;
  }

  if(write_byte == tx_size)
  {
    /* logs_info("serial port write succeed."); */

    ctrl.port.tx.at(0).clear();
    ctrl.port.tx.pop_front();
  }
}

void pkgReceive(void)
{
  if(ctrl.step.finish() != true) {return;}

  std::deque<uint8_t> &rx_buffer = ctrl.port.rx;

  while(rx_buffer.size() > 6)
  {
    if(rx_buffer.at(0) != 0xED) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(1) != 0xDE) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.size() < rx_buffer.at(2)) {return;}

    if(rx_buffer.at(2) < 6) {rx_buffer.pop_front(); continue;}

    uint16_t sum = 0;

    for(size_t loop = 0; loop < (rx_buffer.at(2) - 2); loop++) {sum = sum + rx_buffer.at(loop);}

    if(rx_buffer.at(rx_buffer.at(2) - 2) != ((sum >> 0) & 0xFF)) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(rx_buffer.at(2) - 1) != ((sum >> 8) & 0xFF)) {rx_buffer.pop_front(); continue;}

    if((ctrl.param.logs == true) && (ctrl.param.original == true))
    {
      printf("rx frame: ");
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {
        printf("%02X ", rx_buffer.at(loop));
      }
      printf("\n");
    }

    pkgProcess();

    for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
    {
      rx_buffer.pop_front();
    }

    /* std::cout << "rx buffe size: " << rx_buffer.size() << std::endl; */
  }
}

void pkgProcess(void)
{
  std::deque<uint8_t> &rx_buffer = ctrl.port.rx;

  switch((ReceiveCmd)rx_buffer.at(3))
  {
    case ReceiveCmd::state:
    {
      ctrl.msg.buff.cmd_80_state.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_80_state.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      uint8_t ctrl_mode   = (uint8_t)rx_buffer.at(4);
      uint8_t  percent    = (uint8_t)rx_buffer.at(5);
      uint16_t voltage    = (uint16_t)((rx_buffer.at(7) << 8) | (rx_buffer.at(6) << 0));
      uint16_t state      = (uint16_t)((rx_buffer.at(9) << 8) | (rx_buffer.at(8) << 0));
      uint16_t err        = (uint16_t)((rx_buffer.at(11) << 8) | (rx_buffer.at(10) << 0));
      int16_t linear      = (int16_t)((rx_buffer.at(13) << 8) | (rx_buffer.at(12) << 0));
      int16_t angular     = (int16_t)((rx_buffer.at(15) << 8) | (rx_buffer.at(14) << 0));
      int16_t left_speed  = (int16_t)((rx_buffer.at(17) << 8) | (rx_buffer.at(16) << 0));
      int16_t right_speed = (int16_t)((rx_buffer.at(19) << 8) | (rx_buffer.at(18) << 0));

      ctrl.msg.state.ctrl_mode           = ctrl_mode;
      ctrl.msg.state.battery_percent     = percent;
      ctrl.msg.state.battery_voltage     = ((float)voltage) * 0.1f;
      ctrl.msg.state.stop_button         = (state >> 0) & 0x01;
      ctrl.msg.state.remote_ctrl_stop    = (state >> 1) & 0x01;
      ctrl.msg.state.software_stop       = (state >> 2) & 0x01;
      ctrl.msg.state.remote_ctrl_offline = (state >> 3) & 0x01;
      ctrl.msg.state.front_collision     = (state >> 4) & 0x01;
      ctrl.msg.state.back_collision      = (state >> 5) & 0x01;
      ctrl.msg.state.charge_enable       = (state >> 6) & 0x01;
      ctrl.msg.state.drive_offline       = (err >> 0) & 0x01;
      ctrl.msg.state.drive_error         = (err >> 1) & 0x01;
      ctrl.msg.state.linear_velocity     = ((float)linear) * 0.001f;
      ctrl.msg.state.angular_velocity    = ((float)angular) * 0.001f;
      ctrl.msg.state.left_speed          = left_speed;
      ctrl.msg.state.right_speed         = right_speed;

      ctrl.pub.state.publish(ctrl.msg.state);

      pkgOdomCalculation(ctrl.msg.state.linear_velocity, ctrl.msg.state.angular_velocity);

      ctrl.pkg.state_upload.setResponse();
      ctrl.pkg.get_state.setResponse();

      pkgInfoDisplay(100, "get state success.");

      break;
    }
    case ReceiveCmd::current:
    {
      ctrl.msg.buff.cmd_81_current.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_81_current.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      int16_t left_current  = (int16_t)((rx_buffer.at(5) << 8) | (rx_buffer.at(4) << 0));
      int16_t right_current = (int16_t)((rx_buffer.at(7) << 8) | (rx_buffer.at(6) << 0));

      ctrl.msg.current.left  = ((float)left_current) * 0.1f;
      ctrl.msg.current.right = ((float)right_current) * 0.1f;

      ctrl.pub.current.publish(ctrl.msg.current);

      ctrl.pkg.current_upload.setResponse();
      ctrl.pkg.get_current.setResponse();

      pkgInfoDisplay(100, "get current success.");

      break;
    }
    case ReceiveCmd::charge:
    {
      ctrl.msg.buff.cmd_82_charge.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_82_charge.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      uint8_t state    = (uint8_t)rx_buffer.at(4);
      uint8_t relays   = (uint8_t)!!rx_buffer.at(5);
      uint8_t button   = (uint8_t)!!rx_buffer.at(6);
      uint8_t infrared = (uint8_t)rx_buffer.at(7);
      uint8_t laser    = (uint8_t)rx_buffer.at(8);

      ctrl.msg.charge.state    = state;
      ctrl.msg.charge.relays   = relays;
      ctrl.msg.charge.button   = button;
      ctrl.msg.charge.infrared = infrared;
      ctrl.msg.charge.laser    = laser;

      ctrl.pub.charge.publish(ctrl.msg.charge);

      ctrl.pkg.charge_upload.setResponse();
      ctrl.pkg.get_charge.setResponse();

      pkgInfoDisplay(20, "get charge success.");

      break;
    }
    case ReceiveCmd::remote_ctrl:
    {
      ctrl.msg.buff.cmd_83_remote_ctrl.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_83_remote_ctrl.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      int16_t right_x     = (int16_t)((rx_buffer.at(5) << 8) | (rx_buffer.at(4) << 0));
      int16_t right_y     = (int16_t)((rx_buffer.at(7) << 8) | (rx_buffer.at(6) << 0));
      int16_t left_y      = (int16_t)((rx_buffer.at(9) << 8) | (rx_buffer.at(8) << 0));
      int16_t left_x      = (int16_t)((rx_buffer.at(11) << 8) | (rx_buffer.at(10) << 0));
      int16_t left_round  = (int16_t)((rx_buffer.at(13) << 8) | (rx_buffer.at(12) << 0));
      int16_t right_round = (int16_t)((rx_buffer.at(15) << 8) | (rx_buffer.at(14) << 0));
      uint8_t key         = (uint8_t)rx_buffer.at(16);
      uint8_t state       = (uint8_t)rx_buffer.at(17);

      ctrl.msg.remote_ctrl.right_rocker_x  = right_x;
      ctrl.msg.remote_ctrl.right_rocker_y  = right_y;
      ctrl.msg.remote_ctrl.left_rocker_y   = left_y;
      ctrl.msg.remote_ctrl.left_rocker_x   = left_x;
      ctrl.msg.remote_ctrl.left_round_vra  = left_round;
      ctrl.msg.remote_ctrl.right_round_vrb = right_round;

      ctrl.msg.remote_ctrl.key_swa = (key >> (0 * 2)) & B_0000_0011;
      ctrl.msg.remote_ctrl.key_swb = (key >> (1 * 2)) & B_0000_0011;
      ctrl.msg.remote_ctrl.key_swc = (key >> (2 * 2)) & B_0000_0011;
      ctrl.msg.remote_ctrl.key_swd = (key >> (3 * 2)) & B_0000_0011;

      ctrl.msg.remote_ctrl.is_offline = state & 0x01;

      ctrl.pub.remote_ctrl.publish(ctrl.msg.remote_ctrl);

      ctrl.pkg.remote_ctrl_upload.setResponse();
      ctrl.pkg.get_remote_ctrl.setResponse();

      pkgInfoDisplay(40, "get remote ctrl success.");

      break;
    }
    case ReceiveCmd::drive_error:
    {
      ctrl.msg.buff.cmd_A0_drvie_error.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_A0_drvie_error.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      uint16_t left_error  = (uint16_t)((rx_buffer.at(5) << 8) | (rx_buffer.at(4) << 0));
      uint16_t right_error = (uint16_t)((rx_buffer.at(7) << 8) | (rx_buffer.at(6) << 0));

      memset(&ctrl.msg.drive_error.left, 0, sizeof(ctrl.msg.drive_error.left));
      memset(&ctrl.msg.drive_error.right, 0, sizeof(ctrl.msg.drive_error.right));

      switch(left_error)
      {
        case 0x0001: {ctrl.msg.drive_error.left.encoder_abz_fault_alarm = 1;                              break;}
        case 0x0002: {ctrl.msg.drive_error.left.encoder_uvw_fault_alarm = 1;                              break;}
        case 0x0003: {ctrl.msg.drive_error.left.position_error = 1;                                       break;}
        case 0x0004: {ctrl.msg.drive_error.left.stall = 1;                                                break;}
        case 0x0005: {ctrl.msg.drive_error.left.current_sampling_midpoint_fault = 1;                      break;}
        case 0x0006: {ctrl.msg.drive_error.left.overload = 1;                                             break;}
        case 0x0007: {ctrl.msg.drive_error.left.undervoltage = 1;                                         break;}
        case 0x0008: {ctrl.msg.drive_error.left.overvoltage = 1;                                          break;}
        case 0x0009: {ctrl.msg.drive_error.left.overcurrent = 1;                                          break;}
        case 0x000A: {ctrl.msg.drive_error.left.discharge_alarm_instant_power_high = 1;                   break;}
        case 0x000B: {ctrl.msg.drive_error.left.discharge_circuit_frequent_action_average_power_high = 1; break;}
        case 0x000C: {ctrl.msg.drive_error.left.parameter_read_write_error = 1;                           break;}
        case 0x000D: {ctrl.msg.drive_error.left.input_function_duplicate_definition = 1;                  break;}
        case 0x000E: {ctrl.msg.drive_error.left.communication_watchdog_triggered = 1;                     break;}
        case 0x000F: {ctrl.msg.drive_error.left.motor_overtemperature_alarm = 1;                          break;}
        default: {break;}
      }

      switch(right_error)
      {
        case 0x0001: {ctrl.msg.drive_error.right.encoder_abz_fault_alarm = 1;                              break;}
        case 0x0002: {ctrl.msg.drive_error.right.encoder_uvw_fault_alarm = 1;                              break;}
        case 0x0003: {ctrl.msg.drive_error.right.position_error = 1;                                       break;}
        case 0x0004: {ctrl.msg.drive_error.right.stall = 1;                                                break;}
        case 0x0005: {ctrl.msg.drive_error.right.current_sampling_midpoint_fault = 1;                      break;}
        case 0x0006: {ctrl.msg.drive_error.right.overload = 1;                                             break;}
        case 0x0007: {ctrl.msg.drive_error.right.undervoltage = 1;                                         break;}
        case 0x0008: {ctrl.msg.drive_error.right.overvoltage = 1;                                          break;}
        case 0x0009: {ctrl.msg.drive_error.right.overcurrent = 1;                                          break;}
        case 0x000A: {ctrl.msg.drive_error.right.discharge_alarm_instant_power_high = 1;                   break;}
        case 0x000B: {ctrl.msg.drive_error.right.discharge_circuit_frequent_action_average_power_high = 1; break;}
        case 0x000C: {ctrl.msg.drive_error.right.parameter_read_write_error = 1;                           break;}
        case 0x000D: {ctrl.msg.drive_error.right.input_function_duplicate_definition = 1;                  break;}
        case 0x000E: {ctrl.msg.drive_error.right.communication_watchdog_triggered = 1;                     break;}
        case 0x000F: {ctrl.msg.drive_error.right.motor_overtemperature_alarm = 1;                          break;}
        default: {break;}
      }

      ctrl.pub.drive_error.publish(ctrl.msg.drive_error);

      ctrl.pkg.get_drive_error.setResponse();

      pkgInfoDisplay(10, "get drive error info success.");

      break;
    }
    case ReceiveCmd::parameter:
    {
      ctrl.msg.buff.cmd_A1_parameter.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_A1_parameter.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      uint16_t track_width    = (uint16_t)((rx_buffer.at(5) << 8) | (rx_buffer.at(4) << 0));
      uint16_t wheel_base     = (uint16_t)((rx_buffer.at(7) << 8) | (rx_buffer.at(6) << 0));
      uint16_t wheel_diameter = (uint16_t)((rx_buffer.at(9) << 8) | (rx_buffer.at(8) << 0));
      uint16_t gear_ratio     = (uint16_t)((rx_buffer.at(11) << 8) | (rx_buffer.at(10) << 0));
      uint16_t encoder_line   = (uint16_t)((rx_buffer.at(13) << 8) | (rx_buffer.at(12) << 0));

      ctrl.msg.parameter.track_width    = track_width;
      ctrl.msg.parameter.wheel_base     = wheel_base;
      ctrl.msg.parameter.wheel_diameter = wheel_diameter;
      ctrl.msg.parameter.gear_ratio     = gear_ratio;
      ctrl.msg.parameter.encoder_line   = encoder_line;

      ctrl.pub.parameter.publish(ctrl.msg.parameter);

      ctrl.pkg.get_parameter.setResponse();

      pkgInfoDisplay(1, "get parameter success.");

      break;
    }
    case ReceiveCmd::software:
    {
      ctrl.msg.buff.cmd_A2_software_version.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_A2_software_version.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      uint8_t major_version    = (uint8_t)rx_buffer.at(4);
      uint8_t minor_version    = (uint8_t)rx_buffer.at(5);
      uint8_t revision_version = (uint8_t)rx_buffer.at(6);
      uint8_t other_version    = (uint8_t)rx_buffer.at(7);
      uint8_t year_version     = (uint8_t)rx_buffer.at(8);
      uint8_t month_version    = (uint8_t)rx_buffer.at(9);
      uint8_t day_version      = (uint8_t)rx_buffer.at(10);
      uint8_t seq_version      = (uint8_t)rx_buffer.at(11);

      ctrl.msg.software_version.version.at(0) = ctrl.msg.software_version.major_version    = major_version;
      ctrl.msg.software_version.version.at(1) = ctrl.msg.software_version.minor_version    = minor_version;
      ctrl.msg.software_version.version.at(2) = ctrl.msg.software_version.revision_version = revision_version;
      ctrl.msg.software_version.version.at(3) = ctrl.msg.software_version.other_version    = other_version;
      ctrl.msg.software_version.version.at(4) = ctrl.msg.software_version.year_version     = year_version;
      ctrl.msg.software_version.version.at(5) = ctrl.msg.software_version.month_version    = month_version;
      ctrl.msg.software_version.version.at(6) = ctrl.msg.software_version.day_version      = day_version;
      ctrl.msg.software_version.version.at(7) = ctrl.msg.software_version.seq_version      = seq_version;

      ctrl.pub.software_version.publish(ctrl.msg.software_version);

      ctrl.pkg.get_software_version.setResponse();

      pkgInfoDisplay(1, "get software version success.");

      break;
    }
    case ReceiveCmd::hardware:
    {
      ctrl.msg.buff.cmd_A3_hardware_version.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_A3_hardware_version.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      ctrl.msg.hardware_version.version.at(0) = (uint8_t)rx_buffer.at(4);
      ctrl.msg.hardware_version.version.at(1) = (uint8_t)rx_buffer.at(5);
      ctrl.msg.hardware_version.version.at(2) = (uint8_t)rx_buffer.at(6);
      ctrl.msg.hardware_version.version.at(3) = (uint8_t)rx_buffer.at(7);
      ctrl.msg.hardware_version.version.at(4) = (uint8_t)rx_buffer.at(8);
      ctrl.msg.hardware_version.version.at(5) = (uint8_t)rx_buffer.at(9);
      ctrl.msg.hardware_version.version.at(6) = (uint8_t)rx_buffer.at(10);
      ctrl.msg.hardware_version.version.at(7) = (uint8_t)rx_buffer.at(11);

      ctrl.pub.hardware_version.publish(ctrl.msg.hardware_version);

      ctrl.pkg.get_hardware_version.setResponse();

      pkgInfoDisplay(1, "get hardware version success.");

      break;
    }
    case ReceiveCmd::date:
    {
      ctrl.msg.buff.cmd_A4_date.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {ctrl.msg.buff.cmd_A4_date.push_back(rx_buffer.at(loop));}
      ctrl.pub.buff.publish(ctrl.msg.buff);

      ctrl.msg.date.year  = rx_buffer.at(4);
      ctrl.msg.date.month = rx_buffer.at(5);
      ctrl.msg.date.day   = rx_buffer.at(6);

      ctrl.pub.date.publish(ctrl.msg.date);

      ctrl.pkg.get_date.setResponse();

      pkgInfoDisplay(1, "get date success.");

      break;
    }
    default:
    {
      break;
    }
  }

}

void pkgOdomCalculation(double linear, double radian)
{
  if(ctrl.param.odom == false) {return;}

  ctrl.odom.value.header.stamp = ros::Time::now();
  ctrl.odom.value.header.frame_id = "odom";
  ctrl.odom.value.child_frame_id = "base_link";

  double vx = linear;
  double vy = 0;
  double vyaw = radian;
  double dt = (ros::Time::now() - ctrl.odom.last_time).toSec();
  ctrl.odom.last_time = ros::Time::now();

  double dx = (vx * cos(ctrl.odom.yaw) - vy * sin(ctrl.odom.yaw)) * dt;
  double dy = (vx * sin(ctrl.odom.yaw) + vy * cos(ctrl.odom.yaw)) * dt;
  double dyaw = vyaw * dt;

  ctrl.odom.x += dx;
  ctrl.odom.y += dy;
  ctrl.odom.yaw += dyaw;

  geometry_msgs::Quaternion q = tf::createQuaternionMsgFromYaw(ctrl.odom.yaw);

  ctrl.odom.value.pose.pose.position.x = ctrl.odom.x;
  ctrl.odom.value.pose.pose.position.y = ctrl.odom.y;
  ctrl.odom.value.pose.pose.position.z = 0;
  ctrl.odom.value.pose.pose.orientation = q;
  ctrl.odom.value.twist.twist.linear.x = vx;
  ctrl.odom.value.twist.twist.angular.z = vyaw;
  ctrl.odom.value.pose.covariance = cov_array;
  ctrl.odom.value.twist.covariance = cov_array;

  ctrl.pub.odom.publish(ctrl.odom.value);
}

void pkgFormatFrame(TransmitCmd cmd, size_t data_len, uint8_t *data_buffe, std::vector<uint8_t> &tx_buffer)
{
  size_t loop = 0;
  uint16_t sum_value = 0;

  tx_buffer.clear();

  tx_buffer.push_back(0xED);
  tx_buffer.push_back(0xDE);
  tx_buffer.push_back(6 + data_len);
  tx_buffer.push_back((uint8_t)cmd);

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

  #if 0
  printf("tx frame: ");
  for(loop = 0; loop < tx_buffer.size(); loop++)
  {printf("%02X ", tx_buffer.at(loop));}
  printf("\n");
  #endif
}

void pkgSetStateUpload(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::state_upload, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetCurrentUpload(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::current_upload, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetChargeUpload(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::charge_upload, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetRemoteCtrlUpload(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::remote_ctrl_upload, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetVelocity(double linear, double radian)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[4] = {0};

  int16_t *linear_ptr = nullptr;
  int16_t *radian_ptr = nullptr;

  linear_ptr = (int16_t *)&data_buffer[0];
  radian_ptr = (int16_t *)&data_buffer[2];

  *linear_ptr = (int16_t)(linear * 1000);
  *radian_ptr = (int16_t)(radian * 1000);

  pkgFormatFrame(TransmitCmd::set_velocity, 4, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetSpeed(int16_t left_speed, int16_t right_speed)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[4] = {0};

  int16_t *left_speed_ptr = nullptr;
  int16_t *right_speed_ptr = nullptr;

  left_speed_ptr = (int16_t *)&data_buffer[0];
  right_speed_ptr = (int16_t *)&data_buffer[2];

  *left_speed_ptr = left_speed;
  *right_speed_ptr = right_speed;

  pkgFormatFrame(TransmitCmd::set_velocity, 4, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetStop(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::set_stop, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgCollisionClean(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  if(enable == false) {return;}

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::collision_clean, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgFaultClean(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  if(enable == false) {return;}

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::fault_clean, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetCharge(uint8_t charge_type)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  uint8_t *enable_ptr = nullptr;

  if((charge_type != 0x00) && (charge_type != 0x01) && (charge_type != 0x02)) {return;}

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = charge_type;

  pkgFormatFrame(TransmitCmd::set_charge, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetState(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_state, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetCurrent(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_current, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetCharge(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_charge, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetRemoteCtrl(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_remote_ctrl, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetDriveError(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_drive_error, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetParameter(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_parameter, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetSoftwareVersion(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_software, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetHardwareVersion(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_hardware, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetDate(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[1] = {0};

  pkgFormatFrame(TransmitCmd::get_date, 1, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgLoopInit(void)
{
  ctrl.pkg.step = 0;

  /* get once */

  ctrl.pkg.get_software_version.setOnce(true);
  ctrl.pkg.get_software_version.setTimeout(3000);
  ctrl.pkg.get_software_version.flagClean();

  ctrl.pkg.get_hardware_version.setOnce(true);
  ctrl.pkg.get_hardware_version.setTimeout(3000);
  ctrl.pkg.get_hardware_version.flagClean();

  ctrl.pkg.get_parameter.setOnce(true);
  ctrl.pkg.get_parameter.setTimeout(3000);
  ctrl.pkg.get_parameter.flagClean();

  ctrl.pkg.get_date.setOnce(true);
  ctrl.pkg.get_date.setTimeout(3000);
  ctrl.pkg.get_date.flagClean();

  /* upload */

  ctrl.pkg.state_upload.setOnce(true);
  ctrl.pkg.state_upload.setTimeout(1000);
  ctrl.pkg.state_upload.flagClean();

  ctrl.pkg.charge_upload.setOnce(true);
  ctrl.pkg.charge_upload.setTimeout(1000);
  ctrl.pkg.charge_upload.flagClean();

  ctrl.pkg.remote_ctrl_upload.setOnce(true);
  ctrl.pkg.remote_ctrl_upload.setTimeout(1000);
  ctrl.pkg.remote_ctrl_upload.flagClean();

  /* loop get */

  ctrl.pkg.get_drive_error.setOnce(false);
  ctrl.pkg.get_drive_error.setTimeout(100);
  ctrl.pkg.get_drive_error.flagClean();

  /* not use */

  ctrl.pkg.current_upload.setOnce(true);
  ctrl.pkg.current_upload.setTimeout(1000);
  ctrl.pkg.current_upload.flagClean();

  ctrl.pkg.get_state.setOnce(false);
  ctrl.pkg.get_state.setTimeout(1000);
  ctrl.pkg.get_state.flagClean();

  ctrl.pkg.get_current.setOnce(false);
  ctrl.pkg.get_current.setTimeout(1000);
  ctrl.pkg.get_current.flagClean();

  ctrl.pkg.get_charge.setOnce(false);
  ctrl.pkg.get_charge.setTimeout(1000);
  ctrl.pkg.get_charge.flagClean();

  ctrl.pkg.get_remote_ctrl.setOnce(false);
  ctrl.pkg.get_remote_ctrl.setTimeout(1000);
  ctrl.pkg.get_remote_ctrl.flagClean();

}

void pkgLoopReset(void)
{
  ctrl.pkg.step = 0;

  ctrl.pkg.get_software_version.flagClean();
  ctrl.pkg.get_hardware_version.flagClean();
  ctrl.pkg.get_parameter.flagClean();
  ctrl.pkg.get_date.flagClean();

  ctrl.pkg.state_upload.flagClean();
  ctrl.pkg.charge_upload.flagClean();
  ctrl.pkg.remote_ctrl_upload.flagClean();

  ctrl.pkg.get_drive_error.flagClean();

  ctrl.pkg.current_upload.flagClean();
  ctrl.pkg.get_state.flagClean();
  ctrl.pkg.get_current.flagClean();
  ctrl.pkg.get_charge.flagClean();
  ctrl.pkg.get_remote_ctrl.flagClean();
}

void pkgGetLoop(void)
{
  if(ctrl.step.finish() != true) {return;}

  DtPkgGetCode code = DtPkgGetCode::wait;

  for(size_t step_loop = 0; step_loop < ctrl.pkg.max_num; step_loop++)
  {
    /* get once */

    if(ctrl.pkg.step == 0)
    {
      code = pkgGetCheck(ctrl.pkg.get_software_version, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetSoftwareVersion();}
      if(code == DtPkgGetCode::send)    {logs_info("try get software version."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get software version timeout."); break;}
    }
    else if(ctrl.pkg.step == 1)
    {
      code = pkgGetCheck(ctrl.pkg.get_hardware_version, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetHardwareVersion();}
      if(code == DtPkgGetCode::send)    {logs_info("try get hardware version."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get hardware version timeout."); break;}
    }
    else if(ctrl.pkg.step == 2)
    {
      code = pkgGetCheck(ctrl.pkg.get_parameter, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetParameter();}
      if(code == DtPkgGetCode::send)    {logs_info("try get parameter."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get parameter timeout."); break;}
    }
    else if(ctrl.pkg.step == 3)
    {
      code = pkgGetCheck(ctrl.pkg.get_date, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetDate();}
      if(code == DtPkgGetCode::send)    {logs_info("try get date."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get date timeout."); break;}
    }

    /* upload */

    else if(ctrl.pkg.step == 4)
    {
      code = pkgGetCheck(ctrl.pkg.state_upload, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgSetStateUpload(true);}
      if(code == DtPkgGetCode::send)    {logs_info("try open state upload."); break;}
      if(code == DtPkgGetCode::get)     {logs_info("open state upload success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("open state upload timeout."); break;}
    }
    else if(ctrl.pkg.step == 5)
    {
      code = pkgGetCheck(ctrl.pkg.charge_upload, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgSetChargeUpload(true);}
      if(code == DtPkgGetCode::send)    {logs_info("try open charge upload."); break;}
      if(code == DtPkgGetCode::get)     {logs_info("open charge upload success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("open charge upload timeout."); break;}
    }
    else if(ctrl.pkg.step == 6)
    {
      code = pkgGetCheck(ctrl.pkg.remote_ctrl_upload, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgSetRemoteCtrlUpload(true);}
      if(code == DtPkgGetCode::send)    {logs_info("try open remote ctrl upload."); break;}
      if(code == DtPkgGetCode::get)     {logs_info("open remote ctrl upload success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("open remote ctrl upload timeout."); break;}
    }

    /* loop get */

    else if(ctrl.pkg.step == 7)
    {
      code = pkgGetCheck(ctrl.pkg.get_drive_error, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetDriveError();}
      if(code == DtPkgGetCode::send)    {/* logs_info("try get drive error info."); */ break;}
      // if(code == DtPkgGetCode::get)     {logs_info("get drive error info success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get drive error info timeout."); break;}
    }

    /* not use */

    else if(ctrl.pkg.step == ((size_t)-1))
    {
      code = pkgGetCheck(ctrl.pkg.current_upload, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgSetCurrentUpload(true);}
      if(code == DtPkgGetCode::send)    {logs_info("try open current upload."); break;}
      if(code == DtPkgGetCode::get)     {logs_info("open current upload success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("open current upload timeout."); break;}
    }
    else if(ctrl.pkg.step == ((size_t)-1))
    {
      code = pkgGetCheck(ctrl.pkg.get_current, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetCurrent();}
      if(code == DtPkgGetCode::send)    {/* logs_info("try get current info."); */ break;}
      // if(code == DtPkgGetCode::get)     {logs_info("get current info success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get current info timeout."); break;}
    }
    else if(ctrl.pkg.step == ((size_t)-1))
    {
      code = pkgGetCheck(ctrl.pkg.get_charge, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetCharge();}
      if(code == DtPkgGetCode::send)    {/* logs_info("try get charge info."); */ break;}
      // if(code == DtPkgGetCode::get)     {logs_info("get charge info success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get charge info timeout."); break;}
    }
    else if(ctrl.pkg.step == ((size_t)-1))
    {
      code = pkgGetCheck(ctrl.pkg.get_remote_ctrl, ctrl.pkg.step);
      if(code == DtPkgGetCode::send)    {pkgGetRemoteCtrl();}
      if(code == DtPkgGetCode::send)    {/* logs_info("try get remote ctrl info."); */ break;}
      // if(code == DtPkgGetCode::get)     {logs_info("get remote ctrl info success."); break;}
      if(code == DtPkgGetCode::timeout) {logs_error("get remote ctrl info timeout."); break;}
    }

    if(code == DtPkgGetCode::get) {break;}
    if(code == DtPkgGetCode::wait) {break;}
    if(code == DtPkgGetCode::skip) {continue;}
  }

  if(ctrl.pkg.step == ctrl.pkg.max_num)
  {
    ctrl.pkg.step = 0;
  }
}

DtPkgGetCode pkgGetCheck(DtPkgGet &pkg, uint32_t &step)
{
  if((pkg.do_once == true) && (pkg.flag.once == true))
  {
    step++;
    return DtPkgGetCode::skip;
  }

  if(pkg.flag.request == false)
  {
    pkg.flag.request = true;
    pkg.flag.response = false;
    pkg.flag.timeout_cnt = 0;
    return DtPkgGetCode::send;
  }

  if(pkg.flag.request == true)
  {
    pkg.flag.timeout_cnt++;

    if(pkg.flag.response == true)
    {
      step++;
      pkg.flag.once = true;
      pkg.flag.request = false;
      pkg.flag.response = false;
      pkg.flag.timeout_cnt = 0;
      return DtPkgGetCode::get;
    }

    if(pkg.flag.timeout_cnt > ((pkg.timeout_ms < ctrl.pkg.cycle) ? 1 : (pkg.timeout_ms / ctrl.pkg.cycle)))
    {
      step++;
      pkg.flag.request = false;
      pkg.flag.response = false;
      pkg.flag.timeout_cnt = 0;
      return DtPkgGetCode::timeout;
    }

    if(pkg.flag.response == false)
    {
      return DtPkgGetCode::wait;
    }
  }

  return DtPkgGetCode::wait;
}
