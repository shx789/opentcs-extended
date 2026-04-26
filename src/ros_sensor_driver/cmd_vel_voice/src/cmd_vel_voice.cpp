#include <ros/ros.h>       //类似 C 语言的 stdio.h
#include <std_msgs/String.h>
#include <signal.h>
#include <geometry_msgs/Twist.h>
#include <string.h>
#include <string>
#include <cstdlib>  // 用于调用系统命令
#include "ros/package.h"

// 指定音频文件路径
std::string filepath;
std::string kaiji_audio = filepath +"/wav/kaiji_short.wav";
std::string left_turn_audio = filepath +"/wav/turn_left.wav";
std::string right_turn_audio = filepath + "/wav/turn_right.wav";
std::string back_audio = filepath +"/wav/back.wav";

// 当关闭包时调用，关闭
void mySigIntHandler(int sig)
{
    // 停止任何正在播放的音频
    system("pkill aplay");

    ros::shutdown();
}


void cmdVelCallback(const geometry_msgs::Twist::ConstPtr& msg)
{
    double linear_x = msg->linear.x;
    double angular_z = msg->angular.z;

    static int left_time = 0 ;
    static int right_time = 0;
    static int back_time = 0;

    // 停止任何正在播放的音频
    system("pkill aplay");

    if (angular_z >= 0.1)  // 左转
    {
        left_time++;
        right_time = 0;

        if(left_time >= 5)
        {
            std::string command = "aplay " + left_turn_audio;  // 使用aplay播放音频
            system(command.c_str());
            left_time = 0;
        }
    }
    else if (angular_z <= -0.1)  // 右转
    {
        right_time++;
        left_time = 0;
        if(right_time >= 5)
        {
            std::string command = "aplay " + right_turn_audio;  // 使用aplay播放音频
            system(command.c_str());
            right_time = 0;
        }
    }

    if(linear_x < -0.05)
    {
        back_time++;
        if(back_time >= 5)
        {
            std::string command = "aplay " + back_audio;  // 使用aplay播放音频
            system(command.c_str());
            back_time = 0;
        }
    }
    else if(linear_x > 0)
    {
        back_time = 0;
    }
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "cmd_vel_voice", ros::init_options::NoSigintHandler);
    ros::NodeHandle nh;
    ros::NodeHandle n("~");

    filepath = ros::package::getPath("cmd_vel_voice");

    kaiji_audio = filepath +"/wav/kaiji_short.wav";
    left_turn_audio = filepath +"/wav/turn_left.wav";
    right_turn_audio = filepath + "/wav/turn_right.wav";
    back_audio = filepath +"/wav/back.wav";

    signal(SIGINT, mySigIntHandler); // 把原来ctrl+c中断函数覆盖掉，把信号槽连接到mySigIntHandler保证关闭节点
    // 订阅cmd_vel话题
    ros::Subscriber sub = nh.subscribe("cmd_vel", 1, cmdVelCallback);
    
    // 停止任何正在播放的音频
    system("pkill aplay");
    std::string command = "aplay " + kaiji_audio;  // 使用aplay播放音频
    system(command.c_str());

    ros::spin();  // 保持节点运行并监听消息

    return 0;
}
