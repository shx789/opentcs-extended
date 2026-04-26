cmake_minimum_required(VERSION 2.8.3)
project(apriltag_tracker)


find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  tf
  std_msgs
  message_generation
  serial
  geometry_msgs
  actionlib_msgs 
  actionlib
  roslib
)


## Generate messages in the 'msg' folder
add_message_files(
  FILES
  tag.msg
 )

## Generate services in the 'srv' folder
add_service_files(
  FILES
  set_id.srv
  save_tag.srv
  noise_fit.srv
  custom_shift.srv
)

# Generate actions in the 'action' folder
add_action_files(
  DIRECTORY action 
  FILES 
  action1.action
)

## Generate added messages and services with any dependencies listed here
generate_messages(
  DEPENDENCIES
  std_msgs
  geometry_msgs
  actionlib_msgs
)


catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES apriltag_tracker
#  CATKIN_DEPENDS roscpp rospy std_msgs
#  DEPENDS system_lib
  CATKIN_DEPENDS 
  message_runtime
)


include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)
link_directories(
  ${catkin_LIB_DIRS} lib
)

# catkin_install_python(PROGRAMS
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

add_executable(apriltag_tracker src/apriltag_tracker.cpp)
target_link_libraries(apriltag_tracker ${catkin_LIBRARIES} libjson_linux-gcc-7_libmt.a)

add_dependencies(apriltag_tracker ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS}) #该项让编译器知道要注意编译顺序
target_link_libraries(apriltag_tracker ${catkin_LIBRARIES})
