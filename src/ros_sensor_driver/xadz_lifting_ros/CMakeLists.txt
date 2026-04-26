cmake_minimum_required(VERSION 3.0.2)
project(xadz_lifting_ros)

find_package(
  catkin REQUIRED
  COMPONENTS
    roscpp
    rospy
    serial
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
)

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_executable(
  xadz_lifting_ros_node
  src/lifting_ros.cc
  src/lifting_ros_pkg.cc
)

target_link_libraries(
  xadz_lifting_ros_node
  ${catkin_LIBRARIES}
)

add_dependencies(
  xadz_lifting_ros_node
  ${${PROJECT_NAME}_EXPORTED_TARGETS}
  ${catkin_EXPORTED_TARGETS}
)

#############
## Install ##
#############

install(
  TARGETS
  xadz_lifting_ros_node
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
  launch/lifting_ros.launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
 )

#############
## Testing ##
#############

#
