#ifndef __ROS_MQTT_CLIENT_H_
#define __ROS_MQTT_CLIENT_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <string.h>
#include <jsoncpp/json/json.h>
#include <queue>
#include "mongoose.h"

#include <map>
#include "ros_gpio_msg/gpio.h"
#include <fstream>
#include <ros/ros.h>

using namespace std;

static struct mg_connection *s_conn;
static int s_qos = 0;
static int robot_status = 0;
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) ;
void mqtt_poll();
void readJsonFile(const std::string& file_path);
void json_analysis(std::string str, std::string topic);

// 定义存储结构
struct ConfigData {
    int model;
    int id;
    int circulates;
    int dir;
    int stop_time;
    int path_mode;
    int path_stop_time;
    int run_speed;
    int time;
};


#endif
