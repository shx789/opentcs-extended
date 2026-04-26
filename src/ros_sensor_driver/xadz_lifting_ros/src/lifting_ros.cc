
#include <signal.h>
#include <ros/ros.h>

#include "xadz_lifting_ros/lifting_ros_pkg.h"

Ctrl ctrl;

int main(int argc, char *argv[])
{
  ros::init(argc, argv, "xadz_lifting_ros_node");
  /* ros::init(argc, argv, "xadz_lifting_ros_node", ros::init_options::NoSigintHandler); */

  ros::NodeHandle nh;
  ros::NodeHandle n("~");

  n.param<std::string>("lifting_port", ctrl.param.port, "/dev/ttyUSB0");
  n.param<int>("lifting_baudrate", ctrl.param.baudrate, 115200);
  n.param<bool>("lifting_log_display", ctrl.param.logs, true);
  n.param<bool>("lifting_original_display", ctrl.param.original, false);

  ctrl.sub.velocity = nh.subscribe<std_msgs::Int8>("/lifting/velocity_mode_ctrl", 100,
  boost::function<void (const std_msgs::Int8::ConstPtr &)>(
  [&](const std_msgs::Int8::ConstPtr &msg)
  {
    pkgSetVelocity(msg->data);
  }));

  ctrl.sub.position = nh.subscribe<std_msgs::Int16MultiArray>("/lifting/position_mode_ctrl", 100,
  boost::function<void (const std_msgs::Int16MultiArray::ConstPtr &)>(
  [&](const std_msgs::Int16MultiArray::ConstPtr &msg)
  {
    if(msg->data.size() < 2) {return;}
    pkgSetPosition(static_cast<uint8_t>(msg->data.at(0)), msg->data.at(1));
  }));

  ctrl.sub.zero = nh.subscribe<std_msgs::Empty>("/lifting/position_zero_set", 100,
  boost::function<void (const std_msgs::Empty::ConstPtr &)>(
  [&](const std_msgs::Empty::ConstPtr)
  {
    pkgSetZero();
  }));

  ctrl.pub.mode            = nh.advertise<std_msgs::UInt8>("/lifting/mode_info", 100);
  ctrl.pub.limit_up        = nh.advertise<std_msgs::Bool>("/lifting/limit_up_info", 100);
  ctrl.pub.limit_down      = nh.advertise<std_msgs::Bool>("/lifting/limit_down_info", 100);
  ctrl.pub.drive_error     = nh.advertise<std_msgs::Bool>("/lifting/motor_drive_error_info", 100);
  ctrl.pub.encoder_online  = nh.advertise<std_msgs::Bool>("/lifting/abs_encoder_online_info", 100);
  ctrl.pub.position_action = nh.advertise<std_msgs::UInt8>("/lifting/position_action_info", 100);
  ctrl.pub.position_target = nh.advertise<std_msgs::Int16>("/lifting/position_target_info", 100);
  ctrl.pub.position_real   = nh.advertise<std_msgs::Int16>("/lifting/position_real_info", 100);
  ctrl.pub.velocity_target = nh.advertise<std_msgs::Int8>("/lifting/velocity_target_info", 100);
  ctrl.pub.velocity_real   = nh.advertise<std_msgs::Int8>("/lifting/velocity_real_info", 100);
  ctrl.pub.frame           = nh.advertise<std_msgs::UInt8MultiArray>("/lifting/frame", 100);

  ros::Timer tim_serial_init = nh.createTimer(ros::Duration(1 /* s, double */),
  boost::function<void (const ros::TimerEvent &)>(
  [&](const ros::TimerEvent &event)
  {
    serialInit();
  }), false, true);

  ros::Timer tim_serial_read = nh.createTimer(ros::Duration(0.001 /* s, double */),
  boost::function<void (const ros::TimerEvent &)>(
  [&](const ros::TimerEvent &event)
  {
    serialRead();
  }), false, true);

  ros::Timer tim_serial_write = nh.createTimer(ros::Duration(0.001 /* s, double */),
  boost::function<void (const ros::TimerEvent &)>(
  [&](const ros::TimerEvent &event)
  {
    serialWrite();
  }), false, true);

  ros::Timer tim_pkg_receive = nh.createTimer(ros::Duration(0.001f /* s, double */),
  boost::function<void (const ros::TimerEvent &)>(
  [&](const ros::TimerEvent &event)
  {
    pkgReceive();
  }), false, true);

  ros::Timer tim_pkg_loop = nh.createTimer(ros::Duration(((float)ctrl.pkg.cycle) * 0.001f /* s, double */),
  boost::function<void (const ros::TimerEvent &)>(
  [&](const ros::TimerEvent &event)
  {
    pkgGetLoop();
  }), false, true);

  pkgLoopInit();

  /* signal(SIGINT, node_sigint_handle); */

  ros::Rate rate(cycle_ms_to_freq(1));

  while(ros::ok())
  {
    // do sth

    /* ros::spin(); */
    ros::spinOnce();

    rate.sleep();
  }

  exit(0);
}

void node_sigint_handle(int sig)
{
  ros::shutdown();
}
