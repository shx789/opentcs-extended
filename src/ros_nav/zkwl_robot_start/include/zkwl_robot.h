
#ifndef __ZKWL_ROBOT_H_
#define __ZKWL_ROBOT_H_

#include "ros/ros.h"
#include <iostream>
#include <string.h>
#include <string> 
#include <iostream>
#include <math.h>
#include <stdlib.h>       
#include <unistd.h>      
#include <sys/types.h>
#include <sys/stat.h>
#include <serial/serial.h>
#include <fcntl.h>          
#include <stdbool.h>
#include <tf/transform_broadcaster.h>
#include <std_msgs/String.h>
#include <std_msgs/Float32.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Vector3.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/LaserScan.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <four_wheel_msgs/four_wheel.h>
#include <ros_agv3_msg/agv1.h>            //要用到 msg 中定义的数据类型 
#include <ros_agv3_msg/agv2.h>            //要用到 msg 中定义的数据类型 
//#include "car_top/car_top.h"//自定义消息类型
#include <ros_four1_msg/four1.h>          //要用到 msg 中定义的数据类型 
#include <ros_pt_msg/pt1.h>      //要用到 msg 中定义的数据类型
#include "ros_dt_msg/dt1.h" //要用到 msg 中定义的数据类型
#include "agv_msgs/agv.h"//要用到 msg 中定义的数据类型
#include "dt_control_msgs/dt_state.h"


using namespace std;

#define RECIVER_DATA_HEADER 		0XFEFEFEFE
#define RECIVER_DATA_CHECK_SUM 		0XEE
#define PROTOBUF_SIZE				53

#define PI 					3.1415926f	

#define sampleFreq	50.0f				// sample frequency in Hz
#define twoKpDef	1.0f				// (2.0f * 0.5f)	// 2 * proportional gain
#define twoKiDef	0.0f				// (2.0f * 0.0f)	// 2 * integral gain

#define OFFSET_COUNT 	40

const double odom_pose_covariance[36] = {1e-3, 0, 0, 0, 0, 0, 
										0, 1e-3, 0, 0, 0, 0,
										0, 0, 1e6, 0, 0, 0,
										0, 0, 0, 1e6, 0, 0,
										0, 0, 0, 0, 1e6, 0,
										0, 0, 0, 0, 0, 1e3};
const double odom_pose_covariance2[36] = {1e-9, 0, 0, 0, 0, 0, 
										0, 1e-3, 1e-9, 0, 0, 0,
										0, 0, 1e6, 0, 0, 0,
										0, 0, 0, 1e6, 0, 0,
										0, 0, 0, 0, 1e6, 0,
										0, 0, 0, 0, 0, 1e-9};
 
const double odom_twist_covariance[36] = {1e-3, 0, 0, 0, 0, 0, 
										0, 1e-3, 0, 0, 0, 0,
										0, 0, 1e6, 0, 0, 0,
										0, 0, 0, 1e6, 0, 0,
										0, 0, 0, 0, 1e6, 0,
										0, 0, 0, 0, 0, 1e3};
const double odom_twist_covariance2[36] = {1e-9, 0, 0, 0, 0, 0, 
										0, 1e-3, 1e-9, 0, 0, 0,
										0, 0, 1e6, 0, 0, 0,
										0, 0, 0, 1e6, 0, 0,
										0, 0, 0, 0, 1e6, 0,
										0, 0, 0, 0, 0, 1e-9};

#pragma pack(1)
typedef struct __Mpu6050_Str_
{
	float X_data;
	float Y_data;
	float Z_data;
}Mpu6050_Str;


#pragma pack(4)


class Zkwl_start_object
{
  public:
    Zkwl_start_object();
    ~Zkwl_start_object();

    /* /cmd_val topic call function */
    void cmd_agvCallback(const ros_agv3_msg::agv1 &Agv);
    void cmd_fourCallback(const ros_four1_msg::four1  &four_msg);
    void cmd_ptCallback(const ros_pt_msg::pt1  &pt_msg);
    void cmd_dtCallback(const ros_dt_msg::dt1  &dt1_msg);
    void cmd_imuCallback(const sensor_msgs::Imu &imu_msg);
    void cmd_agv_dtCallback(const agv_msgs::agv &agv_msg);
    void cmd_agv_dt_controlCallback(const dt_control_msgs::dt_state &agv_msg);
    void cmd_agv_fourCallback(const four_wheel_msgs::four_wheel &agv_msg);

    /* This node Publisher topic and tf */
    void PublisherOdom();

    serial::Serial Robot_Serial; //声明串口对象 

  private:
    string robot_frame_id;
    float filter_Vx_match,filter_Vth_match;
    bool first_flag = true;

    /* Ros node define*/
    ros::NodeHandle n;
    ros::Time current_time, last_time;

    ros::Subscriber cmd_vel_sub, amcl_sub ,zkwl_robot_sub,zkwl_robot1_sub,zkwl_robot2_sub,zkwl_robot3_sub,zkwl_robot4_sub,zkwl_robot5_sub,zkwl_robot6_sub,imu_data_sub;;
    ros::Publisher odom_pub;

    /* Odom and tf value*/
    double x, y, th, vx, vy, vth, dt;

    sensor_msgs::Imu Mpu6050;
};


#endif


