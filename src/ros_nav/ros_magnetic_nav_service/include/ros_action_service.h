#ifndef __ROS_ACTION_SERVICE_H_
#define __ROS_ACTION_SERVICE_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <ros/ros.h>  
#include "ros_magnetic_can.h"
#include "ros_rfid_serial.h"
#include "ros/package.h"

#include <geometry_msgs/Twist.h>
#include <nav_msgs/Odometry.h>
#include <std_msgs/Float32.h>

//json
#include <json/json.h>

//action
#include <actionlib/server/simple_action_server.h>
#include <ros_magnetic_nav_service/action1Action.h> 
#include <actionlib/client/simple_action_client.h>

//
#include <ros_magnetic_nav_service/magnetic.h>            //要用\E5\88?msg 中定义的数据类型 

// 自定义消息
#include <lifting_ros/limitation_state.h>
#include <lifting_ros/speed_control.h>

#define HZ_1S 50

using namespace std;

typedef actionlib::SimpleActionServer<ros_magnetic_nav_service::action1Action> Server;

class Action_Service
{
    public:
        Action_Service();
        ~Action_Service();

        struct 
        {
            uint32_t aim_id;  //目标id
            int8_t aim_dir;    //目标方向
            int8_t aim_action;  //目标动作
            uint32_t now_rfid;  //当前rfid
            bool rfid_updata;  //rfid更新标志
            uint8_t step;                       //当前步骤
            uint8_t last_step;             //上一次步骤
            int8_t magnetic_data;  //磁条数据
            int8_t magnetic_num;  //霍尔检测数量 一般4-6
            int8_t magnetic_front_data;//前磁条数据
            int8_t magnetic_back_data;//后磁条数据
            int8_t magnetic_front_num;//前霍尔检测数量 一般4-6
            int8_t magnetic_back_num;//后霍尔检测数量 一般4-6
            float now_speed; //当前速度
            u_int16_t Light12; //鼎盛状态
            u_int16_t agv_state;//agv状态
            bool pub_litf;//发布鼎盛控制标志
            int8_t err_num;//错误码
        }ros_cn;
        
        enum AC{
            ac_Straight ,  //直行
            ac_Stop ,        //停车
            ac_Turn_Left,//左传
            ac_Turn_Right,//右转
            ac_Add_Cut,//加减速
            ac_Error,   //检测到错误卡片,停车
            ac_UpDown,//升降台控制
        };

        enum Error{
           er_success = 0,
           er_Off_Track = 1,
           er_Get_ErrorID,
           er_Cancel,
           er_Collision,
        };

    public:
        void recieveMessages();
    
    private:
        void odom_callback(const nav_msgs::Odometry::ConstPtr&  msg);
        void magnetic_can_front_callback(const ros_magnetic_nav_service::magnetic::ConstPtr&  msg);
        void magnetic_can_back_callback(const ros_magnetic_nav_service::magnetic::ConstPtr&  msg);
        void rfid_callback(const std_msgs::UInt16::ConstPtr&  msg);
        void limit_status_callback(const lifting_ros::limitation_state::ConstPtr&  msg);
        bool read_config_file_to_json(Json::Value *root);

        void clear_all_status(void);

        bool Go_Straight(geometry_msgs::Twist &cmd_vel,int8_t dir,float aim_speed,uint16_t delay);
        bool Turn_Left(geometry_msgs::Twist &cmd_vel,int8_t dir,float aim_speed,uint16_t delay);
        bool Turn_Right(geometry_msgs::Twist &cmd_vel,int8_t dir,float aim_speed,uint16_t delay);
        bool Do_Action(geometry_msgs::Twist &cmd_vel,u_int8_t aim_action,uint16_t delay);
        bool Up_Down(u_int8_t aim_action);

        ros::Publisher cmd_vel_pub;
        ros::Publisher lift_table_pub;

        geometry_msgs::Twist Cmd_Vel;

        //action
        Server *as_;
        ros_magnetic_nav_service::action1Result ac_result;
        ros_magnetic_nav_service::action1Feedback ac_feed;
        void actioncb(const ros_magnetic_nav_service::action1GoalConstPtr &goal);
        uint8_t  get_rfid_action(Json::Value config_data,uint32_t aim_rfid,uint32_t now_rfid);

        std::string config_file = "";


    public:
    
};

#endif


