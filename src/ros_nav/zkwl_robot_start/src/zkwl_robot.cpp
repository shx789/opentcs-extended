#include "zkwl_robot.h"
/***
 @ Description	-> 
 @ Param		-> 6D 
 @ Author		-> zxp
 @ Date			-> 2019-03-10
 @ Function     -> Zkwl_start_object()
***/

Zkwl_start_object::Zkwl_start_object()
{
  x = y = th = vx = vy = vth = dt = 0.0;

  /* Get Luncher file define value */
  ros::NodeHandle nh_private("~");
  nh_private.param<std::string>("robot_frame_id", this->robot_frame_id, "base_footprint");
  nh_private.param<float>("filter_Vx_match", this->filter_Vx_match, 1.0f); 
  nh_private.param<float>("filter_Vth_match", this->filter_Vth_match, 1.0f); 

  /* Create a boot node for the underlying driver layer of the robot base_controller */
  this->zkwl_robot_sub  = n.subscribe("/agv_info", 100,  &Zkwl_start_object::cmd_agvCallback,     this);
  this->zkwl_robot1_sub = n.subscribe("/four_info", 100, &Zkwl_start_object::cmd_fourCallback,    this);
  this->zkwl_robot2_sub = n.subscribe("/PT_Robot_info", 100, &Zkwl_start_object::cmd_ptCallback,    this);
  this->zkwl_robot3_sub = n.subscribe("/DT_Robot_agv1", 100, &Zkwl_start_object::cmd_dtCallback,    this);
  this->zkwl_robot4_sub = n.subscribe("/agv_info_dt", 100, &Zkwl_start_object::cmd_agv_dtCallback,    this);
  this->zkwl_robot5_sub = n.subscribe("/dt/state_info", 100, &Zkwl_start_object::cmd_agv_dt_controlCallback,    this);
  this->zkwl_robot6_sub = n.subscribe("/four_wheel_info", 100, &Zkwl_start_object::cmd_agv_fourCallback,    this);
  this->imu_data_sub    = n.subscribe("/robot_imu", 1,   &Zkwl_start_object::cmd_imuCallback,     this);

  this->odom_pub        = n.advertise<nav_msgs::Odometry>("odom", 50);
}

Zkwl_start_object::~Zkwl_start_object()
{
	//Robot_Serial.close();
}

/***
 @ Description	-> amcl pose Callback function
 @ Param		-> const geometry_msgs::Twist &twist_aux 
 @ Author		-> zxp
 @ Date			-> 2021-03-10
 @ Function     -> Zkwl_start_object::cmd_AmclvelCallback(const geometry_msgs::PoseWithCovarianceStampedConstPtr &amclPose)
***/
void Zkwl_start_object::cmd_imuCallback(const sensor_msgs::Imu &imu_msg)
{

  Mpu6050.linear_acceleration.x = imu_msg.linear_acceleration.x;
  Mpu6050.linear_acceleration.y = imu_msg.linear_acceleration.y;
  Mpu6050.linear_acceleration.z = imu_msg.linear_acceleration.z;


  Mpu6050.angular_velocity.x = imu_msg.angular_velocity.x;
  Mpu6050.angular_velocity.y = imu_msg.angular_velocity.y;
  Mpu6050.angular_velocity.z = imu_msg.angular_velocity.z;
}

void Zkwl_start_object::cmd_ptCallback(const ros_pt_msg::pt1  &pt_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  this->vx =  (pt_msg.flwspeed + pt_msg.frwspeed + pt_msg.blwspeed + pt_msg.brwspeed) /4.0 / 1000.0 * filter_Vx_match;

  //this->vth = -agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;
	
  PublisherOdom();				
  this->last_time = current_time;
}

void Zkwl_start_object::cmd_dtCallback(const ros_dt_msg::dt1  &dt1_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  /* Get robot speed value */
  this->vx =  dt1_msg.Vx / 1000.0 * filter_Vx_match;

  //his->vth = agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;

  PublisherOdom();

  this->last_time = current_time;
}

void Zkwl_start_object::cmd_fourCallback(const ros_four1_msg::four1  &four1_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  this->vx =  (four1_msg.FLSpeed + four1_msg.FRSpeed + four1_msg.BLSpeed + four1_msg.BRSpeed) /4.0 / 1000.0 * filter_Vx_match;

  //this->vth = -agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;
	
  PublisherOdom();				

  this->last_time = current_time;
}

void Zkwl_start_object::cmd_agv_dtCallback(const agv_msgs::agv &agv_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  /* Get robot speed value */
  this->vx =  agv_msg.linear_velocity * filter_Vx_match;

  //his->vth = agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;

  PublisherOdom();

  this->last_time = current_time;
}

void Zkwl_start_object::cmd_agv_dt_controlCallback(const dt_control_msgs::dt_state &agv_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  /* Get robot speed value */
  this->vx =  agv_msg.linear_velocity * filter_Vx_match;

  //his->vth = agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;

  PublisherOdom();

  this->last_time = current_time;
}

void Zkwl_start_object::cmd_agv_fourCallback(const four_wheel_msgs::four_wheel &agv_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  /* Get robot speed value */
  this->vx =  agv_msg.linear_velocity * filter_Vx_match;

  //his->vth = agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;

  PublisherOdom();

  this->last_time = current_time;
}


void Zkwl_start_object::cmd_agvCallback(const ros_agv3_msg::agv1 &agv_msg)
{
  this->current_time = ros::Time::now();
  if(first_flag == true)
  {
    first_flag = false;
    this->last_time = this->current_time;
  }
  this->dt = (current_time - last_time).toSec();

  /* Get robot speed value */
  this->vx =  agv_msg.Vx / 1000.0 * filter_Vx_match;

  //his->vth = agv_msg.Vz;
  this->vth = Mpu6050.angular_velocity.z;

  double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
  double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
  double delta_th = vth * dt;
  x += delta_x;
  y += delta_y;
  th += delta_th;

  PublisherOdom();

  this->last_time = current_time;
}

/***
 @ Description	-> Publisher Odom
 @ Param		-> null
 @ Author		-> zxp
 @ Date			-> 2021-03-10
 @ Function     -> void Zkwl_start_object::PublisherOdom()
***/
void Zkwl_start_object::PublisherOdom()
{
	
  geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th);

  //next, we'll publish the odometry message over ROS
  nav_msgs::Odometry odom;
  odom.header.stamp = ros::Time::now();;
  odom.header.frame_id = "odom";

  //set the position
  odom.pose.pose.position.x = x;
  odom.pose.pose.position.y = y;
  odom.pose.pose.position.z = 0.0;
  odom.pose.pose.orientation = odom_quat;

  //set the velocity
  odom.child_frame_id = this->robot_frame_id;
  odom.twist.twist.linear.x =  this->vx;
  odom.twist.twist.linear.y =  this->vy;
  odom.twist.twist.angular.z = this->vth;		

  if(this->vx == 0)
  {
    memcpy(&odom.pose.covariance, odom_pose_covariance2, sizeof(odom_pose_covariance2));
    memcpy(&odom.twist.covariance, odom_twist_covariance2, sizeof(odom_twist_covariance2));
  }
  else
  {
    memcpy(&odom.pose.covariance, odom_pose_covariance, sizeof(odom_pose_covariance));
    memcpy(&odom.twist.covariance, odom_twist_covariance, sizeof(odom_twist_covariance));
  }				

  //publish the message
  odom_pub.publish(odom);
}


int main(int argc, char** argv)
{
	/* Voltage thread fb*/

	ros::init(argc, argv, "base_controller");
	ROS_INFO("[ZXP] base controller node start! ");

	Zkwl_start_object Robot_Control; 
	//Robot_Control.ReadAndWriteLoopProcess();
	ros::spin();
	return 0;
}


