#ifndef __ROS_SCAN_SHADOW_H_
#define __ROS_SCAN_SHADOW_H_
#include <ros/ros.h>  
#include <sensor_msgs/LaserScan.h>
#include <set>
#include <angles/angles.h>


ros::Publisher output_pub_;

 double min_angle_, max_angle_;  // Filter angle threshold
int window_ = 1, neighbors_ = 10;
bool remove_shadow_start_point_ = false;  // Whether to also remove the start point of the shadow
float min_angle_tan_, max_angle_tan_;  // Filter angle thresholds
std::string scan_topic_,out_topic_;

#endif
