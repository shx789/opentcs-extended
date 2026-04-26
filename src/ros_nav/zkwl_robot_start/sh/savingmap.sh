#!/bin/bash
rosservice call /finish_trajectory 0
sleep 2
rosservice call /write_state "{filename: '/home/robotcar/catkin_ws/src/ros_nav/zkwl_robot_start/map/map.pbstream'}"
rosrun cartographer_ros cartographer_pbstream_to_ros_map -map_filestem=/home/robotcar/catkin_ws/src/ros_nav/zkwl_robot_start/map/map -pbstream_filename=/home/robotcar/catkin_ws/src/ros_nav/zkwl_robot_start/map/map.pbstream -resolution=0.05
