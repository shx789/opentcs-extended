#include "ros_action_service.h"

using namespace std;
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author    zxp
 * @brief      Constructor
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
Action_Service::Action_Service()
{
    config_file = ros::package::getPath("ros_magnetic_nav_service") + "/config/config.json" ;
    clear_all_status();    
}

void Action_Service::clear_all_status(void)
{
    ros_cn.aim_id = 0;
    ros_cn.now_rfid = 0;
    ros_cn.rfid_updata = false;
    ros_cn.step = 0;
    ros_cn.magnetic_front_data = 0;
    ros_cn.magnetic_back_data = 0;
    ros_cn.magnetic_front_num = 0;
    ros_cn.magnetic_back_num = 0;
    ros_cn.magnetic_data = 0;
    ros_cn.magnetic_num = 0;
    ros_cn.Light12 = 0;
    ros_cn.pub_litf = false;
    ros_cn.err_num = 0;

    Cmd_Vel.linear.x = 0.0;
    Cmd_Vel.angular.z = 0.0;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      Destructor
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
Action_Service::~Action_Service()
{
}

/// @brief 
/// @param msg 
void Action_Service::odom_callback(const nav_msgs::Odometry::ConstPtr&  msg)
{
    ros_cn.now_speed = msg->twist.twist.linear.x;
}

/// @brief 磁传感器数据订阅 - +
/// @param msg 
void Action_Service::magnetic_can_front_callback(const ros_magnetic_nav_service::magnetic::ConstPtr&  msg)
{
    ros_cn.magnetic_front_data = msg->value;
    ros_cn.magnetic_front_num = msg->num;
}

/// @brief 磁传感器数据订阅 - +
/// @param msg 
void Action_Service::magnetic_can_back_callback(const ros_magnetic_nav_service::magnetic::ConstPtr&  msg)
{
    ros_cn.magnetic_back_data = msg->value;
    ros_cn.magnetic_back_num = msg->num;
}

/// @brief rfid数据订阅
/// @param msg 
void Action_Service::rfid_callback(const std_msgs::UInt16::ConstPtr&  msg)
{
    //id 和之前的不同表示id更新
    if(ros_cn.now_rfid != msg->data)
    {
        ros_cn.now_rfid = msg->data;
        ros_cn.rfid_updata = true;
        //printf("get rfid:%d\n",msg->data);
    } 

    //如果在目标点启动，则更新
    if(ros_cn.aim_id == msg->data)
    {
        ros_cn.now_rfid = msg->data;
        ros_cn.rfid_updata = true;
    }
    
}

/// @brief 3:up 4:low other:0
/// @param msg 
void Action_Service:: limit_status_callback(const lifting_ros::limitation_state::ConstPtr&  msg)
{
    if(msg->low_limit_state)
    {
        ros_cn.Light12 = 4;
    }
    else if(msg->upper_limit_state)
    {
        ros_cn.Light12 = 3;
    }
    else
    {
        ros_cn.Light12 = 0;
    }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

/// @brief 读取配置文件config.json
/// @param root  返回读取的json数据
/// @return  返回读取状态，true成功
bool Action_Service::read_config_file_to_json(Json::Value *root)
{
    Json::Reader reader;
    Json::FastWriter swriter;
    
    std::ifstream ifs(config_file.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"config file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, *root))
    {
        ROS_ERROR("config json_analysis error");
        ifs.close();
        return false;
    }
    else
    {
        ifs.close();
        return true;
    }
}

/// @brief 根据目标id和当前id从配置中获得运行动作
/// @param config_data 配置文件的json
/// @param aim_rfid  目标id
/// @param now_rfid  当前id
/// @return  返回动作
uint8_t  Action_Service::get_rfid_action(Json::Value config_data,uint32_t aim_rfid,uint32_t now_rfid)
{
    //到点
    if(aim_rfid == now_rfid)
    {
        return ac_Stop;
    }

    for(uint i =0;i<config_data.size();i++)
    {
        if(config_data[i]["aim_id"].asUInt() == aim_rfid)
        {
            //查找左转数组
            for(uint j=0;j<config_data[i]["trun_left_id"].size();j++)
            {
                if(config_data[i]["trun_left_id"][j].asUInt() == now_rfid)
                {
                    return ac_Turn_Left;
                }
            }
            //查找右转数组
            for(uint j=0;j<config_data[i]["trun_right_id"].size();j++)
            {
                if(config_data[i]["trun_right_id"][j].asUInt() == now_rfid)
                {
                    return ac_Turn_Right;
                }
            }
            //查找加减速数组
            for(uint k=0;k<config_data[i]["add_cut_id"].size();k++)
            {
                if(config_data[i]["add_cut_id"][k].asUInt() == now_rfid)
                {
                    return ac_Add_Cut;
                }
            }
             //查找错误数组用于防止走过rfid
            for(uint k=0;k<config_data[i]["error_id"].size();k++)
            {
                if(config_data[i]["error_id"][k].asUInt() == now_rfid)
                {
                    return ac_Error;
                }
            }
        }
    }
    //直行
    return ac_Straight;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Action_Service::Go_Straight(geometry_msgs::Twist &cmd_vel,int8_t dir,float aim_speed,uint16_t delay)
{
    static int stop_cnt = 0;

    if(ros_cn.now_speed == 0)
    {
        cmd_vel.linear.x = 0.0;
    }

    ///front
    if(dir == 0)
    {
        if(ros_cn.magnetic_num >= 1 && ros_cn.magnetic_num <= 6)
        {
            //加速
            if(ros_cn.now_speed < aim_speed)
            {
                cmd_vel.linear.x += 0.02;
                if(cmd_vel.linear.x > aim_speed)
                {
                    cmd_vel.linear.x = aim_speed;
                }
            }
            else
            {
                cmd_vel.linear.x = aim_speed;
            }
            //值/100🉐角速度
            cmd_vel.angular.z = ros_cn.magnetic_data / 100.0;
            stop_cnt = 0;
        }
        else if(ros_cn.magnetic_num > 6)
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            stop_cnt = 0;
            ros_cn.err_num = er_success;
            return false;
        }
        else //脱离磁条处理 2s
        {
            stop_cnt++;
            if(stop_cnt > delay)
            {
                cmd_vel.linear.x = 0.0;
                cmd_vel.angular.z = 0.0;
                stop_cnt = 0;
                ros_cn.err_num = er_Off_Track;
                return false;
            }
        }
    }
    else
    {
        aim_speed = -aim_speed;
        if(ros_cn.magnetic_num >= 1 && ros_cn.magnetic_num <= 6)
        {
            //加速
            if(ros_cn.now_speed > aim_speed)
            {
                cmd_vel.linear.x -= 0.02;
                if(cmd_vel.linear.x < aim_speed)
                {
                    cmd_vel.linear.x = aim_speed;
                }
            }
            else
            {
                cmd_vel.linear.x = aim_speed;
            }
            //值/100🉐角速度
            cmd_vel.angular.z = ros_cn.magnetic_data / 100.0;
            stop_cnt = 0;
        }
        else if(ros_cn.magnetic_num > 6)
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            stop_cnt = 0;
            ros_cn.err_num = er_success;
            return false;
        }
        else //脱离磁条处理 2s
        {
            stop_cnt++;
            if(stop_cnt > delay)
            {
                cmd_vel.linear.x = 0.0;
                cmd_vel.angular.z = 0.0;
                stop_cnt = 0;
                ros_cn.err_num = er_Off_Track;
                return false;
            }
        }
    }
    return true;
}

/// @brief  turn_Left
/// @param cmd_vel 
/// @param dir 
/// @param aim_speed 
/// @return 
bool Action_Service::Turn_Left(geometry_msgs::Twist &cmd_vel,int8_t dir,float aim_speed,uint16_t delay)
{
    static int time_cnt = 0,step = 0;
    if(ros_cn.last_step != ac_Turn_Left)
    {
        step = 0;
    }

    switch (step)
    {
        case 0: //设置停顿
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            time_cnt = delay;
            step = 1;
            break;
        }
        case 1: //计时停顿
        {
            time_cnt--;
            if(time_cnt == 0)
            {
                step = 2;
            }
            break;
        }
        case 2: //旋转出当前磁条
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = aim_speed;
            if(ros_cn.magnetic_num == 0)
            {
                step = 3;
            }
            break;
        }
        case 3:
        {
            //旋转到了下一磁条
            if(ros_cn.magnetic_num >= 3 && ros_cn.magnetic_data <= 0)
            {
                cmd_vel.linear.x = 0.0;
                cmd_vel.angular.z = 0.0;
                step = 4;
                time_cnt = delay;
            }
            break;
        }
        case 4: //计时停顿
        {
            time_cnt--;
            if(time_cnt == 0)
            {
                step = 0;
                return true;
            }
            break;
        }
        default:break;
    }
    return false;
}

/// @brief  turn_Right
/// @param cmd_vel 
/// @param dir 
/// @param aim_speed 
/// @return 
bool Action_Service::Turn_Right(geometry_msgs::Twist &cmd_vel,int8_t dir,float aim_speed,uint16_t delay)
{
    static int time_cnt = 0,step = 0;
    aim_speed = -aim_speed;
    if(ros_cn.last_step != ac_Turn_Right)
    {
        step = 0;
    }

    switch (step)
    {
        case 0: //设置停顿
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            time_cnt = delay;
            step = 1;
            break;
        }
        case 1: //计时停顿
        {
            time_cnt--;
            if(time_cnt == 0)
            {
                step = 2;
            }
            break;
        }
        case 2: ////旋转出当前磁条
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = aim_speed;
            if(ros_cn.magnetic_num == 0)
            {
                step = 3;
            }
            break;
        }
        case 3:
        {
             //旋转到了下一磁条
            if(ros_cn.magnetic_num >= 3 && ros_cn.magnetic_data >= 0)
            {
                cmd_vel.linear.x = 0.0;
                cmd_vel.angular.z = 0.0;
                step = 4;
                time_cnt = delay;
            }
            break;
        }
        case 4: //计时停顿
        {
            time_cnt--;
            if(time_cnt == 0)
            {
                step = 0;
                return true;
            }
            break;
        }
        default:break;
    }
    return false;
}

bool Action_Service::Do_Action(geometry_msgs::Twist &cmd_vel,u_int8_t aim_action,uint16_t delay)
{
    static int step = 0;
    if(ros_cn.last_step != ac_Stop)
    {
        step = 0;
    }
    switch(step)
    {
        case 0:
        {
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            step = 1;
            break;
        }
        case 1:
        {
            std_msgs::Float32 msg;
            msg.data = aim_action;
            lift_table_pub.publish(msg);
            ros_cn.pub_litf = true;
            step = 2;
            break;
        }
        case 2:
        {
            if(ros_cn.Light12 == aim_action)
            {
                    step = 0;
                    return true;
            }
            break;
        }
        default:break;
    }
    return false;
}

//3:up 4:low
bool Action_Service::Up_Down(u_int8_t aim_action)
{
    static int step = 0;
    if(ros_cn.last_step != ac_UpDown)
    {
        step = 0;
    }
    switch(step)
    {
        case 0:
        {
            step = 1;
            break;
        }
        case 1:
        {
            std_msgs::Float32 msg;
            if(aim_action == 3)
            {
                msg.data = 50.0;
            }
            else if(aim_action == 4)
            {
                msg.data = -50.0;
            }
            else
            {
                return true;
            }
            lift_table_pub.publish(msg);
            ros_cn.pub_litf = true;
            step = 2;
            break;
        }
        case 2:
        {
            if(ros_cn.Light12 == aim_action)
            {
                    step = 0;
                    return true;
            }
            break;
        }
        default:break;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
/// @brief action的回调函数
/// @param goal 
void Action_Service::actioncb(const ros_magnetic_nav_service::action1GoalConstPtr &goal)
{
    clear_all_status();

    Json::Value config_data;
    //读取配置文件config.json
    if(!read_config_file_to_json(&config_data))
    {
        as_->setPreempted(ac_result);
        return;
    }
    //赋值目标ID
    ros_cn.aim_id = goal->aim_id;
    ros_cn.aim_dir = goal->aim_dir;
    ros_cn.aim_action = goal->aim_action;

    //忽略当前启动的点
    ros_cn.rfid_updata = false;

    printf("get action:%d\n",get_rfid_action(config_data,ros_cn.aim_id,2));
    ros::Rate loop_rate(HZ_1S);    //设置发送数据的频率为50Hz
    while(ros::ok())
    { 

         //cancel
        if (as_->isPreemptRequested())
        {
            Cmd_Vel.linear.x = 0.0;
            Cmd_Vel.angular.z = 0.0; 
            cmd_vel_pub.publish(Cmd_Vel);
            
            ros_cn.err_num = er_Cancel;
            ac_result.result = ros_cn.err_num;
            ac_result.now_rfid = ros_cn.now_rfid;
            as_->setPreempted(ac_result);
            return;
        }

         //防撞杆促发，进入错误模式
        if(ros_cn.agv_state & 0x000c)
        {
            ros_cn.err_num = er_Collision;
            ac_result.result = ros_cn.err_num;
            ac_result.now_rfid = ros_cn.now_rfid;
            as_->setPreempted(ac_result);
            return;
        }

        //读取到新的rfid 则从配置文件中获取动作
        if(ros_cn.rfid_updata == true)
        {
            ros_cn.step = get_rfid_action(config_data,ros_cn.aim_id,ros_cn.now_rfid);
            // printf("step:%d\n",ros_cn.step);
            ros_cn.rfid_updata = false;

            ac_feed.step = ros_cn.step;
            ac_feed.aim_id = ros_cn.aim_id;
            ac_feed.now_rfid = ros_cn.now_rfid;
            as_->publishFeedback(ac_feed);
        }
        // printf("get action:%d\n",ros_cn.aim_action);
        //ACTION为3或4则直接进入升降模式。
        if(ros_cn.aim_action == 3 ||  ros_cn.aim_action == 4)
        {
            ros_cn.step = ac_UpDown;
        }
        else if(ros_cn.aim_action == 1)
        {
            ros_cn.step = ac_Straight;
            ros_cn.aim_dir = 0;
        }
        else if(ros_cn.aim_action == 2)
        {
            ros_cn.step = ac_Straight;
            ros_cn.aim_dir = 1;
        }

        //方向赋值
        if(ros_cn.aim_dir == 0)
        {
            ros_cn.magnetic_data = ros_cn.magnetic_front_data;
            ros_cn.magnetic_num = ros_cn.magnetic_front_num;
        }
        else
        {
            ros_cn.magnetic_data = ros_cn.magnetic_back_data;
            ros_cn.magnetic_num = ros_cn.magnetic_back_num;
        }

        switch (ros_cn.step)
        {
            case ac_Straight:  //直行
            {
                bool res = Go_Straight(Cmd_Vel,ros_cn.aim_dir,0.1,HZ_1S*2);
                if(res == false)  //error
                {
                    Cmd_Vel.linear.x = 0.0;
                    Cmd_Vel.angular.z = 0.0; 
                    cmd_vel_pub.publish(Cmd_Vel);

                    ac_result.result = ros_cn.err_num;
                    ac_result.now_rfid = ros_cn.now_rfid;
                    as_->setPreempted(ac_result);
                    return;
                }
                break;
            }
            case ac_Stop:       //停车
            {
                bool res = Do_Action(Cmd_Vel,ros_cn.aim_action,HZ_1S);
                if(res)
                {
                    ac_result.result = 0;
                    ac_result.now_rfid = ros_cn.now_rfid;
                    as_->setSucceeded(ac_result);
                    return;
                }
                break;
            }
            case ac_Turn_Left:  //左转
            {
                bool res = Turn_Left(Cmd_Vel,ros_cn.aim_dir,0.3,HZ_1S);
                if(res)
                {
                    ros_cn.step = ac_Straight;
                }
                break;
            }
            case ac_Turn_Right:  //右转
            {
                bool res = Turn_Right(Cmd_Vel,ros_cn.aim_dir,0.3,HZ_1S);
                if(res)
                {
                    ros_cn.step = ac_Straight;
                }
                break;
            }
            case ac_Add_Cut:  //加减速
            {
                break;
            }
            case ac_Error: //错误卡片
            {
                Cmd_Vel.linear.x = 0.0;
                Cmd_Vel.angular.z = 0.0; 
                cmd_vel_pub.publish(Cmd_Vel);

                ros_cn.err_num = er_Get_ErrorID;
                ac_result.result = ros_cn.err_num;
                ac_result.now_rfid = ros_cn.now_rfid;
                as_->setPreempted(ac_result);
                return;
                
                break;
            }
            case ac_UpDown: //升降台控制 3:up 4:low
            {
                bool res = Up_Down(ros_cn.aim_action);
                if(res)
                {
                    ac_result.result = 0;
                    ac_result.now_rfid = ros_cn.now_rfid;
                    as_->setSucceeded(ac_result);
                    return;
                }
                break;
            }
            default:{break;}
        }

        //防止两个话题同时控制车子，导致串口数据连帧
        if(ros_cn.pub_litf)
        {
            ros_cn.pub_litf = false;
        }
        else
        {
            cmd_vel_pub.publish(Cmd_Vel);
        }
        
        ros_cn.last_step = ros_cn.step;
        // printf("ros_cn.step:%d  value:%d  num:%d\n",ros_cn.step,ros_cn.magnetic_data,ros_cn.magnetic_num);

        ros::spinOnce();
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
    }
}

/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      支持多个master同时连接
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
void Action_Service::recieveMessages()
{
    printf("action init!\n");
    ros::NodeHandle n;
    ros::NodeHandle private_nh("~");

    ros::Subscriber  odom_sub = n.subscribe("/odom",10,&Action_Service::odom_callback,this);
    ros::Subscriber  magnetic_can_front_sub = n.subscribe("/magnetic_can_front",10,&Action_Service::magnetic_can_front_callback,this);
    ros::Subscriber  magnetic_can_back_sub = n.subscribe("/magnetic_can_back",10,&Action_Service::magnetic_can_back_callback,this);
    ros::Subscriber  rfid_sub = n.subscribe("/rfid",10,&Action_Service::rfid_callback,this);
    ros::Subscriber  lifting_sub = n.subscribe("/motor/limit_status",10,&Action_Service::limit_status_callback,this);

    //速度控制发布
    cmd_vel_pub    = n.advertise<geometry_msgs::Twist>("cmd_vel", 10);
    lift_table_pub = n.advertise<std_msgs::Float32>("/motor/speed_percent", 10);

    //构建一个action服务，第二个参数是服务的名称，客户端需要根据这个唯一的名称进行连接
    //最后一个参数表示是否构建完成之后就开始运行，一般应该设置为false，并在构建完成之后使用start()方法开始
    // create a server
    as_ = new Server(n, "ros_magnetic_nav_service", boost::bind(&Action_Service::actioncb, this, _1),false);
    as_->start();

    printf("get config:%s\n",config_file.c_str());

    ros::spin();
    
}
