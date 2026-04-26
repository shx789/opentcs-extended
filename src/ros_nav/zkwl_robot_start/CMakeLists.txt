cmake_minimum_required(VERSION 2.8.3)
project(zkwl_robot_start)

find_package(catkin REQUIRED COMPONENTS
  actionlib
  actionlib_msgs
  geometry_msgs
  message_generation
  nav_msgs
  nav_msgs
  roscpp
  serial
  roslib
  rospy
  sensor_msgs
  std_msgs
  tf
  #加入对agv3自定义消息的依赖
  ros_agv3_msg
  #加入对four1自定义消息的依赖
  ros_four1_msg
#  car_top
   #加入对pt自定义消息的依赖
   ros_pt_msg
  #加入对dt自定义消息的依赖
  ros_dt_msg
  #加入对agv自定义消息的依赖
  agv_msgs
)

# ulimit -c unlimited
# ssh firefly@192.168.1.76
add_compile_options(-g -std=c++11)    #-g -> core dumped

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES zkwl_robot_start
#  CATKIN_DEPENDS geometry_msgs message_generation nav_msgs nav_msgs roscpp roslib rospy sensor_msgs std_msgs tf
  DEPENDS system_lib
)

# defind robot status massage
# command atkin_make -DCATKIN_WHITELIST_PACKAGES="new_msg"
include_directories(include ${catkin_INCLUDE_DIRS} ${PCL_INCLUDE_DIRS})
include_directories(
    include
    include/${PROJECT_NAME}
)

install(PROGRAMS
   scripts/show_mark.py
   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

add_executable(zkwl_robot_node src/zkwl_robot.cpp)
target_link_libraries(zkwl_robot_node ${catkin_LIBRARIES})

add_dependencies(zkwl_robot_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS}) #该项让编译器知道要注意编译顺序
target_link_libraries(zkwl_robot_node ${catkin_LIBRARIES})
