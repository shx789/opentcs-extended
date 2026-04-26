cmake_minimum_required(VERSION 3.0.2)
project(four_wheel_msgs)

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
    four_wheel_drive_error.msg
    four_wheel_drive_fb_lr.msg
    four_wheel_drive_fb.msg
    four_wheel_drive_state.msg
    four_wheel_drive.msg
    four_wheel_error.msg
    four_wheel_param.msg
    four_wheel_pt.msg
    four_wheel.msg
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
