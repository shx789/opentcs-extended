#ifndef __ROA_ROBOT_CONTROL_H_
#define __ROA_ROBOT_CONTROL_H_

#include <ros/ros.h>
#include <ros/timer.h>  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream> 
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <Eigen/Core>                   
#include <unistd.h>                 
#include <pthread.h>
#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <std_msgs/UInt8.h>
#include <sstream>
#include <signal.h>
#include "std_msgs/String.h"  //包含了使用的数据类型
#include "geometry_msgs/PoseStamped.h"
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <move_base_msgs/MoveBaseActionResult.h>
#include <actionlib_msgs/GoalID.h>
#include <std_msgs/Float32.h>
#include <std_msgs/UInt8.h>
#include <std_msgs/Int8.h>
#include<std_msgs/Float64.h>
#include "tf/transform_datatypes.h"
#include "tf/transform_listener.h"
#include <tf/transform_broadcaster.h>
#include <tf2/utils.h>
#include <tf2_ros/buffer.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/Pose.h>
#include <nav_msgs/Path.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/Imu.h>
#include <nav_msgs/OccupancyGrid.h>
#include <geometry_msgs/PoseArray.h>
#include <rosgraph_msgs/Log.h>
#include <visualization_msgs/MarkerArray.h>
#include <ros_agv3_msg/agv1.h>            //要用到 msg 中定义的数据类型 
#include <ros_agv3_msg/agv2.h>            //要用到 msg 中定义的数据类型 
#include <ros_four1_msg/four1.h>          //要用到 msg 中定义的数据类型 
#include <ros_pt_msg/pt1.h>                    //要用到 msg 中定义的数据类型
#include <ros_pt_msg/error.h>
#include <four_wheel_msgs/four_wheel.h>
#include <ros_dt_msg/dt1.h> //要用到 msg 中定义的数据类型
#include "agv_msgs/agv.h"//要用到 msg 中定义的数据类型
#include "ros_bms_msg/bms.h"              //要用到 msg 中定义的数据类型
#include "dt_control_msgs/dt_state.h"
#include "dt_control_msgs/dt_charge.h"

#include <apriltag_tracker/save_tag.h>   //二维码
#include <move_base_virtual_wall_server/CreateWall.h>	// srv文件
#include <move_base_virtual_wall_server/DeleteAll.h>	// srv文件
#include <move_base_virtual_wall_server/DeleteWall.h>	// srv文件
#include <std_srvs/Empty.h>

#include <json/json.h>
#include <queue>
#include "mongoose.h"

#include <unistd.h>
#include <pwd.h>

#include "CException.h"
#include <plog/Log.h> // Step1: include the headers	//引入头文件
#include "plog/Initializers/RollingFileInitializer.h"
#include <plog/Appenders/ColorConsoleAppender.h>

#include <sys/types.h> 
#include <sys/stat.h> 

#include <lifting_ros/limitation_state.h>
#include "ros_gpio_msg/gpio.h"

//激光回充启动
#define SCAN_ICP_MATCH   0

typedef unsigned char      u8;
/*************************************************************************/
using namespace std;
/*************************************************************************/
static char result_buf[1024];

pthread_t thread_zkwl_robot,thread_move_base,thread_slam_karto,thread_save_map,thread_updata,thread_mqtt_poll,thread_deal_callback,thread_set_location,thread_backup_map,thread_restore_map,thread_refining_map;
pthread_mutex_t mutex1;

//sub
ros::Subscriber movebase_result_sub;
ros::Subscriber movebase_status_sub;
ros::Subscriber power_sub;
ros::Subscriber zkwl_robot_sub;
ros::Subscriber zkwl_robot1_sub;
ros::Subscriber zkwl_robot2_sub;
ros::Subscriber zkwl_robot3_sub;
ros::Subscriber zkwl_robot4_sub;
ros::Subscriber zkwl_robot5_sub;
ros::Subscriber zkwl_robot6_sub;

ros::Subscriber zkwl_robot5_charge_sub;

ros::Subscriber gloab_path_sub;
ros::Subscriber loacl_path_sub;
ros::Subscriber scan_sub;
ros::Subscriber map_sub;
ros::Subscriber icp_status_sub;
ros::Subscriber rosout_sub;
ros::Subscriber imu_sub;
ros::Subscriber sensor_sub;
ros::Subscriber icp_score_sub;
ros::Subscriber constraint_sub;
ros::Subscriber trajectory_stop_sub;
ros::Subscriber bms_info_sub;
ros::Subscriber half_traffic_path_sub;
ros::Subscriber half_feedback_sub;
ros::Subscriber global_feedback_sub;
ros::Subscriber gpio_sub;
ros::Subscriber limitation_state_sub;

//pub
ros::Publisher  movebase_cancel_pub;
ros::Publisher  goal_pub;
ros::Publisher  goal1_pub;  //g轨道模式下使用
ros::Publisher  go_charge_pub;
ros::Publisher  agv_go_charge_pub;
ros::Publisher  four_go_charge_pub;
ros::Publisher  cmd_vel_pub;
ros::Publisher  initialpose1_pub;
ros::Publisher  trajectory_point_pub;
ros::Publisher  reset_pose_pub;
ros::Publisher  imu_pub;
ros::Publisher  pt_error_clear_pub;
ros::Publisher  agv_fault_clean_pub;
ros::Publisher  four_error_clear_pub;
ros::Publisher  dt_collision_clean_pub;
ros::Publisher  software_stop_pub;
ros::Publisher  pt_software_stop_pub;

//listener
tf::TransformListener  *scan_listener = NULL,*local_listener = NULL, *robot_listener = NULL;


//ros time
ros::Timer movebase_timer,reset_pose_timer,track_stop_timer;
int Order_Interest_index = 0;
int Random_Interest_index = 0;
int Go_Charge_Flag = 0;

//ServiceClient
ros::ServiceClient create_wall_client,delete_all_client,delete_wall_client,clear_costmaps_client;
ros::ServiceClient tracker_save_client; //二维码

//json
Json::Value json_base_status, json_scan , json_map ,json_feedback ,json_interest_point,json_charge_point,json_gloab_path,json_local_path,
            json_track_point , json_more_task, json_task_feedback,json_location_point,json_invent_wall,json_path_point,json_qr_pose;

//pose2d 兴趣点
geometry_msgs::PoseArray interest_point;

//上一次的目标点
geometry_msgs::PoseStamped last_goal;

queue<int> send_que;  //创建队列对象 

//状态结构体
struct
{
   struct
   {
      double x;
      double y;
      double yaw;   
   }pose;

   //特征板定位
   struct
   {
      double icp_socre;
   }reflector;

   struct
   {
      bool laser;
      bool imu;
      bool robot;   
   }sensor;

   struct
   {
      int    status;
      int    last_status;
      int    main_task; //主任务
      int    sub_task;  //子任务
      int    main_error; //错误主类型
      int    sub_error;  //错误子类型
      double power;
      int    charge; 
      int   robot_status; //底盘状态
      double vx;
      double vy;
      double vz;  
      bool doing_reset;
      u_int16_t light12; //鼎盛状态
   }robot;

   struct
   {
      double amcl;
      bool   carto;
   }local;
   
   struct
   {
      double voltage;
      double current;
      int    status;
      int    tem;
      int    remaining_capacity;
      int    error;
      int    soc;
   }bms;

   struct
   {
      bool material;
      bool up;
      bool low;
   }magnetic;
      
}base_status;

struct
{
    double power_max;
    double power_min;
    int go_power;
    int exit_power;
    bool go_power_check;
    bool exit_power_check;
    bool radra_check;
    bool path_check;
    bool start_point_check;
    double move_base_timer;
    int go_charge_error_time;
}setting;

struct Point
{
    double x;
    double y;
};

struct
{
    int start_index;
    bool circul_flag;
    bool dir_flag;
    int finish_goal;
    int  track_pause_step;
    int track_stop_time;
    struct Point first_point;
    struct Point second_point;
}track;

struct
{
    int start_index;
    double stop_time;
    bool circul_flag;
    bool path_mode_flag;       //轨道模式标志
    double half_path_stop_time;//轨道模式亭障时间0无穷等待单位s
}nav;

struct
{
    int start_id;
}location;

struct
{
    int main_task;  //主任务
    int sub_task;   //子任务
    bool main_loop;
    bool sub_loop;
    bool pause_flag;//暂停标志
    bool low_power_flag;//低电量标志

}more_task;

struct
{
    bool up_map_flag;
    bool up_scan_flag;
    bool sub_map_flag;

}deal_back;

struct
{
    unsigned char aim_id;
    unsigned char aim_dir;
    unsigned char aim_action;

}magnetic_nav;



enum track_dir
{
    Forward_Direction = 2,
    Reverse_Direction = 3,
};

//空闲、导航、建图、顺序巡航、随机巡航、回充、轨迹、错误、定位
enum mode {Mod_Free, Mod_Nav, Mod_Slam, Mod_Order_Interest, Mod_Random_Interest,Mod_Charge, Mod_Trajectory , Mod_More_Task ,Mod_More_Pause, Mod_Error,Mod_Location,Mod_Qr_Location,Mod_Magnetic}; // 定义枚举类型week

enum json_data
{
    Json_Status,
    Json_Scan,
    Json_Interest_point,
    Json_Charge_point,
    Json_Location_point,
    Json_Gloab_path,
    Json_Local_path,
    Json_Qr,
    Json_Feedback,
    Json_Map,
    Json_Task_feedback,
    Json_Invent_wall_point,
    Json_More_task,
    Json_Track_point,
   Json_Path_point,
};

enum scan_icp_result 
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
    ac_Excessive_Error,         //误差过大

} ; // 定义枚举类型

std::string interest_point_filename,charge_point_filename,location_point_filename,invent_wall_filename,slam_launch_filename,save_map_sh_filename,path_point_filename,
            updata_sh_filename,move_base_filename,setting_filename,trajectory_point_filename,more_task_filename,save_pose_filename,save_pose_filename1,slam_add_launch_filename,
            backup_map_sh_filename,restore_map_sh_filename,map_pgm_filename,refining_map_py_filename;
std::string golab_path_topic_name,local_path_topic_name;
std::string qr_location_point_filename;

//停止位置更新时间
u_int8_t stop_up_loca_time = 0;
bool add_slam_mode = false;


void move_base_cancel();
void timer_handler(const ros::TimerEvent& e );
void set_goal(std::string frame,double x,double y,double z,double w);
void update_pose(void);
static void charge_control(u8 mode);
void json_analysis(std::string str);
void json_to_ros_pub(Json::Value root);
void start_GoCharge(int time);
void stop_more_task();
void start_track_first(int id,bool dir);
void start_track(int id,int dir);
void stop_track();
void load_save_pose_json(void);
void start_more_task(void);
void start_more_task_interest_point(int point_index);
void start_more_task_track(int id, int dir);
void start_more_task_track_first(int id,int dir);
void task_feedback_set(std::string type, int id, std::string dir, std::string status,double gx,double gy,double gyaw,double nx,double ny,double nyaw);
void updata_save_pose_json(double x,double y,double yaw);
void pub_reset_pose(bool cmd);
void start_Location(int id);
void start_more_task_Location(int task_mode,int id);
void set_client_scan_icp_goal(unsigned char task_mode,unsigned int id);
void set_client_magnetic_nav_goal(unsigned char aim_id,unsigned char aim_dir,unsigned char aim_action);
void set_client_tracker_start_goal(int target_id, bool mode);
#endif


