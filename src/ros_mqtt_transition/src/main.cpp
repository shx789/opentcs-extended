#include <iostream>

#include "ros_mqtt_client.h"
#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <string.h>
#include <json/json.h>
#include <queue>
#include <ros/ros.h>
#include <ros/package.h>

#include <fstream>


using namespace std;
std::string file_path ;
//MQTT线程
void mqttclientRunner()
{
    
    std::cout << "file_path:"<<file_path << std::endl;
    readJsonFile(file_path);
    mqtt_poll();
}

void readjsonRunner()
{
    // RosbridgeClient client;
    // client.connect("ws://192.168.254.100:9090");

    // 读取并保存 JSON 数据
    // std::cout << "file_path:"<<file_path << std::endl;
    // readJsonFile(file_path);
}

int main(int argc,char **argv)
{
    std::cout << "Running " << std::endl;
    ros::init(argc,argv,"ros_mqtt_transition",ros::init_options::NoSigintHandler);
            // 获取 JSON 文件路径
    std::string package_path = ros::package::getPath("ros_mqtt_transition"); // 替换为你的包名
    file_path = package_path + "/config/text.json";
    std::thread mqttSerThread(mqttclientRunner);
    // std::thread readjsonThread(readjsonRunner);
    
    mqttSerThread.join();
    // readjsonThread.join();
   


    return 0;
}
