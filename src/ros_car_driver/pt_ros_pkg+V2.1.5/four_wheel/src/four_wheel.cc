
#include <stdint.h>
#include <signal.h>
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>

#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <std_msgs/UInt8.h>

#include "nav_msgs/Odometry.h"
#include "tf/tf.h"

#include "four_wheel_msgs/four_wheel_drive.h"
#include "four_wheel_msgs/four_wheel_error.h"
#include "four_wheel_msgs/four_wheel_param.h"
#include "four_wheel_msgs/four_wheel_pt.h"
#include "four_wheel_msgs/four_wheel.h"

#include "four_wheel_time_interval.h"
#include "four_wheel_serial_ports.h"
#include "four_wheel_packages.h"
#include "four_wheel.h"

#define cycleMsToFreq(cycle)    (1.0f / (cycle * 0.001f))

using SerialPort = serial_ports::SerialPort;
using Packages = packages::Packages;

using TimeInterval = time_interval::TimeInterval;
using TimeIntervalType = time_interval::TimeType;

using FourWheelDriveMsg = four_wheel_msgs::four_wheel_drive;
using FourWheelErrorMsg = four_wheel_msgs::four_wheel_error;
using FourWheelParamMsg = four_wheel_msgs::four_wheel_param;
using FourWheelPtMsg    = four_wheel_msgs::four_wheel_pt;
using FourWheelMsg      = four_wheel_msgs::four_wheel;

SerialPort *port_ptr = nullptr;
Packages *pkg_ptr = nullptr;

bool info_display = false;
int info_display_time = 2;

bool logs_display = true;
bool original_display = false;

void fw_control_callback(const geometry_msgs::Twist::ConstPtr &twist_aux)
{
  pkg_ptr->setVelocity(twist_aux->linear.x, twist_aux->angular.z);

  #if 0
  left_speed = (twist_aux->linear.x * 1000) - ((twist_aux->angular.z * pkg_ptr->getTrackWidth()) / 2.0);
  right_speed = (twist_aux->linear.x * 1000) + ((twist_aux->angular.z * pkg_ptr->getTrackWidth()) / 2.0);
  #endif
}

void fw_charge_callback(const std_msgs::UInt8 enable)
{
  pkg_ptr->setCharge(enable.data);
}

void fw_fault_clean_callback(const std_msgs::UInt8 clean_type)
{
  pkg_ptr->faultClean(clean_type.data);
}

void fw_odom_clean_callback(const std_msgs::UInt8::ConstPtr &clean)
{
  pkg_ptr->odomClean();
}

void fw_stop_callback(const std_msgs::UInt8 enable)
{
  pkg_ptr->setStop(enable.data);
}

void fw_info_display(FourWheelMsg &four_wheel_pkg, FourWheelParamMsg &four_wheel_param_pkg)
{
  static bool display_once = false;

  if(display_once == false)
  {
    display_once = true;

    printf("\n");

    printf("wheel base    : %d mm \n", four_wheel_param_pkg.wheel_base);
    printf("track width   : %d mm \n", four_wheel_param_pkg.track_width);
    printf("wheel diameter: %d mm \n", four_wheel_param_pkg.wheel_diameter);
    printf("gear ratio    : %d \n", four_wheel_param_pkg.gear_ratio);
    printf("encoder line  : %d \n", four_wheel_param_pkg.encoder_line);

    printf("\n");

  }

  printf("\n");

  printf("linear velocity    : %f m/s \n", four_wheel_pkg.linear_velocity);
  printf("angular velocity   : %f rad/s \n", four_wheel_pkg.angular_velocity);
  printf("voltage            : %f V \n", four_wheel_pkg.voltage);
  printf("stop button        : %s\n", (((bool)four_wheel_pkg.stop_button) == true) ? "push" : "pop");
  printf("front left current : %f A \n", four_wheel_pkg.front_left_current);
  printf("front right current: %f A \n", four_wheel_pkg.front_right_current);
  printf("back left current  : %f A \n", four_wheel_pkg.back_left_current);
  printf("back right current : %f A \n", four_wheel_pkg.back_right_current);

  printf("\n");
}

void node_sigint_handle(int sig)
{
  ros::shutdown();
}

int main(int argc, char *argv[])
{
  ros::init(argc, argv, "four_wheel_node");
  /* ros::init(argc, argv, "four_wheel_node", ros::init_options::NoSigintHandler); */

  std::string fw_port;
  int fw_baudrate = 0;
  bool odom_enable = true;

  ros::NodeHandle nh;
  ros::NodeHandle n("~");

  n.param<std::string>("four_wheel_port", fw_port, "/dev/ttyUSB0");
  n.param<int>("four_wheel_baudrate", fw_baudrate, 115200);
  n.param<bool>("four_wheel_odom_enable", odom_enable, true);
  n.param<bool>("four_wheel_state_display", logs_display, true);
  n.param<bool>("four_wheel_original_display", original_display, false);
  n.param<bool>("four_wheel_info_display", info_display, false);
  n.param<int>("four_wheel_info_display_time", info_display_time, 2);

  /* nh.createTimer(ros::Duration(1), timercallback, false, true); */

  FourWheelMsg fw_msg;
  FourWheelErrorMsg fw_error_msg;
  FourWheelParamMsg fw_param_msg;
  FourWheelDriveMsg fwd_msg;
  FourWheelPtMsg fw_pt_msg;
  nav_msgs::Odometry fw_odom_msg;

  ros::Subscriber fw_velocity_sub = nh.subscribe("/cmd_vel", 100, fw_control_callback);
  ros::Subscriber fw_odom_clean_sub = nh.subscribe("/four_wheel_odom_clean", 100, fw_odom_clean_callback);
  ros::Subscriber fw_fault_sub = nh.subscribe("/four_wheel_fault_clean", 100, fw_fault_clean_callback);
  ros::Subscriber fw_charge_sub = nh.subscribe("/four_wheel_go_charge", 100, fw_charge_callback);
  ros::Subscriber fw_stop_sub = nh.subscribe("/four_wheel_stop_ctrl", 100, fw_stop_callback);

  ros::Publisher fw_pub = nh.advertise<FourWheelMsg>("four_wheel_info", 100);
  ros::Publisher fw_error_pub = nh.advertise<FourWheelErrorMsg>("four_wheel_error_info", 100);
  ros::Publisher fw_param_pub = nh.advertise<FourWheelParamMsg>("four_wheel_param_info", 100, true);
  ros::Publisher fwd_pub = nh.advertise<FourWheelDriveMsg>("four_wheel_drive_info", 100);
  ros::Publisher fw_pt_pub = nh.advertise<FourWheelPtMsg>("four_wheel_pt_info", 100);
  ros::Publisher fw_odom_pub = nh.advertise<nav_msgs::Odometry>("four_wheel_odom_info", 100);

  /* signal(SIGINT, node_sigint_handle); */

  ros::Rate rate(cycleMsToFreq(1));

  port_ptr = new SerialPort(fw_port, 1000, fw_baudrate,
                            serial_ports::ByteSize::eight_bits,
                            serial_ports::Parity::parity_none,
                            serial_ports::StopBit::stop_bit_one,
                            serial_ports::FlowCtrl::flow_ctrl_none);

  pkg_ptr = new Packages(port_ptr);

  pkg_ptr->odomEnable(odom_enable);

  pkg_ptr->setPrintOriginal(original_display);

  pkg_ptr->setPubPtr(&fw_pub, &fw_error_pub, &fw_param_pub, &fwd_pub, &fw_pt_pub, &fw_odom_pub);

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

        pkg_ptr->getPackage(fw_msg, fw_error_msg, fw_param_msg, fwd_msg, fw_pt_msg, fw_odom_msg);

        fw_info_display(fw_msg, fw_param_msg);
      }
    }

    /* ros::spin(); */
    ros::spinOnce();

    rate.sleep();
  }

  exit(0);
}
