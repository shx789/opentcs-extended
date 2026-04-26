#ifndef __ AR_POSE_TRACK_H_
#define __ AR_POSE_TRACK_H_

#include <ros/ros.h>
#include <tf/tf.h>
#include <pwd.h>
#include <cmath>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <json/json.h>
#include <std_msgs/Int8.h>
#include <std_msgs/String.h>
#include <nav_msgs/Odometry.h>
#include <angles/angles.h>
#include <geometry_msgs/Pose.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <apriltag_tracker/tag.h>
#include <apriltag_tracker/set_id.h>
#include <apriltag_tracker/save_tag.h>
#include <apriltag_tracker/noise_fit.h>
#include <apriltag_tracker/custom_shift.h>
#include <apriltag_tracker/action1Action.h>
#include <actionlib/server/simple_action_server.h>
#include <boost/thread.hpp>

#include <CException.h>
#include <plog/Log.h> 
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#define pi 3.141592654

using namespace std;


typedef struct{
    int id;
    geometry_msgs::Pose pose;
}tag_data;

typedef actionlib::SimpleActionServer<apriltag_tracker::action1Action> Server;


enum tracker_error {No_error, List_Error, Service_Error, Track_Cancel, Goal_Lose, Track_Fail};
enum tracker_step {Track, Finish_Track, Standby, Back_Off, Finish_Back};


class Apriltag_tracker
{
    public:
        Apriltag_tracker();

        double Remap(double src, double src_min, double src_max, double dst_min, double dst_max);
        void Cmd_vel_pub(double v, double w);
        void Tag_callback(const apriltag_tracker::tag::ConstPtr & tag_msg);
        void Tf_pub(geometry_msgs::Pose pose, std::string parent_frame, std::string child_frame);
        geometry_msgs::Pose Tf_sub(std::string parent_frame, std::string child_frame);
        geometry_msgs::Point Get_tran(tag_data base_tag, tag_data tran_tag);
        geometry_msgs::Point Get_RPY(geometry_msgs::Quaternion msg);
        geometry_msgs::Point leastSquares(vector<double> x, vector<double> y);

        bool Pure_pursuit_control(geometry_msgs::Point target_pose, geometry_msgs::Point prev_pose);

        void Apriltag_tracker_action_feedback_publish(int step_flag, int id);
        void Apriltag_tracker_action_result_publish(int step_flag, int error_flag, int id);
        bool Apriltag_tracker_actioncb(const apriltag_tracker::action1GoalConstPtr &goal);

        void Read_target_pose();
        void Save_target_pose();
        void Read_noise_fit();
        void Save_noise_fit(double x_k, double x_b, double y_k, double y_b, double custom_x, double custom_y);
        int Search_id_list(int id, std::vector<tag_data> ar_list);
        int Search_num_list(int num, std::vector<tag_data> ar_list);
        bool Tag_save_service(apriltag_tracker::save_tag::Request &req, apriltag_tracker::save_tag::Response &res);
        bool Noise_fit_service(apriltag_tracker::noise_fit::Request &req, apriltag_tracker::noise_fit::Response &res);
        bool Custom_shift_service(apriltag_tracker::custom_shift::Request &req, apriltag_tracker::custom_shift::Response &res);

    private:
        ros::Subscriber tag_sub;
        ros::Publisher cmd_vel_pub, tag_pub;
        ros::ServiceClient set_id_client;
        ros::ServiceServer tag_save_service, noise_fit_service, custom_shift_service;

        std::string tag_save_filename;   //保存路径
        std::string noise_fit_filename;

        enum tracker_step step;
        enum tracker_error error;
        double noise_x_k, noise_x_b, noise_y_k, noise_y_b, custom_x_b, custom_y_k;
        double bias_odom, bias_gamma, bias_dtheta, max_x, max_y;
        bool step_1, step_2;
        tag_data tag, tran, target;

        Server *tracker_as_;

        std::vector<tag_data> save_list; //保存目标点缓存
        std::vector<tag_data> load_list; //读取目标点缓存

        apriltag_tracker::action1Result tracker_ac_result;
        apriltag_tracker::action1Feedback tracker_ac_feed;
};

#endif
