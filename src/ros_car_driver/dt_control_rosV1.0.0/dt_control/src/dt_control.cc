
#include <stdint.h>
#include <signal.h>
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>

#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <std_msgs/UInt8.h>
#include <std_msgs/Bool.h>

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

#define cycleMsToFreq(cycle)    (1.0f / (cycle * 0.001f))

DtCtrl ctrl;

void dtControlCallback(const geometry_msgs::Twist::ConstPtr &twist_aux);
void dtStopCallback(const std_msgs::Bool enable);
void dtCollisionCleanCallback(const std_msgs::Bool enable);
void dtFaultCleanCallback(const std_msgs::Bool enable);
void dtChargeCallback(const std_msgs::UInt8 charge_type);
void dtOdomCleanCallback(const std_msgs::Bool enable);

void timSerialInitCallback(const ros::TimerEvent&);
void timSerialReadCallback(const ros::TimerEvent&);
void timSerialWriteCallback(const ros::TimerEvent&);
void timPkgLoopCallback(const ros::TimerEvent&);
void timInfoDisplayCallback(const ros::TimerEvent&);

int main(int argc, char *argv[])
{
  ros::init(argc, argv, "dt_control_node");
  /* ros::init(argc, argv, "dt_control_node", ros::init_options::NoSigintHandler); */

  ros::NodeHandle nh;
  ros::NodeHandle n("~");

  n.param<std::string>("dt_port", ctrl.param.port, "/dev/ttyUSB0");
  n.param<int>("dt_baudrate", ctrl.param.baudrate, 115200);
  n.param<bool>("dt_odom_enable", ctrl.param.odom, true);
  n.param<bool>("dt_log_display", ctrl.param.logs, true);
  n.param<bool>("dt_original_display", ctrl.param.original, false);
  n.param<bool>("dt_info_display", ctrl.param.info, false);
  n.param<int>("dt_info_display_time", ctrl.param.info_time, 2);

  ctrl.sub.velocity   = nh.subscribe("/dt/velocity_ctrl",   100, dtControlCallback);
  ctrl.sub.stop       = nh.subscribe("/dt/stop_ctrl",       100, dtStopCallback);
  ctrl.sub.collision  = nh.subscribe("/dt/collision_clean", 100, dtCollisionCleanCallback);
  ctrl.sub.fault      = nh.subscribe("/dt/fault_clean",     100, dtFaultCleanCallback);
  ctrl.sub.charge     = nh.subscribe("/dt/charge_ctrl",     100, dtChargeCallback);
  ctrl.sub.odom_clean = nh.subscribe("/dt/odom_clean",      100, dtOdomCleanCallback);

  ctrl.pub.buff             = nh.advertise<DtBuffMsg>           ("/dt/buff_info", 100);
  ctrl.pub.charge           = nh.advertise<DtChargeMsg>         ("/dt/charge_info", 100);
  ctrl.pub.current          = nh.advertise<DtCurrentMsg>        ("/dt/current_info", 100);
  ctrl.pub.date             = nh.advertise<DtDateMsg>           ("/dt/date_info", 100, true);
  ctrl.pub.drive_error      = nh.advertise<DtDriveErrorMsg>     ("/dt/drive_error_info", 100);
  ctrl.pub.hardware_version = nh.advertise<DtHardwareVersionMsg>("/dt/hardware_version_info", 100, true);
  ctrl.pub.parameter        = nh.advertise<DtParameterMsg>      ("/dt/parameter_info", 100, true);
  ctrl.pub.remote_ctrl      = nh.advertise<DtRemoteCtrlMsg>     ("/dt/remote_ctrl_info", 100);
  ctrl.pub.software_version = nh.advertise<DtSoftwareVersionMsg>("/dt/software_version_info", 100, true);
  ctrl.pub.state            = nh.advertise<DtStateMsg>          ("/dt/state_info", 100);
  ctrl.pub.odom             = nh.advertise<nav_msgs::Odometry>  ("/dt/odom_info", 100);

  /* nh.createTimer(ros::Duration(1), timercallback, false, true); */

  ros::Timer tim_serial_init = nh.createTimer(ros::Duration(1), timSerialInitCallback, false, false);
  ros::Timer tim_serial_read = nh.createTimer(ros::Duration(0.001), timSerialReadCallback, false, false);
  ros::Timer tim_serial_write = nh.createTimer(ros::Duration(0.001), timSerialWriteCallback, false, false);
  ros::Timer tim_pkg_loop = nh.createTimer(ros::Duration(((float)ctrl.pkg.cycle) * 0.001f), timPkgLoopCallback, false, false);
  ros::Timer tim_info_display = nh.createTimer(ros::Duration(ctrl.param.info_time), timInfoDisplayCallback, false, false);

  pkgLoopInit();

  tim_serial_init.start();
  tim_serial_read.start();
  tim_serial_write.start();
  tim_pkg_loop.start();

  if(ctrl.param.info == true) {tim_info_display.start();}

  /* signal(SIGINT, node_sigint_handle); */

  ros::Rate rate(cycleMsToFreq(1));

  while(ros::ok())
  {
    pkgReceive();

    // pkgGetLoop();

    /* ros::spin(); */
    ros::spinOnce();

    rate.sleep();
  }

  exit(0);
}

void dtControlCallback(const geometry_msgs::Twist::ConstPtr &twist_aux)
{
  pkgSetVelocity(twist_aux->linear.x, twist_aux->angular.z);
}

void dtStopCallback(const std_msgs::Bool enable)
{
  pkgSetStop(enable.data);
}

void dtCollisionCleanCallback(const std_msgs::Bool enable)
{
  pkgCollisionClean(enable.data);
}

void dtFaultCleanCallback(const std_msgs::Bool enable)
{
  pkgFaultClean(enable.data);
}

void dtChargeCallback(const std_msgs::UInt8 charge_type)
{
  pkgSetCharge(charge_type.data);
}

void dtOdomCleanCallback(const std_msgs::Bool enable)
{
  ctrl.odom.clean();
}

void timSerialInitCallback(const ros::TimerEvent&)
{
  serialInit();
}

void timSerialReadCallback(const ros::TimerEvent&)
{
  serialRead();
}

void timSerialWriteCallback(const ros::TimerEvent&)
{
  serialWrite();
}

void timPkgLoopCallback(const ros::TimerEvent&)
{
  pkgGetLoop();
}

void timInfoDisplayCallback(const ros::TimerEvent&)
{
  printf("\n");

  // printf("linear velocity : %f m/s \n",   ctrl.msg.velocity.linear_velocity);
  // printf("angular velocity: %f rad/s \n", ctrl.msg.velocity.angular_velocity);
  // printf("voltage         : %f V \n",     ctrl.msg.velocity.voltage);
  // printf("stop button     : %s\n",        (((bool)ctrl.msg.state.stop_button) == true) ? "push" : "pop");
  // printf("left current    : %f A \n",     ctrl.msg.drive_error.left_current);
  // printf("right current   : %f A \n",     ctrl.msg.drive_error.right_current);

  printf("\n");
}

void node_sigint_handle(int sig)
{
  ros::shutdown();
}
