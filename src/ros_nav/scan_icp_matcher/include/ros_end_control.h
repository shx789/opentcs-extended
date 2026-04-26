#ifndef __ROS_END_CONTROL_H_
#define __ROS_END_CONTROL_H_
#include <ros/ros.h>    
#include <angles/angles.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_datatypes.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include<nav_msgs/Path.h>
#include <std_msgs/UInt8.h>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/LaserScan.h>

// transforms
#include <tf2/utils.h>
#include <tf2_ros/buffer.h>

//costmap
#include <base_local_planner/costmap_model.h>
#include <nav_msgs/OccupancyGrid.h>
#include <map_msgs/OccupancyGridUpdate.h>
#include <costmap_2d/costmap_2d.h>
#include <costmap_2d/costmap_2d_ros.h>
#include <costmap_2d/footprint.h>
#include <mutex>

//action
#include <actionlib/server/simple_action_server.h>
#include <scan_icp_matcher/action1Action.h> 
#include <actionlib/client/simple_action_client.h>

//servier
#include "scan_icp_matcher/server1.h"

typedef actionlib::SimpleActionServer<scan_icp_matcher::action1Action> Server;

namespace ros_end_control
{
class Ros_end_control
{
   public:
      Ros_end_control();
      ~Ros_end_control();

      enum step 
      {
         Start,                                            //开始
         First_Rotation,                        //第一次旋转
         Straight_Travel,                     //直行
         Second_Rotation,                //第二次旋转
         Align_Ment,                            //对准
         Align_Ment_Complete,     //对完成
         Retreat,                                     //后退

      } ; // 定义枚举类型

      enum result 
      {
         ac_Cancel,                          //取消
         ac_Success,                        //成功
         ac_Back_Success,           //后退成功
         ac_Loss_Icp,                      //信号丢失   
         ac_Not_Start,                      //信号丢失   
         ac_Not_Align,                      //信号丢失   not Align
         ac_Too_Nearl,                      //信号丢失   not Align
         ac_Pcd_Fail,                      //pcd 特征加载失败
         ac_Server_Fail,                //服务call失败
         ac_Excessive_Error,       //误差过大
        
      } ; // 定义枚举类型

       struct
      {
         struct
         {
            double x;
            double y;
            double yaw;   
         }pose;  

         struct
         {
            double x;
            double y;
            double yaw;   
         }goal_pose; 

         struct
         {
            double x;
            double y;
            double yaw;   
         }map_pose; 

         bool get_goal;
         bool will_run;
         bool first_goal_status;
         unsigned char run_step;
         unsigned char last_run_step;
         double laser_x;
         double score;

      }run_con;

      geometry_msgs::Point caul_point(double x,double y,double yaw,double s);
      void run_con_clear();
      void run_control();
      int caul_pi(double nx,double ny,double nyaw,double gx,double gy,double gyaw, geometry_msgs::Twist &cmd_vel,bool goal_status,bool dir);

      void actioncb(const scan_icp_matcher::action1GoalConstPtr &goal);
      

   private:

      void icp_fitness_scoreCallback(const std_msgs::Float64::ConstPtr &score);
      
      void costmap_Callback(const nav_msgs::OccupancyGrid::ConstPtr & msg);

      void costmap_updata_Callback(const map_msgs::OccupancyGridUpdate::ConstPtr & msg);

      void costmap_robot_poseCallback(const geometry_msgs::Pose2D::ConstPtr &pose);

      void scan_cb(const sensor_msgs::LaserScan::ConstPtr &msg);

      void robot_pose_get();

      bool getRobotPose(geometry_msgs::PoseStamped& global_pose) const;

      double caul_distance(double x1,double y1,double x2,double y2);

      int simula_caul_pi(double nx,double ny,double nyaw,double gx,double gy,double gyaw, geometry_msgs::Twist &cmd_vel,bool goal_status);

      void simulation_path(double nx,double ny,double nyaw,double gx,double gy,double gyaw, nav_msgs::Path &path,bool goal_status);

      bool  caul_goal(double nx,double ny,double nyaw,double gx,double gy,double gyaw);

      bool rotation_control(geometry_msgs::Twist &cmd_vel,double n_angel,double goal_angel,double max_w);

      bool straight_control(geometry_msgs::Twist &cmd_vel,double n_x,double n_y,double goal_x,double goal_y,double max_v,bool first_y);

      bool retreat_control(geometry_msgs::Twist &cmd_vel,double n_x,double n_y,double goal_x,double goal_y,double max_v);

      bool get_symbol(double m);

       double footprintCost(double x_i, double y_i, double theta_i);

       bool direct_obstacle_avoidance(void);
      

      std::vector<geometry_msgs::PoseStamped> global_plan;

       ros::Subscriber icp_fitness_score_sub;
       ros::Subscriber costmap_sub;
       ros::Subscriber costmap_updates_sub;
      ros::Subscriber costmap_robot_pose_sub;
      ros::Subscriber scan_sub;

      ros::Publisher cmd_vel_pub;
      ros::Publisher path_pub;
      ros::Publisher goal_pub;

      geometry_msgs::Pose current_pose_ros;
      geometry_msgs::PoseStamped goal_pose,temp_goal;
      geometry_msgs::Twist cmd_vel;

      tf::TransformListener  *robot_listener = NULL;

      double Kp_rho,Kp_alpha,Kp_beta,Max_v,Max_w,Min_socre,Before_Target,Forward_obstacle_dis,Side_obstacle_dis;
      std::vector<geometry_msgs::Point> footprint;

      boost::shared_ptr<base_local_planner::CostmapModel> costmap_model_= nullptr; 
      std::shared_ptr<costmap_2d::Costmap2D> costmap_ = nullptr;
      uint8_t* cost_translation_ = nullptr;
      std::mutex map_update_mutex_;
      bool costmap_init = false;
      std::string Footprint,Scan_topic;
      std::string scan_frame_id;
      bool get_scan_frame_id_flag = false;

      Server *as_;
      scan_icp_matcher::action1Result ac_result;
      scan_icp_matcher::action1Feedback ac_feed;

      //servier
      ros::ServiceClient  pattern_name_client;
      scan_icp_matcher::server1 se_pattern_name;
};
};
#endif
