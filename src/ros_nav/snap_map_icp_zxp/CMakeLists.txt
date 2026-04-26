cmake_minimum_required(VERSION 2.8.3)
project(snap_map_icp_zxp)

find_package(catkin REQUIRED std_msgs nav_msgs laser_geometry sensor_msgs geometry_msgs tf pcl_conversions)

#find_package(Eigen REQUIRED)

#include_directories(${EIGEN_INCLUDE_DIRS})
include_directories("/usr/include/eigen3")
#add_definitions(${EIGEN_DEFINITIONS})


find_package(PCL REQUIRED)
include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})

catkin_package(
  CATKIN_DEPENDS std_msgs nav_msgs laser_geometry sensor_msgs geometry_msgs tf pcl_conversions
  )

include_directories(${catkin_INCLUDE_DIRS}
  )

add_executable(SnapMapICPZXP src/SnapMapICP.cpp)



target_link_libraries(SnapMapICPZXP ${catkin_LIBRARIES} ${PCL_LIBRARIES})

