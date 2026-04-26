cmake_minimum_required(VERSION 3.0.2)
project(dt_control_msgs)

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
    dt_buff.msg
    dt_charge.msg
    dt_current.msg
    dt_date.msg
    dt_drive_error.msg
    dt_drive_hollysys_error.msg
    dt_hardware_version.msg
    dt_parameter.msg
    dt_remote_ctrl.msg
    dt_software_version.msg
    dt_state.msg
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
