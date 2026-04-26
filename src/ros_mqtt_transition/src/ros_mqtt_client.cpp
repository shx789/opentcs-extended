#include "ros_mqtt_client.h"

// #include <ros/ros.h>
// #include <fstream>
// #include <json/json.h>
// #include <map>
// #include "ros_gpio_msg/gpio.h"


pthread_mutex_t mutex1;
queue<int> send_que; // 创建队列对象

static std::string str;
static Json::FastWriter swriter;

ros::Publisher base_status_pub, task_feedback_pub;
ros::Subscriber gpio_sub,robot_state_sub;
// ros::Subscriber point_sub, error_reset_sub,charge_point_control_sub,robot_control_sub;
// ros_mqtt_transition::base_status base_status_msg;
// ros_mqtt_transition::task_feedback task_feedback_msg;

bool K1_flag,K2_flag,K3_flag,K4_flag = false;
static int connect_count = 0;
uint16_t status = 0;


// 使用 std::map 保存所有配置
std::map<std::string, ConfigData> config_map;
/*
void point_control_callback(const ros_mqtt_transition::point_control::ConstPtr &msg)
{
    Json::Value nav;
    nav["cmd_type"] = "interest_point_control";
    uint16_t path_stop_time = msg->path_stop_time;
    uint16_t path_mode = msg->path_mode;
    uint16_t run_speed = msg->run_speed;
    uint16_t circulates = msg->circulates;
    uint16_t time = msg->time;
    uint16_t id = msg->id;
    uint16_t cmd = msg->cmd;
    nav["path_stop_time"] = path_stop_time;
    nav["path_mode"] = path_mode;
    nav["run_speed"] = run_speed / 1000.0;
    nav["circulates"] = circulates;
    nav["time"] = time;
    nav["id"] = id;

    if (cmd == 1)
    {
        nav["cmd"] = "start";
        str = swriter.write(nav);
        mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
    }
    else if (cmd == 2)
    {
        nav["cmd"] = "random";
        str = swriter.write(nav);
        mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
    }
    else if (cmd == 0)
    {
        nav["cmd"] = "stop";
        str = swriter.write(nav);
        mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
    }
}
*/

/*
void error_reset_callback(const ros_mqtt_transition::error_reset::ConstPtr &msg)
{
    Json::Value clean;
    clean["cmd_type"] = "error_reset";
    uint16_t cmd = msg->cmd;
    if (cmd == 1)
    {
        str = swriter.write(clean);
        mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
    }
}
*/
/*
void charge_point_control_callback(const ros_mqtt_transition::charge_control::ConstPtr &msg)
{
    Json::Value chargr;
    chargr["cmd_type"] = "charge_point_control";
    uint16_t path_mode = msg->path_mode;
    uint16_t run_speed = msg->run_speed;
    uint16_t time = msg->time;
    uint16_t cmd = msg->cmd;
    chargr["path_mode"] = path_mode;
    chargr["run_speed"] = run_speed / 1000.0;
    chargr["time"] = time;

    if (cmd == 1)
    {
        chargr["cmd"] = "goto";
        str = swriter.write(chargr);
        mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
    }
    else if (cmd == 0)
    {
        chargr["cmd"] = "stop";
        str = swriter.write(chargr);
        mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
    }        
}
*/
void gpio_msg_callback(const ros_gpio_msg::gpio::ConstPtr &msg)
{
    Json::Value nav;

    if(msg->K1 == 0 && msg->K2 == 0 )
    {
        if(status == 3 || status == 6)
        {
            nav["cmd_type"] = "interest_point_control";

            nav["cmd"] = "stop";
            str = swriter.write(nav);
            mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
        }
        else if(status == 5)
        {
            Json::Value chargr;
            chargr["cmd_type"] = "charge_point_control";
              
            chargr["cmd"] = "stop";
            str = swriter.write(chargr);
            mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
        }
        else if(status == 9)
        {
            Json::Value clean;
            clean["cmd_type"] = "error_reset";
            
            str = swriter.write(clean);
            mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
            
        }
    } 

    if(msg->K1 == 0 && msg->K2 == 1 && msg->K3 == 1 && msg->K4 == 1)
    {
        if(!K1_flag && status == 0)
        {
            static int count1 = 0;
            count1++;
            if(count1 >= 6)
            {
                nav["cmd_type"] = "interest_point_control";
                nav["path_stop_time"] = config_map.at("K1").path_stop_time;
                nav["path_mode"] = config_map.at("K1").path_mode;
                nav["run_speed"] = config_map.at("K1").run_speed / 1000.0;
                nav["circulates"] = config_map.at("K1").circulates;
                nav["time"] = config_map.at("K1").time;
                nav["id"] =  config_map.at("K1").id;

                nav["cmd"] = "start";
                str = swriter.write(nav);
                mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                K1_flag = true;
                count1 = 0;
            }
            
            
            
        }
    }
    else if(msg->K1 == 1)
    {
        K1_flag = false;
    }

    if(msg->K2 == 0 && msg->K1 == 1 && msg->K3 == 1 && msg->K4 == 1)
    {
        if(!K2_flag && status == 0)
        {
            static int count2 = 0;
            count2++;
            if(count2 >= 6)
            {
                nav["cmd_type"] = "interest_point_control";
                nav["path_stop_time"] = config_map.at("K2").path_stop_time;
                nav["path_mode"] = config_map.at("K2").path_mode;
                nav["run_speed"] = config_map.at("K2").run_speed / 1000.0;
                nav["circulates"] = config_map.at("K2").circulates;
                nav["time"] = config_map.at("K2").time;
                nav["id"] =  config_map.at("K2").id;

                nav["cmd"] = "start";
                str = swriter.write(nav);
                mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                K2_flag = true;
                count2 = 0;
            }
            
        }
    }
    else if(msg->K2 == 1)
    {
        K2_flag = false;
    }

    if(msg->K3 == 0 && msg->K1 == 1 && msg->K2 == 1 && msg->K4 == 1)
    {
        if(!K3_flag && status == 0)
        {
            nav["cmd_type"] = "interest_point_control";
            nav["path_stop_time"] = config_map.at("K3").path_stop_time;
            nav["path_mode"] = config_map.at("K3").path_mode;
            nav["run_speed"] = config_map.at("K3").run_speed / 1000.0;
            nav["circulates"] = config_map.at("K3").circulates;
            nav["time"] = config_map.at("K3").time;
            nav["id"] =  config_map.at("K3").id;

            nav["cmd"] = "start";
            str = swriter.write(nav);
            mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
            K3_flag = true;
        }
    }
    else if(msg->K3 == 1)
    {
        K3_flag = false;
    }

    if(msg->K4 == 0)
    {
        if(!K4_flag && status == 0)
        {
            Json::Value chargr;
            chargr["cmd_type"] = "charge_point_control";
            uint16_t path_mode = config_map.at("K4").path_mode;
            uint16_t run_speed = config_map.at("K4").run_speed;
            uint16_t time = config_map.at("K4").time;
            chargr["path_mode"] = path_mode;
            chargr["run_speed"] = run_speed / 1000.0;
            chargr["time"] = time;
            chargr["cmd"] = "goto";
            str = swriter.write(chargr);
            mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
            
            K4_flag = true;
        }

    }
    else if(msg->K4 == 1)
    {
        // Json::Value chargr;
        // chargr["cmd_type"] = "charge_point_control";
        // uint16_t path_mode = config_map.at("K4").path_mode;
        // uint16_t run_speed = config_map.at("K4").run_speed;
        // uint16_t time = config_map.at("K4").time;
        // chargr["path_mode"] = path_mode;
        // chargr["run_speed"] = run_speed / 1000.0;
        // chargr["time"] = time;
        // chargr["cmd"] = "stop";
        // str = swriter.write(chargr);
        // mg_mqtt_pub(s_conn, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);

        K4_flag = false;
    }
}

void json_analysis(std::string str, std::string topic)
{
    connect_count = 0;
    Json::Reader reader;
    Json::Value value;
    static uint16_t get_base_status_cnt = 0;

    if (!reader.parse(str, value))
    {
        printf("json_analysis error\n");
        return;
    }

    if (topic == "base_status")
    {
        if (value["cmd_type"] == "base_status")
        {
            bool local = value["local"]["location"].asBool();
            bool imu = value["sensor"]["imu"].asBool();
            bool laser = value["sensor"]["laser"].asBool();
            bool robot = value["sensor"]["robot"].asBool();
            uint16_t charge = value["robot"]["charge"].asUInt();
            uint16_t amcl = value["local"]["amcl"].asDouble() * 100;
            uint16_t mainerror = value["robot"]["mainerror"].asUInt();
            uint16_t suberror = value["robot"]["suberror"].asUInt();
            unsigned int maintask = value["robot"]["maintask"].asUInt();
            uint16_t subtask = value["robot"]["subtask"].asUInt();
            status = value["robot"]["status"].asUInt();
            int16_t vx = value["robot"]["vx"].asInt();
            int16_t vy = value["robot"]["vy"].asInt();
            int16_t vz = value["robot"]["vz"].asDouble() * 100;
            float x = value["pose"]["x"].asDouble();
            float y = value["pose"]["y"].asDouble();
            float yaw = value["pose"]["yaw"].asDouble();
            // uint16_t robot_status = value["robot"]["robot_status"].asUInt();

            uint16_t power = value["robot"]["power"].asDouble() * 10;
            uint16_t voltage = value["bms"]["voltage"].asDouble() * 10;
            uint16_t soc = value["bms"]["soc"].asUInt();
            int current = value["bms"]["current"].asDouble() * 100;
            uint16_t error = value["bms"]["error"].asUInt();
            uint16_t tem = value["bms"]["tem"].asUInt();
            uint16_t status1 = value["bms"]["status"].asUInt();

            
        }
    }
    else if (topic == "task_feedback")
    {
        if (value["cmd_type"] == "task_feedback")
        {
            uint16_t status2 = 0;
            if (value["status"] == "process")
            {
                status2 = 1;
            }
            else if (value["status"] == "failure")
            {
                status2 = 2;
            }
            else if (value["status"] == "success")
            {
                status2 = 3;
            }
            else if (value["status"] == "timeout")
            {
                status2 = 4;
            }
            uint16_t id = value["id"].asUInt();

            uint16_t type = 0;
            if (value["type"] == "point")
            {
                type = 1;
            }
            else if (value["type"] == "nav")
            {
                type = 2;
            }
            else if (value["type"] == "charge")
            {
                type = 3;
            }
            else if (value["type"] == "track")
            {
                type = 4;
            }
            else if (value["type"] == "location")
            {
                type = 5;
            }
            float gx = value["goal_pose"]["x"].asDouble();
            float gy = value["goal_pose"]["y"].asDouble();
            float gyaw = value["goal_pose"]["yaw"].asDouble();

            
        }
    }
}

void mqtt_to_json_pub(struct mg_connection *c)
{
    static std::string str;
    static Json::FastWriter swriter;

    if (!send_que.empty()) // 此处需要判断此时队列是否为空
    {
        switch (send_que.front())
        {
            default:
            {
                // 需要发送无效信息才能保持连接
                mg_mqtt_pub(c, mg_str("client"), mg_str("online"), s_qos, false);
                break;
            }
        }
        send_que.pop();
    }
    else
    {
        mg_mqtt_pub(c, mg_str("client"), mg_str("online"), s_qos, false);
    }
}

void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    if (ev == MG_EV_OPEN)
    {
        MG_INFO(("CREATED"));
    }
    else if (ev == MG_EV_CONNECT)
    {
        MG_INFO(("MQTT  CONNECTED "));
        // PLOG_INFO << "MQTT  CONNECTED ";
    }
    else if (ev == MG_EV_MQTT_OPEN)
    {
        // MQTT connect is successful
        MG_INFO(("MQTT  OPEN  AND SUB"));
        // PLOG_INFO << "MQTT  OPEN  AND SUB ";
        // sub
        mg_mqtt_sub(s_conn, mg_str("base_status"), 0);
        mg_mqtt_sub(s_conn, mg_str("task_feedback"), 0);
        // Set a label that we're logged in
        c->label[0] = 'X';
    }
    else if (ev == MG_EV_MQTT_MSG)
    {
        // When we get echo response, print it
        struct mg_mqtt_message *mm = (struct mg_mqtt_message *)ev_data;
        // MG_INFO(("RECEIVED %.*s <- %.*s", (int) mm->data.len, mm->data.ptr,(int) mm->topic.len, mm->topic.ptr));

        std::string rx_data, topic;
        // 加入线程锁
        pthread_mutex_lock(&mutex1);
        json_analysis(rx_data.assign(mm->data.ptr, mm->data.len), topic.assign(mm->topic.ptr, mm->topic.len));
        pthread_mutex_unlock(&mutex1);
    }
    else if (ev == MG_EV_POLL && c->label[0] == 'X')
    {
        static unsigned long prev_second;
        unsigned long now_second = (*(unsigned long *)ev_data) / 20;
        if (now_second != prev_second)
        {
            // pub 加入线程锁
            pthread_mutex_lock(&mutex1);
            // TRY //防止崩溃程序
            mqtt_to_json_pub(c);
            // END_TRY
            pthread_mutex_unlock(&mutex1);
            prev_second = now_second;
        }
    }

    if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE)
    {
        MG_INFO(("Got event %d, stopping...", ev));
        // PLOG_INFO << "MQTT CLOSE";
        std::cout << ev_data << "aa\n";
        *(bool *)fn_data = true; // Signal that we're done
    }
}


void readJsonFile(const std::string& file_path) {

ROS_INFO_STREAM("aaa\n");
    // Json::Reader reader;
    // Json::Value value;

    try {
        // 打开 JSON 文件
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::ios_base::failure("Failed to open JSON file");
        }

        // 解析 JSON 数据
        Json::Value jsonData;
        file >> jsonData;

        for (const auto& key : jsonData.getMemberNames()) {
        const Json::Value& value = jsonData[key];

        ConfigData config;
        // config.model = value["model"].asInt();
        config.id = value["id"].asInt();
        config.circulates = value["circulates"].asInt();
        config.dir = value["dir"].asInt();
        config.stop_time = value["stop_time"].asInt();
        config.path_mode = value["path_mode"].asInt();
        config.path_stop_time = value["path_stop_time"].asInt();
        config.run_speed = value["run_speed"].asInt();

        // 将配置数据保存到map中，键为K1, K2, K3等
        config_map[key] = config;

        std::cout << "KEY:"<<key << std::endl;
        std::cout << "id:"<< config.id <<std::endl;
        // MG_INFO(("KEY:"),key);
    }

        ROS_INFO("JSON data successfully loaded and saved.");
    } catch (const std::exception& e) {
        ROS_ERROR_STREAM("Error reading JSON file: " << e.what());
    }
}


void mqtt_poll()
{
    struct mg_mgr mgr;
    struct mg_mqtt_opts opts;
    mg_mgr_init(&mgr);
    bool done = false;
    opts.user = mg_str("");
    opts.pass = mg_str("");
    opts.clean = true;
    opts.will_topic = mg_str("ros_mqtt");
    opts.will_message = mg_str("ros_mqtt_get");
    opts.will_qos = s_qos;
    opts.client_id = mg_str("client");
    opts.version = 4;
    opts.keepalive = 10;
    opts.will_retain = false;

    s_conn = NULL;
    s_conn = mg_mqtt_connect(&mgr, "mqtt://0.0.0.0:1883", &opts, fn, &done);

    ros::NodeHandle nh;
    // base_status_pub = nh.advertise<ros_mqtt_transition::base_status>("base_status_info", 20);       // 创建 publisher 对象
    // task_feedback_pub = nh.advertise<ros_mqtt_transition::task_feedback>("task_feedback_info", 20); // 创建 publisher 对象

    gpio_sub = nh.subscribe("gpio_msg", 10 ,gpio_msg_callback);
    robot_state_sub = nh.subscribe("gpio_msg", 10 ,gpio_msg_callback);
    while (!done)
    {
        while (ros::ok())
        {
            // 10s超时
            mg_mgr_poll(&mgr, 20);
            ros::spinOnce();

            // connect_count++;
            // if (connect_count == 50)
            // {
            //     connect_count = 0;
            //     base_status_msg.connect = false;
            //     base_status_pub.publish(base_status_msg);
            // }
        }
    }

    mg_mgr_free(&mgr);
    MG_INFO(("MQTT Main is close"));
}