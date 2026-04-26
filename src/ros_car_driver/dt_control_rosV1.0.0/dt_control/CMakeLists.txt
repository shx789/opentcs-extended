cmake_minimum_required(VERSION 3.0.2)
project(dt_control)

find_package(
  catkin REQUIRED
  COMPONENTS
    roscpp
    rospy
    serial
    dt_control_msgs
)

################################################
## Declare ROS messages, services and actions ##
################################################

#

################################################
## Declare ROS dynamic reconfigure parameters ##
################################################

#

###################################
## catkin specific configuration ##
###################################

catkin_package(
  CATKIN_DEPENDS
    roscpp
    rospy
    dt_control_msgs
)

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_executable(
  dt_control_node
  src/dt_control.cc
  src/dt_control_pkg.cc
)

target_link_libraries(
  dt_control_node
  ${catkin_LIBRARIES}
)

add_dependencies(
  dt_control_node
  ${${PROJECT_NAME}_EXPORTED_TARGETS}
  ${catkin_EXPORTED_TARGETS}
)

#############
## Install ##
#############

install(
  TARGETS
  dt_control_node
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(
  TARGETS
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
)

install(
  DIRECTORY include
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h"
  PATTERN ".svn" EXCLUDE
)

install(
  FILES
  launch/dt_control.launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
 )

#############
## Testing ##
#############

#
