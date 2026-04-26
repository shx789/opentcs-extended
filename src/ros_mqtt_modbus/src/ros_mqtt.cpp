#include "ros_mqtt.h"
pthread_mutex_t mutex1;
queue<int> send_que;  //创建队列对象

/***************************************************************
 * @author     zxp
 * @brief      mqtt_poll
 * @version    v1
 * @return     null
 * @date       2023/10/09
 * @example     根据MQTT数据，设置MODBUS输入寄存器数据
 **************************************************************/
void json_analysis(std::string str,std::string topic)
{
    Json::Reader reader;
    Json::Value value;
    static uint16_t get_base_status_cnt = 0;

    //std::cout << str << std::endl;

    if(!reader.parse(str, value))
    {
        printf("json_analysis error\n");
        return;
    }
   
    int addr = 1;
    if(topic == "base_status")
    {
        if(value["cmd_type"] == "base_status")
        {
           get_base_status_cnt++;
           modSer.setTab_input_registers(addr++,get_base_status_cnt);

            bool local = value["local"]["location"].asBool();
            modSer.setTab_input_registers(addr++,local);

            bool imu = value["sensor"]["imu"].asBool();
            modSer.setTab_input_registers(addr++,imu);

            bool laser = value["sensor"]["laser"].asBool();
            modSer.setTab_input_registers(addr++,laser);

            bool robot = value["sensor"]["robot"].asBool();
            modSer.setTab_input_registers(addr++,robot);

            uint16_t charge = value["robot"]["charge"].asUInt();
            modSer.setTab_input_registers(addr++,charge);

            uint16_t amcl = value["local"]["amcl"].asDouble() * 100;
            modSer.setTab_input_registers(addr++,amcl);

            addr++;
            uint16_t mainerror = value["robot"]["mainerror"].asUInt();
            modSer.setTab_input_registers(addr++,mainerror);

            uint16_t suberror = value["robot"]["suberror"].asUInt();
            modSer.setTab_input_registers(addr++,suberror);

            unsigned int maintask = value["robot"]["maintask"].asUInt();
            modSer.setTab_input_registers(addr++,mainerror);

            uint16_t subtask = value["robot"]["subtask"].asUInt();
            modSer.setTab_input_registers(addr++,subtask);

            uint16_t status = value["robot"]["status"].asUInt();
            modSer.setTab_input_registers(addr++,status);

            //模式跳转为空闲或者错误，则清除保持寄存器的控制指令
            if(robot_status != status && (status == 0 || status == 9))
            {
               
                switch(robot_status)
                {
                    case 3:
                    {
                        int addr = POINT_CONTROL_ADDR - BASE_ADDR;
                        modSer.setRegisterValue(addr,0);
                        break;
                    }
                    case 4:
                    {
                        int addr = POINT_CONTROL_ADDR - BASE_ADDR;
                        modSer.setRegisterValue(addr,0);
                        break;
                    }
                    case 5:
                    {
                        int addr = CHARGE_CONTROL_ADDR - BASE_ADDR;
                        modSer.setRegisterValue(addr,0);
                        break;
                    }
                    case 6:
                    {
                        int addr = TRACK_CONTROL_ADDR - BASE_ADDR;
                        modSer.setRegisterValue(addr,0);
                        break;
                    }
                    case 7:
                    {
                        int addr = MORE_TASK_CONTROL_ADDR - BASE_ADDR;
                        modSer.setRegisterValue(addr,0);
                        break;
                    }
                    case 8:
                    {
                        int addr = MORE_TASK_CONTROL_ADDR - BASE_ADDR;
                        modSer.setRegisterValue(addr,0);
                        break;
                    }
                }
                
            }
            robot_status = status;

            addr = 19;
            int16_t vx = value["robot"]["vx"].asInt();
            modSer.setTab_input_registers(addr++,vx);

            int16_t vy = value["robot"]["vy"].asInt();
            modSer.setTab_input_registers(addr++,vy);

            int16_t vz = value["robot"]["vz"].asDouble() * 100;
            modSer.setTab_input_registers(addr++,vz);

            float x = value["pose"]["x"].asDouble();
            modSer.setTab_input_Floatregisters(addr++,x);

            addr = 24;
            float y = value["pose"]["y"].asDouble();
            modSer.setTab_input_Floatregisters(addr++,y);

            addr = 26;
            float yaw = value["pose"]["yaw"].asDouble();
            modSer.setTab_input_Floatregisters(addr++,yaw);

            addr = 28;
            uint16_t robot_status = value["robot"]["robot_status"].asUInt();
            modSer.setTab_input_registers(addr++,robot_status);

            uint16_t power = value["robot"]["power"].asDouble()*10;
            modSer.setTab_input_registers(addr++,power);

            uint16_t voltage = value["bms"]["voltage"].asDouble()*10;
            modSer.setTab_input_registers(addr++,voltage);

            uint16_t soc = value["bms"]["soc"].asUInt();
            modSer.setTab_input_registers(addr++,soc);

            int current = value["bms"]["current"].asDouble() * 100;
            modSer.setTab_input_registers(addr++,current);

            uint16_t error = value["bms"]["error"].asUInt();
            modSer.setTab_input_registers(addr++,error);

            uint16_t tem = value["bms"]["tem"].asUInt();
            modSer.setTab_input_registers(addr++,tem);

            uint16_t status1 = value["bms"]["status"].asUInt();
            modSer.setTab_input_registers(addr++,status1);

            bool magnetic = value["magnetic"]["material"].asBool();
            addr = MATERIAL_LIGHT_ADDR - INPUT_BASE_ADDR;
            modSer.setTab_input_registers(addr,magnetic);

            bool up = value["magnetic"]["up"].asBool();
            addr = UP_LIGHT_ADDR - INPUT_BASE_ADDR;
            modSer.setTab_input_registers(addr,up);

            bool low = value["magnetic"]["low"].asBool();
            addr = LOW_LIGHT_ADDR - INPUT_BASE_ADDR;
            modSer.setTab_input_registers(addr,low);
            //printf("json_analysis error up:%d low:%d addr:%d\n",up,low,addr);
        }
    }
    else if(topic == "task_feedback")
    {
        if(value["cmd_type"] == "task_feedback")
        {
            uint16_t status2 = 0;
            if(value["status"] == "process")
            {
                status2 = 1;
            }
            else if(value["status"] == "failure")
            {
                status2 = 2;
            }
            else if(value["status"] == "success")
            {
                status2 = 3;
            }
            else if(value["status"] == "timeout")
            {
                status2 = 4;
            }
            addr = 36;
            modSer.setTab_input_registers(addr++,status2);

            uint16_t id = value["id"].asUInt();
            modSer.setTab_input_registers(addr++,id);

            uint16_t type = 0;
            if(value["type"] == "point")
            {
                type = 1;
            }
            else if(value["type"] == "nav")
            {
                type = 2;
            }
            else if(value["type"] == "charge")
            {
                type = 3;
            }
            else if(value["type"] == "track")
            {
                type = 4;
            }
            else if(value["type"] == "location")
            {
                type = 5;
            }
            else if(value["type"] == "magnetic_nav")
            {
                type = 6;
            }
            modSer.setTab_input_registers(addr++,type);

            float gx = value["goal_pose"]["x"].asDouble();
            modSer.setTab_input_Floatregisters(addr++,gx);
            addr++;

            float gy = value["goal_pose"]["y"].asDouble();
            modSer.setTab_input_Floatregisters(addr++,gy);
            addr++;

            float gyaw = value["goal_pose"]["yaw"].asDouble();
            modSer.setTab_input_Floatregisters(addr++,gyaw);
            addr++;
        }
    }
}

/***************************************************************
 * @author     zxp
 * @brief      mqtt_poll
 * @version    v1
 * @return     null
 * @date       2023/10/09
 * @example     根据modbus指令地址发布MQTT控制指令
 **************************************************************/
void mqtt_to_json_pub(struct mg_connection *c)
{
    static std::string str;
    static Json::FastWriter swriter;
    static uint16_t res_cnt = 0;
    if (!send_que.empty()) //此处需要判断此时队列是否为空
    {
        //请求计数寄存器
        res_cnt++;
         modSer.setRegisterValue(0,res_cnt);

        switch(send_que.front())
        {
            case MOVE_CONTROL_ADDR:  //move指令
            {
                Json::Value move;
                int addr = MOVE_CONTROL_ADDR - BASE_ADDR;
                move["cmd_type"] = "move";
                int16_t vx = modSer.getRegisterValue(addr -3);
                int16_t vy = modSer.getRegisterValue(addr -2);
                int16_t vz = modSer.getRegisterValue(addr -1);
                move["vx"] = vx / 1000.0;
                move["vy"] = vy / 1000.0;
                move["vz"] = vz / 100.0;
                str = swriter.write(move);
                mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                modSer.setRegisterValue(addr,0);
                break;
            }
            case POINT_CONTROL_ADDR:    //巡航点指令
            {
                Json::Value nav;
                int addr = POINT_CONTROL_ADDR - BASE_ADDR;
                nav["cmd_type"] = "interest_point_control";
                uint16_t path_stop_time = modSer.getRegisterValue(addr -6);
                uint16_t path_mode          = modSer.getRegisterValue(addr -5);
                uint16_t run_speed            = modSer.getRegisterValue(addr -4);
                uint16_t circulates              = modSer.getRegisterValue(addr -3);
                uint16_t time                         = modSer.getRegisterValue(addr -2);
                uint16_t id                               = modSer.getRegisterValue(addr -1);
                uint16_t cmd                          = modSer.getRegisterValue(addr -0);
                nav["path_stop_time"]   = path_stop_time;
                nav["path_mode"]            = path_mode;
                nav["run_speed"]              = run_speed / 1000.0;
                nav["circulates"]                = circulates;
                nav["time"]                           = time;
                nav["id"]                                 = id;

                if(cmd == 1)
                {
                    nav["cmd"] = "start";
                    str = swriter.write(nav);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 2)
                {
                    nav["cmd"] = "random";
                    str = swriter.write(nav);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    nav["cmd"] = "stop";
                    str = swriter.write(nav);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                break;
            }
            case TRACK_CONTROL_ADDR:    //轨迹点指令
            {
                Json::Value track;
                int addr = TRACK_CONTROL_ADDR - BASE_ADDR;
                track["cmd_type"] = "trajectory_point_control";
                uint16_t stop_time      = modSer.getRegisterValue(addr -5);
                uint16_t run_speed     = modSer.getRegisterValue(addr -4);
                uint16_t circulates       = modSer.getRegisterValue(addr -3);
                uint16_t id                       = modSer.getRegisterValue(addr -2);
                uint16_t dir                      = modSer.getRegisterValue(addr -1);
                uint16_t cmd                  = modSer.getRegisterValue(addr -0);
                track["stop_time"]        = stop_time;
                track["run_speed"]       = run_speed / 1000.0;
                track["circulates"]         = circulates;
                track["id"]                         = id;
                track["dir"]                       = dir;

                if(cmd == 1)
                {
                    track["cmd"] = "start";
                    str = swriter.write(track);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    track["cmd"] = "stop";
                    str = swriter.write(track);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                
                break;
            }
            case MORE_TASK_CONTROL_ADDR:    //多任务指令
            {
                Json::Value more_task;
                int addr = MORE_TASK_CONTROL_ADDR - BASE_ADDR;
                more_task["cmd_type"] = "more_task_control";
                uint16_t run_speed      = modSer.getRegisterValue(addr -5);
                uint16_t main_task      = modSer.getRegisterValue(addr -4);
                uint16_t sub_task         = modSer.getRegisterValue(addr -3);
                uint16_t main_loop     = modSer.getRegisterValue(addr -2);
                uint16_t sub_loop        = modSer.getRegisterValue(addr -1);
                uint16_t cmd                  = modSer.getRegisterValue(addr -0);
                more_task["stop_time"]        = 0;
                more_task["run_speed"]       = run_speed / 1000.0;
                more_task["main_task"]        = main_task;
                more_task["sub_task"]           = sub_task;
                more_task["main_loop"]       = main_loop;
                more_task["sub_loop"]          = sub_loop;

                if(cmd == 1)
                {
                    more_task["cmd"] = "start";
                    str = swriter.write(more_task);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 2)
                {
                    more_task["cmd"] = "pause";
                    str = swriter.write(more_task);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    more_task["cmd"] = "stop";
                    str = swriter.write(more_task);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                break;
            }
            case CHARGE_CONTROL_ADDR:   //回充点指令
            {
                Json::Value chargr;
                int addr = CHARGE_CONTROL_ADDR - BASE_ADDR;
                chargr["cmd_type"] = "charge_point_control";
                uint16_t path_stop_time = modSer.getRegisterValue(addr -4);
                uint16_t path_mode          = modSer.getRegisterValue(addr -3);
                uint16_t run_speed            = modSer.getRegisterValue(addr -2);
                uint16_t time                         = modSer.getRegisterValue(addr -1);
                uint16_t cmd                          = modSer.getRegisterValue(addr -0);
                chargr["path_stop_time"]   = path_stop_time;
                chargr["path_mode"]            = path_mode;
                chargr["run_speed"]              = run_speed / 1000.0;
                chargr["time"]                           = time;

                if(cmd == 1)
                {
                    chargr["cmd"] = "goto";
                    str = swriter.write(chargr);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    chargr["cmd"] = "stop";
                    str = swriter.write(chargr);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                break;
            }
            case LOCAT_CONTROL_ADDR:    //定位点指令
            {
                Json::Value locat;
                int addr = LOCAT_CONTROL_ADDR - BASE_ADDR;
                locat["cmd_type"]    = "location_point_control";
                uint16_t id                    = modSer.getRegisterValue(addr -1);
                uint16_t cmd               = modSer.getRegisterValue(addr -0);
               
                locat["id"]                     = id;

                if(cmd == 1)
                {
                    locat["cmd"] = "start";
                    str = swriter.write(locat);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    locat["cmd"] = "stop";
                    str = swriter.write(locat);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                break;
            }
            case QR_LOCAT_CONTROL_ADDR:    //二维码指令
            {
                Json::Value locat;
                int addr = QR_LOCAT_CONTROL_ADDR - BASE_ADDR;
                locat["cmd_type"]    = "qr_location_point_control";
                uint16_t id                    = modSer.getRegisterValue(addr -1);
                uint16_t cmd               = modSer.getRegisterValue(addr -0);
               
                locat["id"]                     = id;

                if(cmd == 1)
                {
                    locat["cmd"] = "start";
                    str = swriter.write(locat);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    locat["cmd"] = "stop";
                    str = swriter.write(locat);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                break;
            }
            case STOP_CONTROL_ADDR: //停止所有任务指令
            {
                int addr = STOP_CONTROL_ADDR - BASE_ADDR;
                uint16_t cmd = modSer.getRegisterValue(addr -0);
                if(cmd == 1)
                {
                    modSer.setRegisterValue(addr,0);
                    switch(robot_status)
                    {
                    
                        case 3: //顺序巡航
                        {
                            Json::Value nav;
                            nav["cmd_type"] = "interest_point_control";
                            nav["cmd"] = "stop";
                            str = swriter.write(nav);
                            mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                            break;
                        }
                        case 4: //随机巡航
                        {
                            Json::Value nav;
                            nav["cmd_type"] = "interest_point_control";
                            nav["cmd"] = "stop";
                            str = swriter.write(nav);
                            mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                            break;
                        }
                        case 5: //  回充任务
                        {
                            Json::Value chargr;
                            chargr["cmd_type"] = "charge_point_control";
                            chargr["cmd"] = "stop";
                            str = swriter.write(chargr);
                            mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                            break;
                        }
                        case 6: //轨迹任务
                        {
                            Json::Value track;
                            track["cmd_type"] = "trajectory_point_control";
                            track["cmd"] = "stop";
                            str = swriter.write(track);
                            mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                            break;
                        }
                        case 7: //多任务
                        {
                            Json::Value more_task;
                            more_task["cmd_type"] = "more_task_control";
                            more_task["cmd"] = "stop";
                            str = swriter.write(more_task);
                            mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                            break;
                        }
                        case 8: //多任务暂停
                        {
                            Json::Value more_task;
                            more_task["cmd_type"] = "more_task_control";
                            more_task["cmd"] = "stop";
                            str = swriter.write(more_task);
                            mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                            break;
                        }
                    }
                }
                break;
            }
            case CLEAN_CONTROL_ADDR:    //错误清除指令
            {
                Json::Value clean;
                int addr = CLEAN_CONTROL_ADDR - BASE_ADDR;
                clean["cmd_type"]    = "error_reset";
                uint16_t cmd               = modSer.getRegisterValue(addr -0);
                if(cmd == 1)
                {
                    str = swriter.write(clean);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                    modSer.setRegisterValue(addr,0);
                }
                break;
            }
            case RESET_CONTROL_ADDR:    //冲定位指令
            {
                Json::Value reset;
                int addr = RESET_CONTROL_ADDR - BASE_ADDR;
                reset["cmd_type"]    = "reset";
            
                uint16_t cmd               = modSer.getRegisterValue(addr -0);
                if(cmd == 1)
                {
                    str = swriter.write(reset);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                    modSer.setRegisterValue(addr,0);
                }
                break;
            }
            case SOF_STOP_CONTROL_ADDR: //软件急停指令
            {
                Json::Value sof_stop;
                int addr = SOF_STOP_CONTROL_ADDR - BASE_ADDR;
                sof_stop["cmd_type"]    = "software_stop";
            
                uint16_t cmd   = modSer.getRegisterValue(addr -0);
                if(cmd == 1)
                {
                    sof_stop["stop"] = true;
                    str = swriter.write(sof_stop);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                else if(cmd == 0)
                {
                    sof_stop["stop"] = false;
                    str = swriter.write(sof_stop);
                    mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                }
                break;
            }
            case MAGNETIC_CONTROL_ADDR: //磁导航控制
            {
                Json::Value magnetic;
                int addr = MAGNETIC_CONTROL_ADDR - BASE_ADDR;
                uint16_t cmd = modSer.getRegisterValue(addr -0);
                magnetic["cmd_type"] = "magnetic_nav";
                magnetic["aim_id"] = 0;
                magnetic["aim_dir"] = 0;
                magnetic["aim_action"] = cmd;
                str = swriter.write(magnetic);
                mg_mqtt_pub(c, mg_str("robot_control"), mg_str(str.c_str()), s_qos, false);
                modSer.setRegisterValue(addr,0);
                break;;
            }
            default:
            {
                //需要发送无效信息才能保持连接
                mg_mqtt_pub(c, mg_str("modbus"), mg_str("online"), s_qos, false);
                break;
            }
        }
        send_que.pop();
    }
    else
    {
        mg_mqtt_pub(c, mg_str("modbus"), mg_str("online"), s_qos, false);
    }
}

/***************************************************************
 * @author     zxp
 * @brief      mqtt_poll
 * @version    v1
 * @return     null
 * @date       2023/10/09
 * @example     mqtt 连接处理
 **************************************************************/
void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) 
{
  if (ev == MG_EV_OPEN) 
  {
    MG_INFO(("CREATED"));
  } 
  else if (ev == MG_EV_CONNECT) 
  {
    MG_INFO(("MQTT  CONNECTED "));
    //PLOG_INFO << "MQTT  CONNECTED ";
  } 
  else if (ev == MG_EV_MQTT_OPEN) 
  {
    // MQTT connect is successful
    MG_INFO(("MQTT  OPEN  AND SUB"));
    //PLOG_INFO << "MQTT  OPEN  AND SUB ";
    //sub
    mg_mqtt_sub(s_conn, mg_str("base_status"), 0);
    mg_mqtt_sub(s_conn, mg_str("task_feedback"), 0);
    // Set a label that we're logged in
    c->label[0] = 'X';  
  } 
  else if (ev == MG_EV_MQTT_MSG)
  {
    // When we get echo response, print it
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    //MG_INFO(("RECEIVED %.*s <- %.*s", (int) mm->data.len, mm->data.ptr,(int) mm->topic.len, mm->topic.ptr));

   
    std::string rx_data,topic;
    //加入线程锁
    pthread_mutex_lock(&mutex1);
    json_analysis(rx_data.assign(mm->data.ptr,mm->data.len),topic.assign(mm->topic.ptr,mm->topic.len));
    pthread_mutex_unlock(&mutex1);
    
  } 
  else if (ev == MG_EV_POLL && c->label[0] == 'X') 
  {
        static unsigned long prev_second;
        unsigned long now_second = (*(unsigned long *) ev_data) /  20;
        if (now_second != prev_second) 
        {
            //pub 加入线程锁
            pthread_mutex_lock(&mutex1);
            //TRY //防止崩溃程序
            mqtt_to_json_pub(c);
            //END_TRY
            pthread_mutex_unlock(&mutex1);  
            prev_second = now_second;
        }
  }

  if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE)
   {
        MG_INFO(("Got event %d, stopping...", ev));
        //PLOG_INFO << "MQTT CLOSE";
        *(bool *) fn_data = true;  // Signal that we're done
  }
}

/***************************************************************
 * @author     zxp
 * @brief      mqtt_poll
 * @version    v1
 * @return     null
 * @date       2023/10/09
 * @example     mqtt poll处理
 **************************************************************/
void  mqtt_poll()
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
    opts.client_id = mg_str("modbus");
    opts.version = 4;
    opts.keepalive = 10;
    opts.will_retain = false;
                              
    s_conn = NULL;
    s_conn = mg_mqtt_connect(&mgr, "mqtt://0.0.0.0:1883", &opts, fn, &done);
    
    while (!done )
    {
        //10s超时
       mg_mgr_poll(&mgr,20); 
    }
    mg_mgr_free(&mgr); 
    MG_INFO(("MQTT Main is close"));
}

/***************************************************************
 * @author     zxp
 * @brief      mqtt_poll
 * @version    v1
 * @return     null
 * @date       2023/10/09
 * @example     从modbus的0x06命令中获得地址
 **************************************************************/
void get_modbus_cmd(int addr)
{
    send_que.push(addr);
}
