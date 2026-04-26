#ifndef __ROS_MAGNETIC_CAN_H_
#define __ROS_MAGNETIC_CAN_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <string.h>
#include <queue>
#include "controlcan.h"
 #include <unistd.h>
 #include <ros/ros.h>
 #include <std_msgs/Int8.h>

 #include <ros_magnetic_nav_service/magnetic.h>            //要用\E5\88?msg 中定义的数据类型 

using namespace std;

//class RDSModbusSlave;
//extern RDSModbusSlave modSer;

void run_magnetic_can();

#endif


