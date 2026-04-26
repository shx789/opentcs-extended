#ifndef ROS_HALF_PLANNER_H
#define ROS_HALF_PLANNER_H

#include <ros/ros.h>
#include <mutex>
#include <nav_msgs/Path.h>
#include <geometry_msgs/PoseStamped.h>
#include <ros/ros.h>
#include <sensor_msgs/Range.h>
#include <tf2/utils.h>
#include <tf2_ros/transform_listener.h>
#include <vector>
#include "half_struct_planner.h"
#include <vector>
#include <string.h>
#include<fstream>
#include<iostream>
#include <json/json.h>

#include "tf/transform_datatypes.h"
#include "tf/transform_listener.h"
#include <tf/transform_broadcaster.h>
#include <std_msgs/UInt8.h>
#include "costmap_2d/costmap_2d.h"
#include "costmap_2d/costmap_2d_ros.h"
#include <std_msgs/String.h>

std::shared_ptr<xju::pnc::HalfStructPlanner> half_struct_planner_;
//Json::Value json_track;

std::string path_point_filename;

 ros::Publisher feedback_pub_;
 ros::Publisher path_pub_;

std::shared_ptr<costmap_2d::Costmap2D> costmap_;
uint8_t* cost_translation_ = nullptr;
std::mutex map_update_mutex_;
//listener
tf::TransformListener  *robot_listener = NULL;

 geometry_msgs::PoseStamped    robot_pose,last_robot_pose;
 double pose_th = 0.0;


#endif // ROS_HALF_PLANNER_H
