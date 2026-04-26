cmake_minimum_required(VERSION 3.0.2)
project(agv_msgs)

find_package(
  catkin REQUIRED
  COMPONENTS
    std_msgs
    message_generation
)

################################################
## Declare ROS messages, services and actions ##
################################################

add_message_files(
  FILES
    agv_drive_error.msg
    agv_drive_lr.msg
    agv_drive_state.msg
    agv_drive.msg
    agv_error.msg
    agv_param.msg
    agv_dt.msg
    agv.msg
)

generate_messages(
  DEPENDENCIES
    std_msgs
)

################################################
## Declare ROS dynamic reconfigure parameters ##
################################################

#

###################################
## catkin specific configuration ##
###################################

catkin_package(
  CATKIN_DEPENDS
    std_msgs
    message_runtime
)

###########
## Build ##
###########

include_directories(
  ${catkin_INCLUDE_DIRS}
)

#############
## Install ##
#############

#

#############
## Testing ##
#############

#
