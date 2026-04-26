cmake_minimum_required(VERSION 3.0.2)
project(four_wheel)

find_package(
  catkin REQUIRED
  COMPONENTS
    roscpp
    rospy
    serial
    four_wheel_msgs
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
    four_wheel_msgs
)

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_executable(
  four_wheel_node
  src/four_wheel.cc
  src/four_wheel_serial_ports.cc
  src/four_wheel_fsm.cc
  src/four_wheel_packages.cc
)

target_link_libraries(
  four_wheel_node
  ${catkin_LIBRARIES}
)

add_dependencies(
  four_wheel_node
  ${${PROJECT_NAME}_EXPORTED_TARGETS}
  ${catkin_EXPORTED_TARGETS}
)

#############
## Install ##
#############

install(
  TARGETS
  four_wheel_node
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
  launch/four_wheel.launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
 )

#############
## Testing ##
#############

#
