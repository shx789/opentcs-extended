
#include <stdint.h>
#include <signal.h>
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>

#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <std_msgs/UInt8.h>

#include "nav_msgs/Odometry.h"
#include "tf/tf.h"

#include "agv_msgs/agv_drive.h"
#include "agv_msgs/agv_error.h"
#include "agv_msgs/agv_param.h"
#include "agv_msgs/agv_dt.h"
#include "agv_msgs/agv.h"

#include "agv_time_interval.h"
#include "agv_serial_ports.h"
#include "agv_packages.h"
#include "agv.h"

#define cycleMsToFreq(cycle)    (1.0f / (cycle * 0.001f))

using SerialPort = serial_ports::SerialPort;
using Packages = packages::Packages;

using TimeInterval = time_interval::TimeInterval;
using TimeIntervalType = time_interval::TimeType;

using AgvDriveMsg = agv_msgs::agv_drive;
using AgvErrorMsg = agv_msgs::agv_error;
using AgvParamMsg = agv_msgs::agv_param;
using AgvDtMsg    = agv_msgs::agv_dt;
using AgvMsg      = agv_msgs::agv;

SerialPort *port_ptr = nullptr;
Packages *pkg_ptr = nullptr;

bool info_display = false;
int info_display_time = 2;

bool logs_display = true;

void agv_control_callback(const geometry_msgs::Twist::ConstPtr &twist_aux)
{
  pkg_ptr->setVelocity(twist_aux->linear.x, twist_aux->angular.z);

  #if 0
  left_speed = (twist_aux->linear.x * 1000) - ((twist_aux->angular.z * pkg_ptr->getTrackWidth()) / 2.0);
  right_speed = (twist_aux->linear.x * 1000) + ((twist_aux->angular.z * pkg_ptr->getTrackWidth()) / 2.0);
  #endif
}

void agv_charge_callback(const std_msgs::UInt8 charge_type)
{
  pkg_ptr->setCharge(charge_type.data);
}

void agv_fault_clean_callback(const std_msgs::UInt8 clean_type)
{
  pkg_ptr->faultClean(clean_type.data);
}

void agv_odom_clean_callback(const std_msgs::UInt8::ConstPtr &clean)
{
  pkg_ptr->odomClean();
}

void agv_stop_callback(const std_msgs::UInt8 enable)
{
  pkg_ptr->setStop(enable.data);
}

void agv_info_display(AgvMsg &agv_pkg, AgvParamMsg &agv_param_pkg)
{
  static bool display_once = false;

  if(display_once == false)
  {
    display_once = true;

    printf("\n");

    printf("track width   : %d mm \n", agv_param_pkg.track_width);
    printf("wheel base    : %d mm \n", agv_param_pkg.wheel_base);
    printf("wheel diameter: %d mm \n", agv_param_pkg.wheel_diameter);
    printf("gear ratio    : %d \n", agv_param_pkg.gear_ratio);
    printf("encoder line  : %d \n", agv_param_pkg.encoder_line);

    printf("\n");

  }

  printf("\n");

  printf("linear velocity : %f m/s \n", agv_pkg.linear_velocity);
  printf("angular velocity: %f rad/s \n", agv_pkg.angular_velocity);
  printf("voltage         : %f V \n", agv_pkg.voltage);
  printf("stop button     : %s\n", (((bool)agv_pkg.stop_button) == true) ? "push" : "pop");
  printf("left current    : %f A \n", agv_pkg.left_current);
  printf("right current   : %f A \n", agv_pkg.right_current);

  printf("\n");
}

void node_sigint_handle(int sig)
{
  ros::shutdown();
}

int main(int argc, char *argv[])
{
  ros::init(argc, argv, "agv_node");
  /* ros::init(argc, argv, "agv_node", ros::init_options::NoSigintHandler); */

  std::string agv_port;
  int agv_baudrate = 0;
  bool odom_enable = true;

  ros::NodeHandle nh;
  ros::NodeHandle n("~");

  n.param<std::string>("agv_port", agv_port, "/dev/ttyUSB0");
  n.param<int>("agv_baudrate", agv_baudrate, 115200);
  n.param<bool>("agv_odom_enable", odom_enable, true);
  n.param<bool>("agv_state_display", logs_display, true);
  n.param<bool>("agv_info_display", info_display, false);
  n.param<int>("agv_info_display_time", info_display_time, 2);

  /* nh.createTimer(ros::Duration(1), timercallback, false, true); */

  AgvMsg agv_msg;
  AgvErrorMsg agv_error_msg;
  AgvParamMsg agv_param_msg;
  AgvDriveMsg agv_drive_msg;
  AgvDtMsg agv_dt_msg;
  nav_msgs::Odometry agv_odom_msg;

  ros::Subscriber agv_velocity_sub = nh.subscribe("/agv_velocity", 100, agv_control_callback);
  ros::Subscriber agv_odom_clean_sub = nh.subscribe("/agv_odom_clean", 100, agv_odom_clean_callback);
  ros::Subscriber agv_fault_sub = nh.subscribe("/agv_fault_clean", 100, agv_fault_clean_callback);
  ros::Subscriber agv_charge_sub = nh.subscribe("/agv_go_charge", 100, agv_charge_callback);
  ros::Subscriber agv_stop_sub = nh.subscribe("/agv_stop_ctrl", 100, agv_stop_callback);

  ros::Publisher agv_pub = nh.advertise<AgvMsg>("agv_info_dt", 100);
  ros::Publisher agv_error_pub = nh.advertise<AgvErrorMsg>("agv_error_info", 100);
  ros::Publisher agv_param_pub = nh.advertise<AgvParamMsg>("agv_param_info", 100, true);
  ros::Publisher agv_drive_pub = nh.advertise<AgvDriveMsg>("agv_drive_info", 100);
  ros::Publisher agv_dt_pub = nh.advertise<AgvDtMsg>("agv_dt_info", 100);
  ros::Publisher agv_odom_pub = nh.advertise<nav_msgs::Odometry>("agv_odom_info", 100);

  /* signal(SIGINT, node_sigint_handle); */

  ros::Rate rate(cycleMsToFreq(1));

  port_ptr = new SerialPort(agv_port, 1000, agv_baudrate,
                            serial_ports::ByteSize::eight_bits,
                            serial_ports::Parity::parity_none,
                            serial_ports::StopBit::stop_bit_one,
                            serial_ports::FlowCtrl::flow_ctrl_none);

  pkg_ptr = new Packages(port_ptr);

  pkg_ptr->odomEnable(odom_enable);

  pkg_ptr->setPubPtr(&agv_pub, &agv_error_pub, &agv_param_pub, &agv_drive_pub, &agv_dt_pub, &agv_odom_pub);

  TimeInterval dispaly_time;

  if(info_display == true) {dispaly_time.start();}

  while(ros::ok())
  {
    port_ptr->scan();
    pkg_ptr->scan();

    if(info_display == true)
    {
      if(dispaly_time.timeout(TimeIntervalType::seconds, info_display_time) == true)
      {
        dispaly_time.start();

        pkg_ptr->getPackage(agv_msg, agv_error_msg, agv_param_msg, agv_drive_msg, agv_dt_msg, agv_odom_msg);

        agv_info_display(agv_msg, agv_param_msg);
      }
    }

    /* ros::spin(); */
    ros::spinOnce();

    rate.sleep();
  }

  exit(0);
}
