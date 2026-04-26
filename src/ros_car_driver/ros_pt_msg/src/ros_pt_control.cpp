#include <sstream>
#include "ros/ros.h"
#include "std_msgs/String.h"
#include "ros_pt_control.h"
#include "ros_pt_msg/control1.h"
#include "ros_pt_msg/error.h"
#include <signal.h>

void mySigIntHandler(int sig)
{
   ros::shutdown();
}

int main(int argc, char** argv)
{
    //Ros 节点初始化
    ros::init(argc, argv, "pt_control_talker");

    //创建节点句柄
    ros::NodeHandle n;

    //创建一个publisher，参数依次为chatter->topic名称;1000->消息队列的长度
    // 当发布消息实际速度太慢时，Publisher会将消息储存在一定空间队列中，如果消息数量超过队列大小，ROS会自动删除队列中最早入队的消息
    // 消息类型为std_msgs::String
    ros::Publisher control_pub = n.advertise<ros_pt_msg::control1>("/PT_Control",1000);

    // 设置循环的频率
    // 单位是hz,此处为1hz，当调用Rate::sleep()时，ROS节点会根据此处设置的频率进行相应的休眠
    ros::Rate loop_rate(1);
   signal(SIGINT, mySigIntHandler); 

    while(ros::ok())
    {
        // 按照自定义的消息类型封装一条topic
        // 发布消息
        // 将封装好的topic发布出去
        ros_pt_msg::control1 control_msg;
        control_msg.flspeed = 0;
        control_msg.frspeed = 0;
        control_msg.blspeed = 0;
        control_msg.brspeed = 0;
        control_msg.stop = 0;
        control_pub.publish(control_msg);

        //循环等待回调函数
        ros::spinOnce();

        // 按照循环频率延时
        loop_rate.sleep();
    
    }

    return 0;
}
