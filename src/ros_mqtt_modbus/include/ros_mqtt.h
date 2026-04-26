#ifndef __ROS_MQTT_H_
#define __ROS_MQTT_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <string.h>
#include <json/json.h>
#include <queue>
#include "mongoose.h"
#include "ros_modbus.h"

using namespace std;

class RDSModbusSlave;
extern RDSModbusSlave modSer;

static struct mg_connection *s_conn;
static int s_qos = 0;
static int robot_status = 0;
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) ;
void mqtt_poll();
void get_modbus_cmd(int addr);

#endif


