#include "ros_robot_control.h"
//MQTTV3.35.1
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <iostream> /* cout */
#include <unistd.h>/* gethostname */
#include <netdb.h> /* struct hostent */
#include <net/if.h>
#include <sys/ioctl.h>

//action
#include <actionlib/client/simple_action_client.h>
#include <scan_icp_matcher/action1Action.h>
#include <ros_magnetic_nav_service/action1Action.h>
#include <apriltag_tracker/action1Action.h>

// #define LASER_CHARGE //开启则使用激光回冲，不开启为红外红外回冲

typedef actionlib::SimpleActionClient<scan_icp_matcher::action1Action> Client1;
typedef actionlib::SimpleActionClient<ros_magnetic_nav_service::action1Action> Client2;
typedef actionlib::SimpleActionClient<apriltag_tracker::action1Action> Client3;
//
Client1 *client_scan_icp_ptr;
Client2 *client_magnetic_nav_ptr;
Client3 *client_tracker_start_ptr;

/************************************************************************
函数名称：   bool get_run_nodel(const char *proc_name)
函数功能：   查找节点函数
函数参数：   节点名字
函数返回：   节点是否运行
************************************************************************/
void get_run_nodel(void)
{
    FILE *fp;
    if((fp = popen("rosnode list", "r")) != NULL)
    {  
        memset(result_buf,'\0',sizeof(result_buf));
        fread( result_buf, sizeof(char), sizeof(result_buf),  fp);  //将刚刚FILE* stream的数据流读取到buf中
    }
    pclose(fp);
}

bool get_run_nodel_statu(const char *proc_name)
{
    if(strstr(result_buf,proc_name) != NULL)
    {
        return true;
    }
    return false;
}

/************************************************************************
函数名称：   void kill_nodel(void)
函数功能：   关闭除某些特定节点
函数参数：   无
函数返回：   无
************************************************************************/
void kill_nodel(void)
{
    FILE *fp;
    char cmd[200] = {'\0'};
    //关闭除某些特定节点
    sprintf(cmd, "rosnode kill $(rosnode list | grep -v /ros_robot_control_node | grep -v /rosbridge_websocke | grep -v /cam_flask)");
    if((fp = popen(cmd, "r")) != NULL)
    {
         
    }
    pclose(fp);
}

/************************************************************************
函数名称：   void kill_anny_nodel(const char *proc_name)
函数功能：   关闭特定节点
函数参数：   无
函数返回：   无
************************************************************************/
void kill_anny_nodel(const char *proc_name)
{
    FILE *fp;
    char cmd[200] = {'\0'};
    //关闭除某些特定节点
    sprintf(cmd, "rosnode kill %s",proc_name);
    if((fp = popen(cmd, "r")) != NULL)
    {
         
    }
    pclose(fp);
}

/************************************************************************
函数名称：   void *client_fun(void *arg)
函数功能：   进程id返回函数。
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
pid_t getProcessPidByName(const char *proc_name)
{
     FILE *fp;
     char buf[256];
     char cmd[200] = {'\0'};
     pid_t pid = -1;
     sprintf(cmd, "pidof %s", proc_name);
     if((fp = popen(cmd, "r")) != NULL)
     {
         if(fgets(buf, 255, fp) != NULL)
         {
             pid = atoi(buf);
         }
     }
     //printf("pid = %d\n", pid);
     pclose(fp);
     return pid;
}

/************************************************************************
函数名称：   char *getWfiDEVICEName(void)
函数功能：   返回无线网卡名字。
函数参数：   已连接套接字
函数返回：   无线网卡名字
************************************************************************/
void getWfiDEVICEName(char *name)
{
     FILE *fp;
     char buf[200];
     char cmd[200] = {'\0'};
     char *dev;
     //char name[20]={0};
     if((fp = popen("nmcli device status", "r")) != NULL)
     {
         while(fgets(buf, sizeof(buf), fp) != NULL)
         {
             if(strstr(buf,"wifi") != NULL)
             {
                 dev = strtok(buf," ");
                 memcpy(name, dev, strlen(dev));
                 //printf("%s",name);
             } 
         }
     }
     pclose(fp);
}

bool ConnectWif(char *name,char *password)
{
    FILE *fp;
    char buf[200];
    char cmd[200] = {'\0'};
    //断开连接
    //getWfiDEVICEName();
    sleep(1);
    //连接wifi
    sprintf(cmd, "nmcli dev wifi connect '%s' password '%s'", name,password);
    if((fp = popen(cmd, "r")) != NULL)
    {
        while(fgets(buf, sizeof(buf), fp) != NULL)
        {
             
             if(strstr(buf,"激活了") != NULL)
             {
                 printf("%s connect successful\n",name);
                 return true;
             } 
        }
    }
    printf("%s connect false\n",name);
    return false;
}

/************************************************************************
函数名称：   getCpuUse(CPU_OCCUPY *o, CPU_OCCUPY *n)
函数功能：   获取cpu占用率
函数参数：   none
函数返回：   cpu占用率
************************************************************************/
// 定义一个cpu occupy的结构体，用来存放CPU的信息
typedef struct CPUPACKED
{
    char name[20];       //定义一个char类型的数组名name有20个元素
    unsigned int user;   //定义一个无符号的int类型的user
    unsigned int nice;   //定义一个无符号的int类型的nice
    unsigned int system; //定义一个无符号的int类型的system
    unsigned int idle;   //定义一个无符号的int类型的idle
    unsigned int lowait;
    unsigned int irq;
    unsigned int softirq;
} CPU_OCCUPY;

// 该函数，利用上述公式。计算出两段时间的之间的CPU占用率
// 输入为：当前时刻和上一采样时刻cpu的信息
double getCpuUse(CPU_OCCUPY *o, CPU_OCCUPY *n)
{
    unsigned long od, nd;
    od = (unsigned long)(o->user + o->nice + o->system + o->idle + o->lowait + o->irq + o->softirq); //第一次(用户+优先级+系统+空闲)的时间再赋给od
    nd = (unsigned long)(n->user + n->nice + n->system + n->idle + n->lowait + n->irq + n->softirq); //第二次(用户+优先级+系统+空闲)的时间再赋给od
    double sum = nd - od;
    double idle = n->idle - o->idle;
    return (sum - idle) / sum;
}

//获得CPU占用百分比
int get_cpu_use(void)
{
        static CPU_OCCUPY old_cpu_occupy;
        CPU_OCCUPY cpu_occupy;
        FILE *fd;       // 定义打开文件的指针
        char buff[256]; // 定义个数组，用来存放从文件中读取CPU的信息
        int cpu_use = 0.0;
        fd = fopen("/proc/stat", "r");

        if (fd != NULL)
        {
            // 读取第一行的信息，cpu整体信息
            fgets(buff, sizeof(buff), fd);
            if (strstr(buff, "cpu") != NULL) // 返回与"cpu"在buff中的地址，如果没有，返回空指针
            {
                // 从字符串格式化输出
                sscanf(buff, "%s %u %u %u %u %u %u %u", cpu_occupy.name, &cpu_occupy.user, &cpu_occupy.nice, &cpu_occupy.system, &cpu_occupy.idle, &cpu_occupy.lowait, &cpu_occupy.irq, &cpu_occupy.softirq);
                // cpu的占用率 = （当前时刻的任务占用cpu总时间-前一时刻的任务占用cpu总时间）/ （当前时刻 - 前一时刻的总时间）
                cpu_use = getCpuUse(&old_cpu_occupy, &cpu_occupy)*100 ;
                old_cpu_occupy = cpu_occupy;
            }
            fclose(fd);
        }
        //std::cout << cpu_use << std::endl; // 打印cpu的占用率
        return cpu_use;
}

/************************************************************************
函数名称：   run_move_base
函数功能：   move_base包打开函数以及log保存进程
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_move_base(void *arg)
{

    std::string str="roslaunch " + move_base_filename;
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    if(status < 0)
    {
        //ROS_ERROR("movebase error: %s", strerror(errno));
        PLOG_ERROR<<"movebase error:"<< strerror(errno);
        pthread_detach(thread_move_base); // 线程分离，结束时自动回收资源
    }
}

/************************************************************************
函数名称：   run_slam_karto
函数功能：   run_slam_karto包打开函数以及log保存进程
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_slam_karto(void *arg)
{
    std::string str;
    if(add_slam_mode == false)
    {
        str="roslaunch "  + slam_launch_filename;
    }
    else //add map
    {
        str="roslaunch "  + slam_add_launch_filename;
    }
    const char *p = str.c_str();//同上，要加const或者等号右边用char*
    int status = system(p);
    if(status < 0)
    {
        //ROS_ERROR("zkwl robot error: %s", strerror(errno));
        PLOG_ERROR<<"zkwl robot error:"<<   strerror(errno);
        pthread_detach(thread_slam_karto); // 线程分离，结束时自动回收资源
    }
}

/************************************************************************
函数名称：   run_slam_karto
函数功能：   run_slam_karto包打开函数以及log保存进程
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
int System_Check(int result)
{
    if((-1 != result) && (WIFEXITED(result)) && (!(WEXITSTATUS(result))))
        return 0;
    else
        return -1;
}

/************************************************************************
函数名称：   run_save_map
函数功能：   保存地图
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_save_map(void *arg)
{
    std::string str="bash " + save_map_sh_filename;
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    //判断执行完成
    if(!System_Check(status))
    {
         //保存成功
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("save_map_sucess");
        //send
        send_que.push(Json_Feedback);
    }
    else
    {
        //ROS_ERROR("save map error: %s", strerror(errno));
        PLOG_ERROR<<"save map error:"<< strerror(errno);
        //保存失败
        json_feedback["cmd_type"] = Json::Value("feedback");
      	json_feedback["cmd"] = Json::Value("save_map_error");
      	//send
      	send_que.push(Json_Feedback);
        pthread_detach(thread_save_map); // 线程分离，结束时自动回收资源
    }
}

/************************************************************************
函数名称：   run_backup_map
函数功能：   备份地图
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_backup_map(void *arg)
{
    std::string str="bash " + backup_map_sh_filename;
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    //判断执行完成
    if(!System_Check(status))
    {
        PLOG_ERROR<<"backup map sucess";
    }
    else
    {
        PLOG_ERROR<<"backup map error";
        pthread_detach(thread_backup_map); // 线程分离，结束时自动回收资源
    }
}

/************************************************************************
函数名称：   run_restore_map
函数功能：   还原地图
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_restore_map(void *arg)
{
    std::string str="bash " + restore_map_sh_filename;
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    //判断执行完成
    if(!System_Check(status))
    {
        PLOG_ERROR<<"restore map sucess";
    }
    else
    {
        PLOG_ERROR<<"restore map error";
        pthread_detach(thread_restore_map); // 线程分离，结束时自动回收资源
    }
}

/************************************************************************
函数名称：   run_refining_map
函数功能：   细化地图
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_refining_map(void *arg)
{
    // std::string str="python " + refining_map_py_filename;
    std::string str="bash " + refining_map_py_filename;
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    //判断执行完成
    if(!System_Check(status))
    {
        PLOG_INFO<<"refining map sucess";
        //保存成功
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("refining_map_sucess");
        //send
        send_que.push(Json_Feedback);
    }
    else
    {
        PLOG_ERROR<<"refining map error:"<< strerror(errno);
        //保存失败
        json_feedback["cmd_type"] = Json::Value("feedback");
      	json_feedback["cmd"] = Json::Value("refining_map_error");
      	//send
      	send_que.push(Json_Feedback);
        pthread_detach(thread_refining_map); // 线程分离，结束时自动回收资源
    }
}


void *run_set_location(void *arg)
{
    int id = *(int *)arg;
    if(id < 0){id = 0;}
    std::string str_id=std::to_string(id);
    std::string str="rosrun scan_icp_matcher pattern_recorder _output_file:=${HOME}/catkin_ws/src/scan_icp_matcher/pcd/pattern_" +  str_id + ".pcd ";
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    //判断执行完成
    if(!System_Check(status))
    {
         //保存成功
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("set_location_sucess");
        //send
        send_que.push(Json_Feedback);
    }
    else
    {
        //ROS_ERROR("save map error: %s", strerror(errno));
        PLOG_ERROR<<"set location error:"<< strerror(errno);
         //保存失败
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("set_location_error");
        //send
        send_que.push(Json_Feedback);
        pthread_detach(thread_set_location); // 线程分离，结束时自动回收资源
    }
}

// 二维码
void run_set_qr_location(int id)
{
    apriltag_tracker::save_tag tracker_save_srv;
    tracker_save_srv.request.id = id;
    if (tracker_save_client.call(tracker_save_srv))//服务调用
    {
        if(tracker_save_srv.response.result > 0)
        {
            PLOG_INFO<<"save id finish";
            //保存成功
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("set_qr_location_sucess");
            //send
            send_que.push(Json_Feedback);
        }
        else
        {
            PLOG_ERROR<<"save id error";
            //保存失败
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("set_qr_location_error");
            //send
            send_que.push(Json_Feedback);
        }  
    }
    else
    {
        PLOG_ERROR<<"save id error";
         //保存失败
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("set_qt_location_error");
        //send
        send_que.push(Json_Feedback);
    }
}

// 二维码
void  start_qr_Location(int id)
{
   
   task_feedback_set("qr_location",id,"","start",0,0,0,0,0,0);
   set_client_tracker_start_goal(id, true);

    base_status.robot.last_status = base_status.robot.status;
    base_status.robot.status = Mod_Qr_Location;
}

//start location 
//task_mode 0:对准， 1:退后
void  start_more_task_qr_Location(int id)
{
    Json::Reader reader;
    //clear
    //json_qr_pose.clear();
    std::ifstream ifs(qr_location_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_qr_pose))
    {
        PLOG_ERROR<<"start qr_location error";
        ifs.close();

        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 0;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_qr_pose["point"].size() == 0)
    {
        //ROS_ERROR("error have no location point");
        PLOG_ERROR<<"error have no qr_location point";
        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 0;
        base_status.robot.status = Mod_Error;
        return;
    }

   // 二维码
   start_qr_Location(id);
   base_status.robot.last_status = base_status.robot.status;
   base_status.robot.status = Mod_More_Task;
}

// 二维码
void stop_qr_Location(void)
{
    client_tracker_start_ptr -> cancelGoal();
    set_client_tracker_start_goal(-1,false);
}

/************************************************************************
函数名称：   run_updata
函数功能：   系统更新重启
函数参数：   已连接套接字
函数返回：   无
************************************************************************/
void *run_updata(void *arg)
{
    std::string str="bash " + updata_sh_filename;
    const char *p = str.c_str();//同上，要加const或者等号右边用char*

    int status = system(p);
    if(status < 0)
    {
        //ROS_ERROR("updata error: %s", strerror(errno));
        PLOG_ERROR<<"updata error:" << strerror(errno);
        pthread_detach(thread_updata); // 线程分离，结束时自动回收资源
    }
}


double double_to_3double(double value)
{
    int temp = (value + 0.0005) * 1000;
    return temp / 1000.0;
}

//导航取消函数
void move_base_cancel()
{
    //nav.path_mode_flag = false;
    actionlib_msgs::GoalID cancel_goal;
    movebase_cancel_pub.publish(cancel_goal);
    base_status.robot.status = Mod_Free;
    //关闭定时器，防止关闭任务前在定时器开启了下一个任务
    movebase_timer.stop();
     //设置轨迹模式false
    ros::param::set("/move_base/TebLocalPlannerROS/trajectory_mode",false);
    PLOG_WARNING<<"set trajectory_mode false";
}

//发布导航目标点信息
void set_goal(std::string frame,double x,double y,double z,double w)
{
     //非轨道模式
    if(nav.path_mode_flag == false)
    {
        //设置轨迹模式false
        ros::param::set("/move_base/TebLocalPlannerROS/trajectory_mode",false);
        PLOG_WARNING<<"set trajectory_mode false";
    }

    //clear costmap
    std_srvs::Empty srv;
    if (clear_costmaps_client.call(srv))//服务调用
    {
        //ROS_INFO("clear costmap");
        PLOG_INFO<<"clear costmap";

    }
    //加1S延时，解决刚清理代价地图，导师为空的情况。
    ros::Duration(1.0).sleep();

    geometry_msgs::PoseStamped goal;
    //设置frame
    goal.header.frame_id= frame;
    //设置时刻
    goal.header.stamp=ros::Time::now();
    goal.pose.position.x= x;
    goal.pose.position.y= y;
    goal.pose.position.z= 0;
    goal.pose.orientation.z= z;
    goal.pose.orientation.w= w;

    //轨道模式
    if(nav.path_mode_flag == true)
    {
        goal1_pub.publish(goal);
        last_goal.pose.position.x = goal.pose.position.x;
        last_goal.pose.position.y = goal.pose.position.y;
        last_goal.pose.orientation.z = goal.pose.orientation.z;
        last_goal.pose.orientation.w = goal.pose.orientation.w;
    }
    //巡航模式
    else
    {
        goal_pub.publish(goal);
        last_goal.pose.position.x = goal.pose.position.x;
        last_goal.pose.position.y = goal.pose.position.y;
        last_goal.pose.orientation.z = goal.pose.orientation.z;
        last_goal.pose.orientation.w = goal.pose.orientation.w;
    }
    ros::spinOnce();
}

void set_trajectory_goal(void)
{
    //设置轨迹模式true
    ros::param::set("/move_base/TebLocalPlannerROS/trajectory_mode",true);

    //clear costmap
    std_srvs::Empty srv;
    if (clear_costmaps_client.call(srv))//服务调用
    {
        //ROS_INFO("clear costmap");
        PLOG_INFO<<"clear costmap";
    }
    else
    {
        PLOG_ERROR<<"call clear costmap error";
    }
    //加1S延时，解决刚清理代价地图，导师为空的情况。
    //ros::Duration(1.0).sleep();
    sleep(1);

    geometry_msgs::PoseStamped goal;
    //设置frame
    goal.header.frame_id= "map";
    //设置时刻
    goal.header.stamp=ros::Time::now();
    goal.pose.position.x= last_goal.pose.position.x;
    goal.pose.position.y= last_goal.pose.position.y;
    goal.pose.position.z= 0;
    goal.pose.orientation.z = last_goal.pose.orientation.z;
    goal.pose.orientation.w= last_goal.pose.orientation.w;
    goal_pub.publish(goal);

    PLOG_INFO<<"plan the zero goal";

    ros::spinOnce();
}


//mode 0:off charger , 1:on charge
static void charge_control(u8 mode)
{
    std_msgs::UInt8 status;
    status.data = mode;
    if(mode == 0)
    {
        agv_go_charge_pub.publish(status);
        go_charge_pub.publish(status);
        four_go_charge_pub.publish(status);
        #ifdef LASER_CHARGE
        set_client_scan_icp_goal(1,8081);  //退出激光回充
        #endif
        PLOG_WARNING<<"exit charge !";
        cout << "--------------------------------------------------------------------" << endl;
    }
    else
    {
        #ifdef LASER_CHARGE
        status.data = 2;
        set_client_scan_icp_goal(0,8081); //开启激光回充
        #endif
    }
    agv_go_charge_pub.publish(status);
    go_charge_pub.publish(status);
    four_go_charge_pub.publish(status);
}

//回充
void goto_power_point(void)
{
    //ROS_WARN("go to power!");
    PLOG_WARNING<<"go to power!";
    cout << "--------------------------------------------------------------------" << endl;
    
}

/***************************************回调********************************************************************/
//timer out deal处理点停顿时间到了发送下一个点
void timer_handler(const ros::TimerEvent& e )
{
    switch(base_status.robot.status)
    {
        case Mod_Order_Interest:  //顺序巡航模式
        {
            if(json_interest_point["point"].size() == 0)
	    {
	     	//ROS_ERROR("error have no interest point");
            PLOG_ERROR<<"error have no interest point";
	     	base_status.robot.main_error = Mod_Order_Interest;
	      	base_status.robot.sub_error  = 2;
	     	base_status.robot.status = Mod_Error;
	     	return;
	    }

            Order_Interest_index += 1;
            //循环
            if(Order_Interest_index >= json_interest_point["point"].size())
            {
                Order_Interest_index = 0;
            }
            geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Order_Interest_index]["z"].asDouble());
            set_goal("map",json_interest_point["point"][Order_Interest_index]["x"].asDouble(),json_interest_point["point"][Order_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
            //成功反馈
            task_feedback_set("nav",Order_Interest_index,"","start",last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               
            break;
        }
        case Mod_Random_Interest:  //随机巡航模式
        {
	    if(json_interest_point["point"].size() == 0)
	    {
	     	//ROS_ERROR("error have no interest point");
            PLOG_ERROR<<"error have no interest point";
	     	base_status.robot.main_error = Mod_Order_Interest;
	      	base_status.robot.sub_error  = 2;
	     	base_status.robot.status = Mod_Error;
	     	return;
	    }

            Random_Interest_index = rand() % json_interest_point["point"].size();
            geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Random_Interest_index]["z"].asDouble());
            set_goal("map",json_interest_point["point"][Random_Interest_index]["x"].asDouble(),json_interest_point["point"][Random_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w); 
            task_feedback_set("nav",Random_Interest_index,"","start",last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            break;
        }
        case Mod_Trajectory:  //循轨迹
        {
            switch(track.finish_goal)
            {
                case 0:   //错误，没有路径点
                {
                    break;
                }
                case 1:   //取消
                {
                    break;
                }
                case Forward_Direction:   //正向运行
                {
                    track.dir_flag = true;
                    start_track_first(track.start_index,track.dir_flag);
                    break;
                }
                case Reverse_Direction:   //逆向运行
                {
                    track.dir_flag = false;
                    start_track_first(track.start_index,track.dir_flag);
                    break;
                }
                case 4: //超时
                {
                    break;
                }
                case 5: //到达起始路径点
                {
                    if(track.dir_flag == false)
                    {
                        track.finish_goal = 2;
                        start_track(track.start_index,Forward_Direction);
                        
                    }
                    else
                    {
                        track.finish_goal = 3;
                        start_track(track.start_index,Reverse_Direction);
  
                    }
                    
                    break;
                }
            }
            break;
        }
        case Mod_More_Task:  //多任务模式
         {
              if(json_more_task["task"].size() == 0)
      	      {
      	          //ROS_ERROR("error have no task point");
                  PLOG_ERROR<<"error have no task point";
      	          base_status.robot.main_error = Mod_More_Task;
      	          base_status.robot.sub_error  = 7;
      	          base_status.robot.status = Mod_Error;
      	          return;
      	      }

              //循迹到达起始点
              if(track.finish_goal == 5)
              {
                  int id = json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt();
                  int dir = 0;
                  if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                  {
                      dir = 2;
                  }
                  else  //back
                  {
                      dir = 3;
                  }
                  start_more_task_track(id, dir);
                  track.finish_goal = 0;
              }
              //完成一个子任务
              else
              {
                  //如果子任务循环
                  if(more_task.sub_loop == true)
                  {
                      more_task.sub_task++;
                      //循环
                      if(more_task.sub_task >= json_more_task["task"][more_task.main_task].size())
                      {
                          more_task.sub_task = 0;
                      }
                      //nav point
                      if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                              start_more_task_interest_point(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                          }
                      }
                      //track point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "track")
                      {
                          int id = json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt();
                          int dir = 0;
                          if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                          {
                              dir = 2;
                          }
                          else  //back
                          {
                              dir = 3;
                          }
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                              start_more_task_track_first(id,dir);
                          }
                      }
                       //location point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "loc")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                     start_more_task_Location(0,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                                else  //back
                                {
                                    start_more_task_Location(1,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                          }
                      }
                      // 二维码
                      // qr_location point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "qr")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                    start_more_task_qr_Location(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                                else  //back
                                {
                                    stop_qr_Location();
                                }
                          }
                      }
                  }
                  //如果主任务循环
                  else if(more_task.main_loop == true)
                  {
                    PLOG_WARNING<< "main loop";
                      more_task.sub_task++;
                      //循环
                      if(more_task.sub_task >= json_more_task["task"][more_task.main_task].size())
                      {
                          more_task.sub_task = 0;
                          more_task.main_task++;
                          ////循环
                          if(more_task.main_task >= json_more_task["task"].size())
                          {
                              more_task.main_task = 0;
                          }
                      }
                      //nav point
                      if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                              start_more_task_interest_point(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                          }
                      }
                      //track point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "track")
                      {
                          int id = json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt();
                          int dir = 0;
                          if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                          {
                              dir = 2;
                          }
                          else  //back
                          {
                              dir = 3;
                          }
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                              start_more_task_track_first(id,dir);
                          }
                      }
                       //location point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "loc")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                        start_more_task_Location(0,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                                else  //back
                                {
                                    start_more_task_Location(1,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                          }
                      }
                      // 二维码
                      // qr_location point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "qr")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                                PLOG_WARNING<< "main loop1";
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                    start_more_task_qr_Location(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                                else  //back
                                {
                                    stop_qr_Location();
                                }
                          }
                      }
                  }
                  //不循环
                  else
                  {
                      more_task.sub_task++;
                      //不循环
                      if(more_task.sub_task >= json_more_task["task"][more_task.main_task].size())
                      {
                          stop_more_task();
                          base_status.robot.status = Mod_Free;
                          return;
                      }
                      //nav point
                      if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                              start_more_task_interest_point(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                          }
                      }
                      //track point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "track")
                      {
                          int id = json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt();
                          int dir = 0;
                          if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                          {
                              dir = 2;
                          }
                          else  //back
                          {
                              dir = 3;
                          }

                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                              start_more_task_track_first(id,dir);
                          }
                      }
                       //location point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "loc")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                     start_more_task_Location(0,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                                else  //back
                                {
                                    start_more_task_Location(1,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                          }
                      }
                      // 二维码
                      // qr_location point
                      else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "qr")
                      {
                          //低电量回充
                          if(more_task.low_power_flag)
                          {
                              start_GoCharge(setting.go_charge_error_time);
                          }
                          else
                          {
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                    start_more_task_qr_Location(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                }
                                else  //back
                                {
                                    stop_qr_Location();
                                }
                          }
                      }
                  }
              }
              break;
         }
         case Mod_More_Pause:  //多任务暂停模式
         {
              break;
         }
          
    }
    movebase_timer.stop();
    //ROS_WARN("go  timer !");
    PLOG_WARNING<<"go  timer !";
    cout << "--------------------------------------------------------------------" << endl;
}

//定位超时处理
void reset_pose_timer_hanlde(const ros::TimerEvent& e)
{
     //cannecl reset pose
    pub_reset_pose(false);
    reset_pose_timer.stop();
    base_status.robot.doing_reset = false;

    json_feedback["cmd_type"] = Json::Value("feedback");
    json_feedback["cmd"] = Json::Value("reset_failure");
    //send
    send_que.push(Json_Feedback);

}

//轨迹避障停车超时处理
void track_stop_timer_hanlde(const ros::TimerEvent& e)
{
    switch(base_status.robot.status)
    {
        case Mod_Free:  //空闲模式下
        {
            break;
        }
        case Mod_Nav:  //导航模式下
        {
                //避障停车超时反馈
            task_feedback_set("point",0,"","timeout",
                                                    last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            break;
        }
        case Mod_Order_Interest:  //顺序巡航模式
        {
            //避障停车超时反馈
            task_feedback_set("nav",Order_Interest_index,"","timeout",
                                                    last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            break;
        }
        case Mod_Random_Interest:  //随机巡航模式
        {
            task_feedback_set("nav",Random_Interest_index,"","timeout",
                                                    last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            break;
        }
        case Mod_Charge:  //回冲模式
        {
            task_feedback_set("charge",0,"","timeout",
                                                    last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            break;
        }
        case Mod_Trajectory:  //轨迹模式
        {
            if(track.dir_flag == false)
            {
                task_feedback_set("track",track.start_index,"forward","timeout",
                                                            last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            }
            else
            {
                task_feedback_set("track",track.start_index,"back","timeout",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            }
            break;
        }
        case Mod_More_Task:  //多任务模式 到定时处理
        {
            //读取任务
            if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
            {

                task_feedback_set("nav",json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt(),"","timeout",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            }
            else
            {
                task_feedback_set("track",json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt(),"","timeout",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
            }
            break;
        }
        default:break;
    }
    track_stop_timer.stop();
    PLOG_WARNING<<"track stop time out!";
}

//icp匹配状态订阅
void icp_status_agvCallback(const std_msgs::Float32 &status)
{
    base_status.local.amcl = status.data;
}


//rosout回调主要是订阅cato的定位匹配结果
void rosout_Callback(const rosgraph_msgs::LogConstPtr &msg)
{
    switch(msg->level)
    {
        case 1:     //DEBUG
        {
            break;
        }
        case 2:     //INFO
        {
            break;
        }
        case 4:     //WARN
        {
            //查找到matches表示匹配成功
            if(msg->name == "/cartographer_node" && msg->msg.find("matches") != -1)
            {
                if(base_status.local.carto == false)
                {
                    sleep(1);
                    //cannecl reset pose
                    pub_reset_pose(false);

                    //关闭超时定时
                    reset_pose_timer.stop();
                    base_status.robot.doing_reset = false;
                    
                    //重定位成功标志
                    json_feedback["cmd_type"] = Json::Value("feedback");
                    json_feedback["cmd"] = Json::Value("reset_successful");
                    //send
                    send_que.push(Json_Feedback);
                }
                
                //标记carto定位成功
                base_status.local.carto = true;
                
            }
            /*
            //查找到matches表示匹配成功
            if(msg->name == "/cartographer_node" && msg->msg.find("localization_true") != -1)
            {
               
                //cannecl reset pose
                pub_reset_pose(false);

                //关闭超时定时
                reset_pose_timer.stop();
                base_status.robot.doing_reset = false;
                    
                //重定位成功标志
                json_feedback["cmd_type"] = Json::Value("feedback");
                json_feedback["cmd"] = Json::Value("reset_successful");
                //send
                send_que.push(Json_Feedback);

                //标记carto定位成功
                base_status.local.carto = true;
                
            }*/
            else if(msg->name == "/cartographer_node" && msg->msg.find("global_localization_false") != -1)
            {
                //之前为定位准确，现在定位丢失，则反馈定位丢失 不在建图模式下
                if(base_status.local.carto == true && base_status.robot.status != Mod_Slam)
                {
                    //重定位成功标志
                    json_feedback["cmd_type"] = Json::Value("feedback");
                    json_feedback["cmd"] = Json::Value("location_false");
                    //send
                    send_que.push(Json_Feedback);
                }

                //标记carto定位false 不在建图模式下
                if(base_status.robot.status != Mod_Slam)
                {
                     base_status.local.carto = false; 
                }
               
                
            }
            else if(msg->name == "/move_base" && msg->msg.find("track") != -1)
            {
                //轨迹模式避障停车
                base_status.robot.main_error = base_status.robot.status;
                base_status.robot.sub_error  = 10;
            }
            break;
        }
        case 8:     //ERROR
        {
            break;
        }
        case 16:    //FATAL
        {
            break;
        }
    }
}


//特征板icp得分订阅定位订阅
void icp_fitness_scoreCallback(const std_msgs::Float64  &score)
{
  base_status.reflector.icp_socre = score.data;
}

//IMU订阅
void cmd_imuCallback(const sensor_msgs::Imu::ConstPtr &imu_msg)
{
    sensor_msgs::Imu temp_imu;
    if(imu_msg->linear_acceleration.z)
    {
        base_status.sensor.imu = true;
    }

    temp_imu.header                           = imu_msg->header;
    temp_imu.orientation                      = imu_msg->orientation;
    temp_imu.orientation_covariance           = imu_msg->orientation_covariance;
    temp_imu.angular_velocity.z                 = imu_msg->angular_velocity.z;
    temp_imu.angular_velocity_covariance      = imu_msg->angular_velocity_covariance;
    // temp_imu.linear_acceleration              = imu_msg->linear_acceleration;
     temp_imu.linear_acceleration.z             = 9.8; 
    temp_imu.linear_acceleration_covariance   = imu_msg->linear_acceleration_covariance;

    temp_imu.header.stamp = ros::Time::now(); 
    //去除IUM零飘
    if( abs(base_status.robot.vx) < 10 && abs(base_status.robot.vz) < 0.01)
    {
        temp_imu.angular_velocity.z = base_status.robot.vz;
    }
    imu_pub.publish(temp_imu);
}
//constraint_list订阅
void constraint_listCallback(const visualization_msgs::MarkerArray::ConstPtr &constraints)
{
    static int  diff_constraint = 0;
    if(constraints->markers.size() > 5)
    {
        //printf("diff trac constraint size1:%d size3:%d  size5:%d\n\n",constraints->markers[1].points.size(),constraints->markers[3].points.size(),constraints->markers[5].points.size());
        //不同轨迹的约束增长，且大于2，则可以判断为定位成功
        if(base_status.local.carto == false)
        {
             if(diff_constraint < constraints->markers[5].points.size() && constraints->markers[5].points.size() >= 4)
             {
                 //关闭超时定时
                 reset_pose_timer.stop();
                 base_status.robot.doing_reset = false;

                 //重定位成功标志
                 json_feedback["cmd_type"] = Json::Value("feedback");
                 json_feedback["cmd"] = Json::Value("reset_successful");
                 //send
                 send_que.push(Json_Feedback);
                 base_status.local.carto = true;
             }
        }
        else
        {
          
        }
        diff_constraint = constraints->markers[5].points.size();
    }
    
}

//
void trajectory_stopCallback(const std_msgs::UInt8 &status)
{
    static bool  track_stop_time_flag = false;
    if(status.data == 0)
    {
       if(base_status.robot.sub_error == 10 && 
          (base_status.robot.status ==Mod_Trajectory || base_status.robot.status == Mod_More_Task || 
            base_status.robot.status == Mod_Order_Interest ||   base_status.robot.status == Mod_Random_Interest ))
       {
           //轨迹模式避障停车
           base_status.robot.main_error = base_status.robot.status;
           base_status.robot.sub_error  = 0;
       }
       track_stop_timer.stop();
       track_stop_time_flag = false;
    }
    else
    {
        //不等于0开启避障定时
        if(track.track_stop_time != 0 && track_stop_time_flag  == false)
        {
            track_stop_timer.setPeriod(ros::Duration(track.track_stop_time),true);
            track_stop_timer.start();
            track_stop_time_flag = true;
            //ROS_WARN("track time start:%d",track.track_stop_time);
            PLOG_WARNING<<"track time start:"<<track.track_stop_time;
        }
        if(base_status.robot.status ==Mod_Trajectory || base_status.robot.status == Mod_More_Task || 
            base_status.robot.status == Mod_Order_Interest ||   base_status.robot.status == Mod_Random_Interest)
        {
             //轨迹模式避障停车
            base_status.robot.main_error = base_status.robot.status;
            base_status.robot.sub_error  = 10;
        }
       
    }
}

//BMS数据回调
void bms_infoCallback(const ros_bms_msg::bms::ConstPtr &status)
{
    base_status.bms.voltage            = status->voltage/10.0;
    base_status.bms.current            = status->current/10.0;
    base_status.bms.status             = status->status;
    base_status.bms.tem                = status->tem;
    base_status.bms.remaining_capacity = status->remaining_capacity;
    base_status.bms.error              = status->error;
    base_status.bms.soc                = status->soc;
}

//轨道路径点订阅
void half_traffic_pathCallback(const nav_msgs::Path &path)
{
    geometry_msgs::PoseArray track_point;
    for(int i=0;i<path.poses.size();i++)
    {
        track_point.poses.push_back(path.poses[i].pose);
    }
    trajectory_point_pub.publish(track_point);

    /*
    if(path.poses.size() && nav.path_mode_flag )
    {
         //设置轨迹模式
        //set_trajectory_goal();
         //设置轨迹模式true
        ros::param::set("/move_base/TebLocalPlannerROS/trajectory_mode",true);

        geometry_msgs::PoseStamped goal;
        //设置frame
        goal.header.frame_id= "map";
        //设置时刻
        goal.header.stamp=ros::Time::now();
        goal.pose.position.x= 0;
        goal.pose.position.y= 0;
        goal.pose.position.z= 0;
        goal.pose.orientation.z= 0;
        goal.pose.orientation.w= 1;
        goal_pub.publish(goal);
        PLOG_INFO<<"half traffic plan the zero goal "<<nav.path_mode_flag;
    }
    else
    {
        //ROS_ERROR(" half_traffic_path is empty! will not goto");
        PLOG_ERROR<<" half_traffic_path is empty! will not goto";
    }*/
    if(path.poses.size() && nav.path_mode_flag )
    {
       
    }
    else
    {
        //ROS_ERROR(" half_traffic_path is empty! will not goto");
        PLOG_ERROR<<" half_traffic_path is empty! will not goto";
    }
   
}
//轨道反馈订阅
void half_feedbackCallback(const std_msgs::String::ConstPtr &msg)
{
    //轨道规划失败
    if(msg->data == "false")
    {
        base_status.robot.main_error = base_status.robot.status;
        base_status.robot.sub_error  = 12;
        base_status.robot.status = Mod_Error;
          //失败尝试反馈
        task_feedback_set("nav",0,"","failure",last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
    }
}

//全局路径规划获得轨道反馈
void global_feedbackCallback(const std_msgs::Int8::ConstPtr &msg)
{
    if(nav.path_mode_flag )
    {
         //设置轨迹模式
        //set_trajectory_goal();
         //设置轨迹模式true
        ros::param::set("/move_base/TebLocalPlannerROS/trajectory_mode",true);

        geometry_msgs::PoseStamped goal;
        //设置frame
        goal.header.frame_id= "map";
        //设置时刻
        goal.header.stamp=ros::Time::now();
        goal.pose.position.x= last_goal.pose.position.x;
        goal.pose.position.y= last_goal.pose.position.y;
        goal.pose.position.z= 0;
        goal.pose.orientation.z = last_goal.pose.orientation.z;
        goal.pose.orientation.w= last_goal.pose.orientation.w;

        goal_pub.publish(goal);
        PLOG_INFO<<"half traffic plan the zero goal "<<nav.path_mode_flag;
    }
}

//gpio
void gpio_msgCallback(const ros_gpio_msg::gpio::ConstPtr &msg)
{
    if(msg->K1)
    {
        base_status.magnetic.material = false;
    }
    else
    {
        base_status.magnetic.material = true;
    }
}

//limitation_state
void limitation_stateCallback(const lifting_ros::limitation_state::ConstPtr &msg)
{
    if(msg->upper_limit_state)
    {
        base_status.magnetic.up = true;
    }
    else
    {
        base_status.magnetic.up = false;
    }

    if(msg->low_limit_state)
    {
        base_status.magnetic.low = true;
    }
    else
    {
        base_status.magnetic.low = false;
    }
}

//在空闲模式下防止多线程原因导致没法关闭导航
void movebase_statusCallback(const actionlib_msgs::GoalStatusArray::ConstPtr &status)
{
    if(base_status.robot.status == Mod_Free)
    {
        for(int i=0;i<status->status_list.size();i++)
        {
            if(status->status_list[i].status == 1)
            {
                //取消导航
                move_base_cancel();
                break;
            }
        }
          if(base_status.robot.sub_error == 10 )
          {
            base_status.robot.sub_error = 0;
          }
    }
}

//导航结果回调函数
void movebase_resultCallback(const move_base_msgs::MoveBaseActionResult::ConstPtr &result)
{
    double x,y,z,w;
    static unsigned char order_error = 0,random_error = 0,charge_error = 0,more_error = 0;

    //成功
    if(result->status.status == 3)
    {
        //test
        /*ROS_WARN("the error X:%.2f    Y:%.2f  Yaw:%.2f\n",fabs(last_goal.pose.position.x - base_status.pose.x),
                                                                                                                 fabs(last_goal.pose.position.y - base_status.pose.y),
                                                                                                                 fabs(tf2::getYaw(last_goal.pose.orientation) - base_status.pose.yaw));*/
        PLOG_WARNING<<"the goal err X:"<<fabs(last_goal.pose.position.x - base_status.pose.x)<<"    Y:"<<fabs(last_goal.pose.position.y - base_status.pose.y)<<"    Yaw:"<< fabs(tf2::getYaw(last_goal.pose.orientation) - base_status.pose.yaw);
       order_error = 0;
       random_error = 0;
       charge_error = 0;
       switch(base_status.robot.status)
       {
           case Mod_Free:  //空闲模式下
           {
                break;
           }
           case Mod_Nav:  //导航模式下
           {
                base_status.robot.status = Mod_Free;
                //成功反馈
                task_feedback_set("point",0,"","success",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                break;
           }
           case Mod_Order_Interest:  //顺序巡航模式
           {
               order_error = 0;
               //alarm(move_base_timer);
               if(nav.circul_flag)
               {
                   movebase_timer.start();
                   //ROS_WARN("will order go to next cruise point:%.1f",setting.move_base_timer);
                   PLOG_WARNING<<"will order go to next cruise point:"<<Order_Interest_index;
                   cout <<"准备顺序前往下一个巡航点:"<<Order_Interest_index<<endl;
                   cout << "--------------------------------------------------------------------" << endl;
               }
               else
               {
                  base_status.robot.status = Mod_Free;
                  //ROS_WARN("arrive the point,the task is done:%.1f",setting.move_base_timer);
                  PLOG_WARNING<<"arrive the point,the task is done:"<<Order_Interest_index;
                  cout << "--------------------------------------------------------------------" << endl;
               }

               //成功反馈
               task_feedback_set("nav",Order_Interest_index,"","success",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               break;
           }
           case Mod_Random_Interest:  //随机巡航模式
           {
               random_error = 0;
               //alarm(move_base_timer);
               if(nav.circul_flag)
               {
                 movebase_timer.start();

                 //ROS_WARN("will random go to next cruise point:%.1f",setting.move_base_timer);
                 PLOG_WARNING<<"will random go to next cruise point:"<<Random_Interest_index;
                 cout <<"准备随机前往下一个巡航点:"<<Random_Interest_index<<endl;
                 cout << "--------------------------------------------------------------------" << endl;
               }
               else
               {
                  base_status.robot.status = Mod_Free;
                  //ROS_WARN("arrive the point,the task is done:%.1f",setting.move_base_timer);
                  PLOG_WARNING<<"arrive the point,the task is done:"<<Random_Interest_index;
                  cout << "--------------------------------------------------------------------" << endl;
               }

               //成功反馈
               task_feedback_set("nav",Random_Interest_index,"","success",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               break;
           }
           case Mod_Charge:  //回冲模式
           {
               charge_error = 0;
               //on charge
               sleep(1);
               charge_control(1);
               Go_Charge_Flag = 0;
	
               //ROS_WARN("going charge!");
               PLOG_WARNING<<"going charge!";
               cout <<"开始回充！"<<endl;
               cout << "--------------------------------------------------------------------" << endl;

               //成功反馈只是到达回冲点，只能是过程成功
               task_feedback_set("charge",0,"","process",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               break;
           }
           case Mod_Trajectory:  //轨迹模式
           {
                bool trajectory_mode;
                ros::param::get("/move_base/TebLocalPlannerROS/trajectory_mode",trajectory_mode);

                //到达起点
                if(trajectory_mode == false)
                {
                  movebase_timer.setPeriod(ros::Duration(2.0),true);
                  movebase_timer.start();

                  //ROS_WARN("will do Trajectory task");
                  PLOG_WARNING<<"will do Trajectory task";
                  cout <<"到达起点准备循迹:"<<endl;
                  cout << "--------------------------------------------------------------------" << endl;

                  //到达轨迹起点
                  if(track.dir_flag == false)
                  {
                      task_feedback_set("track",track.start_index,"forward","process",
                                                                 last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                  }
                  else
                  {
                      task_feedback_set("track",track.start_index,"back","process",
                                                                last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                  }
                }
                //循迹结束
                else
                {
                    //轨迹模式 到定时处理
                    if(base_status.robot.status == Mod_Trajectory)
                    {
                        //轨迹循环
                        if(track.circul_flag == true)
                        {
                            movebase_timer.setPeriod(ros::Duration(2.0),true);
                            movebase_timer.start();
                        }
                        else
                        {
                            stop_track();
                        }
                    
                    }

                    //ROS_WARN("Trajectory task is success");
                    PLOG_WARNING<<"Trajectory task is success";
                    //成功反馈，轨迹运行完成
                    task_feedback_set("track",track.start_index,"forward","success",
                                                            last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                }
                
                
                break;
           }
           case Mod_More_Task:  //多任务模式 到定时处理
           {
                more_error = 0;
                movebase_timer.setPeriod(ros::Duration(2.0),true);
                movebase_timer.start();
                //ROS_WARN("will order go to next task");
                PLOG_WARNING<<"will order go to next task";
                cout <<"准备随机前往下一个任务:"<<endl;
                cout << "--------------------------------------------------------------------" << endl;
                PLOG_WARNING<<"will order go to next task";
                if(json_more_task["task"].size() == 0)
                {
                    //ROS_ERROR("error have no task point");
                    PLOG_ERROR<<"error have no task point";
                    base_status.robot.main_error = Mod_More_Task;
                    base_status.robot.sub_error  = 7;
                    base_status.robot.status = Mod_Error;
                    return;
                }

                //读取任务
                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
                {

                    task_feedback_set("nav",json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt(),"","success",
                                                            last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                }
                else
                {
                    task_feedback_set("track",json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt(),"","process",
                                                            last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                }
                break;
           }
           case Mod_More_Pause:  //多任务暂停模式
           {
                break;
           }
       }
    }
    //失败
    else if(result->status.status == 4)
    {
       switch(base_status.robot.status)
       {
           case Mod_Free:  //空闲模式下
           {
                base_status.robot.main_error = 0;
                base_status.robot.sub_error  = 0;
                break;
           }
           case Mod_Nav:  //导航模式下
           {
                base_status.robot.main_error = base_status.robot.status;
                base_status.robot.sub_error  = 0;
                base_status.robot.status = Mod_Free;
                //失败反馈
                task_feedback_set("point",0,"","failure",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                PLOG_ERROR<<"go to the point failure";
                break;
           }
           case Mod_Order_Interest:  //顺序巡航模式
           {
      	       if(json_interest_point["point"].size() == 0)
      	       {
      	     		//ROS_ERROR("error have no interest point");
                    PLOG_ERROR<<"error have no interest point";
      	     		base_status.robot.main_error = Mod_Order_Interest;
      	      		base_status.robot.sub_error  = 2;
      	     		base_status.robot.status = Mod_Error;
      	     		return;
      	       }

                //轨道模式下再次设置目标
               if(nav.path_mode_flag == true)
               {
                     bool trajectory_mode;
                    ros::param::get("/move_base/TebLocalPlannerROS/trajectory_mode",trajectory_mode);

                    if(trajectory_mode == true)
                    {
                        set_trajectory_goal();
                    }
                    return;
               }

               order_error++;
               if(order_error < 2)
               {
                   geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Order_Interest_index]["z"].asDouble());
                   set_goal("map",json_interest_point["point"][Order_Interest_index]["x"].asDouble(),json_interest_point["point"][Order_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                   //ROS_WARN("order error will again order go to cruise point:%.1f",setting.move_base_timer);
                   PLOG_WARNING<<"order error will again order go to cruise point:"<<Order_Interest_index;
                   cout <<"导航错误准备再次前往巡航点:"<<setting.move_base_timer<<endl;
                   cout << "--------------------------------------------------------------------" << endl;

                   //失败尝试反馈
                  task_feedback_set("nav",Order_Interest_index,"","try",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }
               else if(order_error == 2)
               {
                   if(nav.circul_flag)
                   {
                     Order_Interest_index += 1;
                   }
                   //循环
                   if(Order_Interest_index >= json_interest_point["point"].size())
                   {
                       Order_Interest_index = 0;
                   }
                   geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Order_Interest_index]["z"].asDouble());
                   set_goal("map",json_interest_point["point"][Order_Interest_index]["x"].asDouble(),json_interest_point["point"][Order_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                   //ROS_WARN("order error will  order go to next cruise point:%.1f",setting.move_base_timer);
                   PLOG_WARNING<<"order error will  order go to next cruise point:"<<Order_Interest_index;
                   cout <<"导航错误准备前往下一个巡航点:"<<setting.move_base_timer<<endl;
                   cout << "--------------------------------------------------------------------" << endl;

                    //失败尝试反馈
                  task_feedback_set("nav",Order_Interest_index,"","try",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }
               else
               {
                   //ROS_WARN("order error three:%.1f",setting.move_base_timer);
                   PLOG_ERROR<<"order error three:"<<setting.move_base_timer;
                   cout <<"导航第三次错误:"<<setting.move_base_timer<<endl;
                   cout << "--------------------------------------------------------------------" << endl;
                   order_error = 0;

                   base_status.robot.main_error = base_status.robot.status;
                   base_status.robot.sub_error  = 0;
                   base_status.robot.status = Mod_Error;

                    //失败尝试反馈
                  task_feedback_set("nav",Order_Interest_index,"","failure",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }

               
               break;
           }
           case Mod_Random_Interest:  //随机巡航模式
           {
            if(json_interest_point["point"].size() == 0)
	    	{
	     		//ROS_ERROR("error have no interest point");
                PLOG_ERROR<<"error have no interest point";
	     		base_status.robot.main_error = Mod_Random_Interest;
	      		base_status.robot.sub_error  = 2;
	     		base_status.robot.status = Mod_Error;
	     		return;
	    	}

                //轨道模式下再次设置目标
               if(nav.path_mode_flag == true)
               {
                     bool trajectory_mode;
                    ros::param::get("/move_base/TebLocalPlannerROS/trajectory_mode",trajectory_mode);

                    if(trajectory_mode == true)
                    {
                        set_trajectory_goal();
                    }
                    return;
               }

               random_error++;

               if(random_error < 2)
               {
                   geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Random_Interest_index]["z"].asDouble());
                   set_goal("map",json_interest_point["point"][Random_Interest_index]["x"].asDouble(),json_interest_point["point"][Random_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                   //ROS_WARN("random error will again go to cruise point:%.1f",setting.move_base_timer);
                   PLOG_WARNING<<"random error will again go to cruise point:"<<Random_Interest_index;
                   cout <<"导航错误准备再次前往巡航点:"<<setting.move_base_timer<<endl;
                   cout << "--------------------------------------------------------------------" << endl;

                   //失败尝试反馈
                  task_feedback_set("nav",Random_Interest_index,"","try",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }
               else if(random_error == 2)
               {
                   //失败尝试反馈
                   task_feedback_set("nav",Random_Interest_index,"","try",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);

                   int index = rand() % json_interest_point["point"].size();
                   if(nav.circul_flag)
                   {
                     Random_Interest_index = index;
                   }

                   geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][index]["z"].asDouble());
                   set_goal("map",json_interest_point["point"][index]["x"].asDouble(),json_interest_point["point"][index]["y"].asDouble(),goal_quat.z,goal_quat.w); 
                   //ROS_WARN("random error will go to next cruise point:%.1f",setting.move_base_timer);
                   PLOG_WARNING<<"random error will go to next cruise point:"<<index;
                   cout <<"导航错误准备随机前往下一个巡航点:"<<setting.move_base_timer<<endl;
                   cout << "--------------------------------------------------------------------" << endl;
               }
               else
               {
                   //ROS_WARN("random error three:%.1f",setting.move_base_timer);
                   PLOG_ERROR<<"random error three"<<setting.move_base_timer;
                   cout <<"导航第三次错误:"<<setting.move_base_timer<<endl;
                   cout << "--------------------------------------------------------------------" << endl;
                   random_error = 0;
                   base_status.robot.main_error = base_status.robot.status;
                   base_status.robot.sub_error  = 0;
                   base_status.robot.status = Mod_Error;

                   //失败尝试反馈
                  task_feedback_set("nav",Random_Interest_index,"","failure",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }

               break;
           }
           case Mod_Charge:  //回冲模式
           {
                 if(json_charge_point["point"].size() == 0)
                {
                    //ROS_ERROR("error have no charge point");
                    PLOG_ERROR<<"error have no charge point";
                    base_status.robot.main_error = Mod_Charge;
                    base_status.robot.sub_error  = 2;
                    base_status.robot.status = Mod_Error;
                    return;
                }

                //轨道模式下再次设置目标
               if(nav.path_mode_flag == true)
               {
                     bool trajectory_mode;
                    ros::param::get("/move_base/TebLocalPlannerROS/trajectory_mode",trajectory_mode);

                    if(trajectory_mode == true)
                    {
                        set_trajectory_goal();
                    }
                    return;
               }

               charge_error++;
  
               if(charge_error < setting.go_charge_error_time)
               {
                   int index = 0;
                   geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_charge_point["point"][index]["z"].asDouble());
                   set_goal("map",json_charge_point["point"][index]["x"].asDouble(),json_charge_point["point"][index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                   
                   //ROS_WARN("going charge error will again!");
                   PLOG_WARNING<<"going charge error will again!";
                   cout <<"回充错误将再次回充！"<<endl;
                   cout << "--------------------------------------------------------------------" << endl;

                    //失败尝试反馈
                  task_feedback_set("charge",0,"","try",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }
               else
               {
                   //ROS_WARN("going charge error!:%d",charge_error);
                   PLOG_ERROR<<"going charge error!:"<<charge_error;
                   cout <<"回充错误"<<charge_error<<endl;
                   cout << "--------------------------------------------------------------------" << endl;
                   charge_error = 0;
                   base_status.robot.main_error = base_status.robot.status;
                   base_status.robot.sub_error  = 0;
                   base_status.robot.status = Mod_Error;

                    //失败尝试反馈
                  task_feedback_set("charge",0,"","failure",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
               }

               break;
           }
           case Mod_Trajectory:  //轨迹模式
           {
                bool trajectory_mode;
                ros::param::get("/move_base/TebLocalPlannerROS/trajectory_mode",trajectory_mode);

                if(trajectory_mode == true)
                {
                    set_trajectory_goal();
                }
                else
                {
                    base_status.robot.main_error = base_status.robot.status;
                    base_status.robot.sub_error  = 3;
                    base_status.robot.status = Mod_Error;
                   
                    //到达轨迹起点失败
                    if(track.dir_flag == false)
                    {
                        task_feedback_set("track",track.start_index,"forward","failure",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                    }
                    else
                    {
                        task_feedback_set("track",track.start_index,"back","failure",
                                                        last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
                    }
                    PLOG_WARNING<<"goto trajectory failure";
                }
                break;
           }
           case Mod_More_Task:  //多任务模式 到定时处理
           {
                bool trajectory_mode;
                ros::param::get("/move_base/TebLocalPlannerROS/trajectory_mode",trajectory_mode);

                if(trajectory_mode == true)
                {
                    set_trajectory_goal();
                }
                else
                {
                    more_error++;
                    if(more_error < 2)
                    {
                        set_goal("map",last_goal.pose.position.x,last_goal.pose.position.y,last_goal.pose.orientation.z,last_goal.pose.orientation.w);
                        //ROS_WARN("more task go cruise point error will again go to cruise point:%d",more_error);
                        PLOG_WARNING<<"more task go cruise point error will again go to cruise point:"<<more_error;
                        cout <<"多任务前往目标点错误准备再次前往巡航点:"<<setting.move_base_timer<<endl;
                        cout << "--------------------------------------------------------------------" << endl;
                    }
                    else
                    {
                        more_error = 0;
                        //ROS_WARN("more task error :%.1f",setting.move_base_timer);
                         PLOG_ERROR<<"more task error :"<<setting.move_base_timer;
                        cout <<"多任务导航错误:"<<setting.move_base_timer<<endl;
                        cout << "--------------------------------------------------------------------" << endl;
                        base_status.robot.main_error = base_status.robot.status;
                        base_status.robot.sub_error  = 0;
                        base_status.robot.status = Mod_Error;
                    }
                }
                break;
           }
       }
    }
    //取消
    else if(result->status.status == 6)
    {
        order_error = 0;
        random_error = 0;
        charge_error = 0;
        more_error = 0;
    }
}


//map信息反馈
int map_index = 0;
nav_msgs::OccupancyGrid map_msg;
void map_agvCallback(const nav_msgs::OccupancyGrid::ConstPtr &map)
{
    map_msg.info = map->info;
    map_msg.data = map->data;
    //防止在启动过程中订阅到地图导致显示错误
    if(deal_back.sub_map_flag == true)
    {
    	deal_back.up_map_flag = true;
    }
    map_index = 0;
    printf("sub map->data:%d\n",map->data.size());
}

//处理map数据
void deal_map_data()
{
    Json::Value pose;
    Json::Value data;

    //json_map.clear();

    json_map["cmd_type"]   = Json::Value("map");
    json_map["resolution"] =  Json::Value(map_msg.info.resolution);
    json_map["width"]      =  Json::Value(map_msg.info.width);
    json_map["height"]     =  Json::Value(map_msg.info.height);
    
    double pose_th = 0.0;
    tf::Quaternion q;
    tf::quaternionMsgToTF(map_msg.info.origin.orientation, q);
    pose_th = tf::getYaw(q);  //角度
    //pose
    pose["yaw"] =   Json::Value(double_to_3double(pose_th));
    pose["x"]   =       Json::Value(map_msg.info.origin.position.x);
    pose["y"]   =       Json::Value(map_msg.info.origin.position.y);
    json_map["pose"] = pose;

    //建图模式下，数据压缩
    if(base_status.robot.status == Mod_Slam)
    {
        json_map["mode"] = Json::Value(1);
        for(int i=0;i<map_msg.data.size();i+=8)
        {
            unsigned char num = 0;
            unsigned char tem_data = 0;
            for(int j=0;j<8;j++)
            {
                if(map_msg.data[i+j] == -1 || map_msg.data[i+j] == 0)
                {

                }
                else 
                {
                    num = 0x01; 
                    num <<= j;
                    tem_data |= num;
                }
            }
            data.append(Json::Value(tem_data));
        }
    }
    else
    {
        json_map["mode"] = Json::Value(0);
        //每次最多发送10w数据 100000/1024=100kb   然后频率为10则为1m/s
        for(int i=map_index;   i<map_index+100000 &&  i<map_msg.data.size();i++)
        {
            data.append(Json::Value(map_msg.data[i]));
        }
        printf("map map_index:%d\n",map_index);
    }
    printf("map size:%d\n",map_msg.data.size());
    json_map["data"] = data;

    //pub
    bool have_map_flag = false;
    //遍历队列
    int send_que_size = send_que.size();
    for(int i = 0; i < send_que_size; i++) 
    {   //send_que_size 必须是固定值
        if(send_que.front() == Json_Map)
        {
            //队列中还有未发送的map
            have_map_flag = true;
        }
        send_que.push(send_que.front());
        send_que.pop();
    }

    if(have_map_flag == false)
    {
        send_que.push(Json_Map);
    }
    //pose.clear();
    //data.clear();
    //send_que.push(Json_Map);
}


//scan信息反馈 
sensor_msgs::LaserScan scan_msg;
void scan_agvCallback(const sensor_msgs::LaserScan::ConstPtr &scan)
{
    scan_msg.angle_min = scan->angle_min;
    scan_msg.angle_max = scan->angle_max;
    scan_msg.angle_increment = scan->angle_increment;
    scan_msg.time_increment = scan->time_increment;
    scan_msg.scan_time = scan->scan_time;
    scan_msg.range_min = scan->range_min;
    scan_msg.range_max = scan->range_max;
    scan_msg.ranges.clear();
    for(int i=0;i<scan->ranges.size();i++)
    {   
        scan_msg.ranges.push_back(scan->ranges[i]);
    }

    deal_back.up_scan_flag = true;
}

void deal_scan_data()
{
    Json::Value head;
    Json::Value ranges;
    Json::Value pose;

    base_status.sensor.laser = true;

    json_scan["cmd_type"] = Json::Value("scan");

    //head
    head.append(Json::Value(scan_msg.angle_min));
    head.append(Json::Value(scan_msg.angle_max));
    head.append(Json::Value(scan_msg.angle_increment));
    head.append(Json::Value(scan_msg.time_increment));
    head.append(Json::Value(scan_msg.scan_time));
    head.append(Json::Value(scan_msg.range_min));
    head.append(Json::Value(scan_msg.range_max));
    json_scan["hed"] = head;


    //pose  激光雷达在地图中的位置
    tf::StampedTransform transform;
    tf::Quaternion q;
    geometry_msgs::Pose current_pose_ros;
    double pose_th = 0.0;
    double roll = 0.0,pitch = 0.0,yaw = 0.0;

    try 
    {
        //得到坐标map和坐标laser之间的关系 多线雷达为laser1
        scan_listener->waitForTransform("map","laser", ros::Time(0), ros::Duration(0.2));
        scan_listener->lookupTransform("map","laser",ros::Time(0), transform);

        geometry_msgs::TransformStamped transform_pose;
        tf::transformStampedTFToMsg(transform, transform_pose);
        current_pose_ros.position.x = transform.getOrigin().x();
        current_pose_ros.position.y = transform.getOrigin().y();
        current_pose_ros.position.z = 0.0;
        current_pose_ros.orientation= transform_pose.transform.rotation;
    
        tf::quaternionMsgToTF(current_pose_ros.orientation, q);
        //pose_th = tf::getYaw(q);  //角度
        
        tf::Matrix3x3(q).getRPY(roll,pitch,yaw);
        //有时候为-3.14原因还未找到可能是TF变换时候3.1415926大了一点点
        roll = fabs(roll);
        //printf("roll:%.2f pitch:%.2f yaw:%.2f\n",roll,pitch,yaw);

        pose["x"]   = Json::Value((current_pose_ros.position.x));
        pose["y"]   = Json::Value((current_pose_ros.position.y));
        pose["yaw"] = Json::Value(double_to_3double(yaw));
    } 
    catch (std::exception e ) 
    {
        pose["x"]   = Json::Value(0.0);
        pose["y"]   = Json::Value(0.0);
        pose["yaw"] = Json::Value(0.0);
        //ROS_WARN("cannot get laser pose!");
        PLOG_WARNING<<"cannot get laser pose!";
    }

    json_scan["pose"] = pose;

    //判断翻滚角接近于3.14则为雷达倒装
    if(fabs(roll - 3.14) < 0.5)
    {
         //range  雷达倒装
        for(int i=scan_msg.ranges.size()-1;i>0;i-=2)
        {
            if(scan_msg.ranges[i] > scan_msg.range_max)
            {
                ranges.append(0.0);
            }
            else
            {
                ranges.append(double_to_3double (scan_msg.ranges[i]) );
            }
        }
    }
    else
    {
         //ranges  雷达正装
        for(int i=0;i<scan_msg.ranges.size();i+=2)
        {
            if(scan_msg.ranges[i] > scan_msg.range_max)
            {
                ranges.append(Json::Value(0.0));
            }
            else
            {
                ranges.append(Json::Value(double_to_3double (scan_msg.ranges[i])) );
            }
        }
    }
    json_scan["ranges"] = ranges;
}

//agv底盘信息回调
void cmd_agvCallback(const ros_agv3_msg::agv1::ConstPtr &agv_msg)
{
    //获得状态
    if(agv_msg->AccZ)
    {
        base_status.sensor.imu   = true;
    }
    base_status.sensor.robot = true;
    base_status.robot.power = agv_msg->Voltage/10.0;
    base_status.robot.vx = agv_msg->Vx;
    base_status.robot.vy = 0.0;
    base_status.robot.vz = agv_msg->Vz;
    if(SCAN_ICP_MATCH == 0)
    {
         base_status.robot.charge = (agv_msg->State >> 8) & 0xff;
    }
    base_status.robot.robot_status = agv_msg->State;
    switch(base_status.robot.charge)
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            break;
        }
        //error
        case 2:
        {
            break;
        }
        case 3:
        {
            break;
        }
        //回充成功，正常接触
        case 4:
        {   
          /*
             if(base_status.robot.status == Mod_Free)
             {
                base_status.robot.status = Mod_Charge;
             }*/
            break;
        }
        //error
        case 5:
        {
            break;
        }
        //error
        case 6:
        {
            break;
        }
        case 7:  //退出
        {       
            break;
        }
    }

}

#define Base_Width  1100  //轴距
//four底盘信息回调
void cmd_fourCallback(const ros_four1_msg::four1::ConstPtr  &four1_msg)
{
    base_status.sensor.robot = true;
    base_status.robot.power = four1_msg->Voltage/10.0;
    base_status.robot.vx = (four1_msg->FLSpeed + four1_msg->FRSpeed + four1_msg->BLSpeed + four1_msg->BRSpeed) /4.0 / 1.0;
    base_status.robot.vy = 0.0;
    base_status.robot.vz = ((four1_msg->FRSpeed + four1_msg->BRSpeed) - (four1_msg->FLSpeed + four1_msg->BLSpeed))/2.0 / Base_Width;
    base_status.robot.charge = 0;
    base_status.robot.robot_status = four1_msg->State;
}

//pt底盘信息回调
void cmd_ptCallback(const ros_pt_msg::pt1::ConstPtr   &pt_msg)
{
    base_status.sensor.robot = true;
    base_status.robot.power = pt_msg->voltage;
    base_status.robot.vx = (pt_msg->flwspeed + pt_msg->frwspeed + pt_msg->blwspeed + pt_msg->brwspeed) /4.0 / 1.0;
    base_status.robot.vy = 0.0;
    base_status.robot.vz = ((pt_msg->frwspeed + pt_msg->brwspeed) - (pt_msg->flwspeed + pt_msg->blwspeed))/2.0 / Base_Width;
    //base_status.robot.charge = 0;
     if(SCAN_ICP_MATCH == 0)
    {
        base_status.robot.charge = (pt_msg->status >> 8) & 0xff;
    }
    base_status.robot.robot_status = pt_msg->status;
    //防撞杆促发，进入错误模式
    if(pt_msg->status & 0x1800)
    {
        base_status.robot.main_error = base_status.robot.status;
        base_status.robot.sub_error  = 13;
        base_status.robot.status = Mod_Error;
    }
}

void cmd_dtCallback(const ros_dt_msg::dt1::ConstPtr  &dt1_msg)
{
    base_status.sensor.robot = true;
    base_status.robot.power = dt1_msg->Voltage/10.0;
    base_status.robot.vx = dt1_msg->Vx;
    base_status.robot.vy = 0.0;
    base_status.robot.vz =dt1_msg->Vz;
    base_status.robot.light12 = dt1_msg->Light12;
    //base_status.robot.charge = 0;
     if(SCAN_ICP_MATCH == 0)
    {
        base_status.robot.charge = (dt1_msg->State >> 8) & 0xff;
    }
    base_status.robot.robot_status =dt1_msg->State;
    //防撞杆促发，进入错误模式
    if(dt1_msg->State & 0x000c)
    {
        base_status.robot.main_error = base_status.robot.status;
        base_status.robot.sub_error  = 13;
        base_status.robot.status = Mod_Error;
    }
}

void cmd_agv_dtCallback(const agv_msgs::agv &agv_msg)
{
    base_status.sensor.robot = true;
    base_status.robot.power = agv_msg.voltage;
    base_status.robot.vx = agv_msg.linear_velocity*1000;
    base_status.robot.vy = 0.0;
    base_status.robot.vz =agv_msg.angular_velocity;
    #ifdef LASER_CHARGE
    #else
     base_status.robot.charge =agv_msg.charge_state;
    #endif
    base_status.robot.robot_status =agv_msg.charge_state;
    //防撞杆促发，进入错误模式
    if(agv_msg.front_collision || agv_msg.back_collision  || agv_msg.drive_error  || agv_msg.voltage_error)
    {
        base_status.robot.main_error = base_status.robot.status;
        base_status.robot.sub_error  = 13;
        base_status.robot.status = Mod_Error;
    }
}

//pt底盘信息回调
void cmd_four_ptCallback(const four_wheel_msgs::four_wheel::ConstPtr   &pt_msg)
{
    base_status.sensor.robot = true;
    base_status.robot.power = pt_msg->voltage;
    base_status.robot.vx = pt_msg->linear_velocity * 1000;
    base_status.robot.vz = pt_msg->angular_velocity;
    //base_status.robot.charge = 0;
     if(SCAN_ICP_MATCH == 0)
    {
        base_status.robot.charge = pt_msg->charge_state;
    }
    //防撞杆促发，进入错误模式
    if(pt_msg->front_collision)
    {
        base_status.robot.main_error = 0;
        base_status.robot.sub_error  = 13;
        base_status.robot.status = Mod_Error;
    }
}

void cmd_agv_dt_controlCallback(const dt_control_msgs::dt_state &agv_msg)
{
    base_status.sensor.robot = true;
    base_status.robot.power = agv_msg.battery_voltage;
    base_status.robot.vx = agv_msg.linear_velocity*1000;
    base_status.robot.vy = 0.0;
    base_status.robot.vz =agv_msg.angular_velocity;
    #ifdef LASER_CHARGE
    #else
    //base_status.robot.charge =agv_msg.charge_state;
    #endif
    //base_status.robot.robot_status =agv_msg.charge_state;
    //防撞杆促发，进入错误模式
    if(agv_msg.front_collision || agv_msg.back_collision  || agv_msg.drive_error)
    {
        base_status.robot.main_error = base_status.robot.status;
        base_status.robot.sub_error  = 13;
        base_status.robot.status = Mod_Error;
    }
}

void cmd_agv_dt_control_chargeCallback(const dt_control_msgs::dt_charge &charge_msg)
{
    base_status.robot.charge = charge_msg.infrared;
    base_status.robot.robot_status = charge_msg.infrared;
}

//全剧路径规划反馈
void gloab_path_Callback(const nav_msgs::Path::ConstPtr &path)
{
    //Json::Value json_gloab_path;
    Json::Value point;
    Json::Value array;

    json_gloab_path["cmd_type"] = Json::Value("gloab_path");
    for(int i=0;i<path->poses.size();)
    {
       point["x"] = Json::Value(double_to_3double(path->poses[i].pose.position.x));
       point["y"] = Json::Value(double_to_3double(path->poses[i].pose.position.y));
       array.append(point);
      
       if(nav.path_mode_flag == true)
       {
           i+=1;
       }
       else
       {
           i+=16;
       }
    }
    json_gloab_path["point"] = array;
    //pub
    //send_que.push(5);
    
}

//局部路径规划反馈
void local_path_Callback(const nav_msgs::Path::ConstPtr &path)
{
    geometry_msgs::PoseStamped Pose;

    //Json::Value json_local_path;
    Json::Value point;
    Json::Value array;
    

    json_local_path["cmd_type"] = Json::Value("local_path");
    for(int i=0;i<path->poses.size();i+=1)
    {
       try
       {
           local_listener->transformPose("map",path->poses[i],Pose);
       }
       catch(tf::TransformException ex)
       {
           //ROS_INFO("local path error!");
           PLOG_INFO<<"local path error!";
           return;
       }
       
       point["x"] = Json::Value(double_to_3double(Pose.pose.position.x));
       point["y"] = Json::Value(double_to_3double(Pose.pose.position.y));
       array.append(point);
    }
    json_local_path["point"] = array;
    //pub
    //send_que.push(6);
}

/***********************************************************************************************************/

//===============================================================
// 语法格式：    int get_local_ip(char * ifname, char * ip ,char * hostname)
// 实现功能：    主函数，建立一个TCP并发服务器
// 入口参数：    char * ifname 网卡名, char * ip 获得的IP,char * hostname 主机名
// 出口参数：    0 返回成功 -1 失败
//===============================================================

//#define IF_NAME "wlp1s0"
//得到本地IP和广播IP
int get_local_ip(char * ifname, char * ip ,char * hostname)
{
    char *temp = NULL;
    int inet_sock;
    struct ifreq ifr;
    char name[25];
    gethostname(name, sizeof(name));
    memcpy(hostname, name, strlen(name));

    inet_sock = socket(AF_INET, SOCK_DGRAM, 0); 

    memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    memcpy(ifr.ifr_name, ifname, strlen(ifname));
    //获得IP
    if(0 != ioctl(inet_sock, SIOCGIFADDR, &ifr)) 
    {   
        perror("get ip error");
        return -1;
    }

    temp = inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr);     
    memcpy(ip, temp, strlen(temp));

    //获得广播IP
    if(0 != ioctl(inet_sock, SIOCGIFBRDADDR, &ifr)) 
    {   
        perror("get board ip error");
        return -1;
    }
    temp = inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr); 
    memcpy(ip, temp, strlen(temp));
    close(inet_sock);

    return 0;
}


char msg[128] = "zkwl";	
struct sockaddr_in my_addr, user_addr;
socklen_t slen;
int lfd, ret;
//UDP初始化
void UDPSendInit(void)
{
    //获得无线网卡名
    char IF_NAME[20]={0};
    getWfiDEVICEName(IF_NAME);
    //intf("wifiname:%s",name);
    //获得广播IP地址和主机名
    char ip[32] = {0},hostname[32] = {0};
    get_local_ip(IF_NAME,ip,hostname);
    if(0 != strcmp(ip, ""))
    {
        printf("%s ip is %s  name:%s\n",IF_NAME, ip,hostname);
    }
    lfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(lfd < 0)
    {
        perror("socket create error");
        //exit(1);
    }

    //创建连接
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &flag, sizeof(flag) );
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4343);
    my_addr.sin_addr.s_addr = inet_addr(ip);
    
    setsockopt(lfd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &flag, sizeof(flag) );
    user_addr.sin_family = AF_INET;
    user_addr.sin_port = htons(4343);
    user_addr.sin_addr.s_addr = inet_addr("10.42.0.255");
}

//UDP广播发送
void UDPSend(void)
{
    int n = sendto(lfd, msg, strlen(msg), 0, (struct sockaddr *)&my_addr, sizeof(my_addr));
    
    int k = sendto(lfd, msg, strlen(msg), 0, (struct sockaddr *)&user_addr, sizeof(user_addr));
    if(n  < 0 && k < 0)
    {
        perror("sendto Error:");
        //exit(1);
    }
}


double calc_distance(double x,double y,double cx,double cy) 
{
    double distance = sqrt((cx-x)*(cx-x)+(cy-y)*(cy-y));
    return distance;
}
//防止机器位置跳跃
void Prevent_pose_jumping(void)
{
    tf::Quaternion q;
    static geometry_msgs::Pose last_pose;
    static  double last_pose_th = 0.0;
    static  int time_cnt = 0;
    static  bool last_local_carto = false;

    if(base_status.robot.status != Mod_Slam && base_status.local.carto == true)
    {
        double distance = calc_distance(last_pose.position.x,last_pose.position.y,base_status.pose.x,base_status.pose.y);

        if(((abs(distance) > abs(base_status.robot.vx * 3.0 * 0.1)/1000.0 + 0.1) ) 
            && time_cnt == 0)
        {
            //ROS_WARN("the distance is jump %f %f",abs(distance),abs(base_status.robot.vx * 3.0 * 0.1)/1000.0 + 0.1);
            LOG_WARNING<<"the distance is jump:"<<distance;

            //test 0215
            geometry_msgs::PoseWithCovarianceStamped pose;
            pose.header.frame_id = "map";
            pose.pose.pose.position.x = last_pose.position.x;
            pose.pose.pose.position.y = last_pose.position.y;

            geometry_msgs::Quaternion pose_quat = tf::createQuaternionMsgFromYaw(last_pose_th);
    
            pose.pose.pose.orientation.z = pose_quat.z;
            pose.pose.pose.orientation.w = pose_quat.w;

            float factorPos = 0.03;
            float factorRot = 0.1;
            pose.pose.covariance[6*0+0] = (0.5 * 0.5) * factorPos;
            pose.pose.covariance[6*1+1] = (0.5 * 0.5) * factorPos;
            pose.pose.covariance[6*3+3] = (M_PI/12.0 * M_PI/12.0) * factorRot;
    
            //initialpose1_pub.publish(pose);

            //标记没定位
            //base_status.local.carto = false;
        }
        else
        {
            
            if(time_cnt){time_cnt--;}

        }
        //记录carto的定位状态
        last_local_carto = base_status.local.carto;
        last_pose.position.x = base_status.pose.x;
        last_pose.position.y = base_status.pose.y;
        last_pose_th         = base_status.pose.yaw;
    }
    /*
    else
    {
        last_pose.position.x = base_status.pose.x;
        last_pose.position.y = base_status.pose.y;
        last_pose_th         = base_status.pose.yaw;
        if(time_cnt){time_cnt--;}
    }*/

}

//----------------------------------------------------json------------------------------------------------------------------
#include <geometry_msgs/Pose2D.h>
void robot_pose_get(void)
{
    tf::StampedTransform transform;
    tf::Quaternion q;
    geometry_msgs::Pose current_pose_ros;
    double pose_th = 0.0;
    static int cnt = 0;
    static bool load_pose_first_run_flag = true;

    try 
    {
        //得到坐标odom和坐标base_link之间的关系
        robot_listener->waitForTransform("map","base_link", ros::Time(0), ros::Duration(0.2));
        robot_listener->lookupTransform("map","base_link",ros::Time(0), transform);

        geometry_msgs::TransformStamped transform_pose;
        tf::transformStampedTFToMsg(transform, transform_pose);
        current_pose_ros.position.x = transform.getOrigin().x();
        current_pose_ros.position.y = transform.getOrigin().y();
        current_pose_ros.position.z = 0.0;
        current_pose_ros.orientation= transform_pose.transform.rotation;
    
        tf::quaternionMsgToTF(current_pose_ros.orientation, q);
        pose_th = tf::getYaw(q);  //角度

        base_status.pose.x     = current_pose_ros.position.x;
        base_status.pose.y     = current_pose_ros.position.y;
        base_status.pose.yaw   = pose_th;
    } 
    catch (std::exception e ) 
    {
        base_status.pose.x     = 0;
        base_status.pose.y     = 0;
        base_status.pose.yaw   = 0;
        //ROS_WARN ("robot_pose_get cannot get robot pose!");
        PLOG_WARNING<<"robot_pose_get cannot get robot pose!";
        return;
    }
    if(load_pose_first_run_flag)
    {
        cnt++;
        if(cnt > 5)
        {
            cnt = 0;
            load_pose_first_run_flag = false;
	    //如果为顶点启动，则自动重定位
	    load_save_pose_json();
	    if(setting.start_point_check == true)
	    {
	        pub_reset_pose(true);
                //开启超时定时
                reset_pose_timer.start();  
	        base_status.robot.doing_reset = true;
	        PLOG_INFO<<"start reset pose";
	    }
        }
    }

    
}

void task_feedback_set(std::string type, int id, std::string dir, std::string status,double gx,double gy,double gyaw,double nx,double ny,double nyaw)
{
    Json::Value g_pose;
    Json::Value n_pose;
    json_task_feedback["cmd_type"] = Json::Value("task_feedback");
    json_task_feedback["type"]     = Json::Value(type);
    json_task_feedback["id"]       = Json::Value(id);
    json_task_feedback["dir"]      = Json::Value(dir);
    json_task_feedback["status"]   = Json::Value(status);

    g_pose["x"] = gx;
    g_pose["y"] = gy;
    g_pose["yaw"] = double_to_3double(gyaw);
    json_task_feedback["goal_pose"] = g_pose;

    n_pose["x"] = double_to_3double(nx);
    n_pose["y"] = double_to_3double(ny);
    n_pose["yaw"] = double_to_3double(nyaw);
    json_task_feedback["now_pose"] = n_pose;

    send_que.push(Json_Task_feedback);
}

void jobnum_get(void)
{
    switch(base_status.robot.status)
    {
        case Mod_Free:
        {
            base_status.robot.main_task = 0;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_Nav:
        {
            base_status.robot.main_task = 0;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_Slam:
        {
            base_status.robot.main_task = 0;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_Order_Interest:
        {
            base_status.robot.main_task = Order_Interest_index;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_Random_Interest:
        {
            base_status.robot.main_task = Random_Interest_index;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_Charge:
        {
            base_status.robot.main_task = 0;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_Trajectory:
        {
            base_status.robot.main_task = track.start_index;
            base_status.robot.sub_task = 0;
            break;
        }
        case Mod_More_Task:
        {
            base_status.robot.main_task = more_task.main_task;
            base_status.robot.sub_task  = more_task.sub_task;
            break;
        }
        case Mod_More_Pause:
        {
            base_status.robot.main_task = more_task.main_task;
            base_status.robot.sub_task  = more_task.sub_task;
            break;
        }
        case Mod_Error:
        {
            base_status.robot.main_task = 0;
            base_status.robot.sub_task = 0;
            break;
        }
    }
}

//===============================================================
// 语法格式：    void local_updata_control_1hz(void)
// 实现功能：    1hz停止时候位置更新控制
// 入口参数：    无
// 出口参数：    无
//===============================================================
#define Wait_Time  10
void local_updata_control_1hz(void)
{
     std_msgs::UInt8 msg;
     //空闲模式下，且不在重定位的时候
     if(/*base_status.robot.status == Mod_Free &&*/ base_status.robot.doing_reset == false && abs(base_status.robot.vx) < 50 && abs(base_status.robot.vz) < 0.1)
     {
        stop_up_loca_time++;
        if(stop_up_loca_time > Wait_Time)
        {
           stop_up_loca_time = Wait_Time;
        }
        if(stop_up_loca_time == Wait_Time)
        {
            msg.data = 2;
            reset_pose_pub.publish(msg);
        }

    }
    else if(base_status.robot.doing_reset == false)
    {
  	stop_up_loca_time = 0;
        msg.data =0;
        reset_pose_pub.publish(msg);
    }
    else
    {
        stop_up_loca_time = 0;
    }
}
//===============================================================
// 语法格式：    void base_status_send_10hz(void)
// 实现功能：    10hz发送基本状态
// 入口参数：    无
// 出口参数：    无
//===============================================================
void base_status_send_10hz(void)
{

    robot_pose_get();

    //只有各状态正常才保存当前位置坐标，且不在建图模式下
    if(base_status.sensor.laser == true && base_status.sensor.imu == true && base_status.sensor.robot == true && base_status.robot.status != Mod_Slam
        && setting.start_point_check == false  && base_status.local.carto == true)
    {
        updata_save_pose_json(base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
    }

    jobnum_get();

    Prevent_pose_jumping();

   

    //pose
    Json::Value json_pose;
    json_pose["x"]   = Json::Value(double_to_3double(base_status.pose.x));
    json_pose["y"]   = Json::Value(double_to_3double(base_status.pose.y));
    json_pose["yaw"] = Json::Value(double_to_3double(base_status.pose.yaw));

     //reflector
    Json::Value json_reflector;
    json_reflector["icp_socre"] = Json::Value(base_status.reflector.icp_socre);

    //sensor
    Json::Value json_sensor;
    json_sensor["laser"]   = Json::Value(base_status.sensor.laser);
    json_sensor["imu"]     = Json::Value(base_status.sensor.imu);
    json_sensor["robot"]   = Json::Value(base_status.sensor.robot);

    //robot
    Json::Value json_robot;
    json_robot["status"]   = Json::Value(base_status.robot.status);
    json_robot["maintask"] = Json::Value(base_status.robot.main_task);
    json_robot["subtask"]  = Json::Value(base_status.robot.sub_task);
    json_robot["mainerror"] = Json::Value(base_status.robot.main_error);
    json_robot["suberror"]  = Json::Value(base_status.robot.sub_error);
    json_robot["power"]  = Json::Value(base_status.robot.power);
    json_robot["charge"] = Json::Value(base_status.robot.charge);
    json_robot["robot_status"] = Json::Value(base_status.robot.robot_status);
    json_robot["vx"] = Json::Value(base_status.robot.vx);
    json_robot["vy"] = Json::Value(base_status.robot.vy);
    json_robot["vz"] = Json::Value(base_status.robot.vz);

    //test
     json_robot["light12"] = Json::Value(base_status.robot.light12);

    //bms
    Json::Value json_bms;
    json_bms["voltage"]            = Json::Value(base_status.bms.voltage);
    json_bms["current"]            = Json::Value(base_status.bms.current);
    json_bms["status"]             = Json::Value(base_status.bms.status);
    json_bms["tem"]                = Json::Value(base_status.bms.tem);
    json_bms["remaining_capacity"] = Json::Value(base_status.bms.remaining_capacity);
    json_bms["error"]              = Json::Value(base_status.bms.error);
    json_bms["soc"]                = Json::Value(base_status.bms.soc);

    //local
    Json::Value json_loac;
    json_loac["amcl"] = Json::Value(double_to_3double(base_status.local.amcl));
    json_loac["location"] = Json::Value(base_status.local.carto);

    //magnetic
    Json::Value json_magnetic;
    json_magnetic["material"] = Json::Value(base_status.magnetic.material);
    json_magnetic["up"] = Json::Value(base_status.magnetic.up);
    json_magnetic["low"] = Json::Value(base_status.magnetic.low);

    //base_status
    //json_base_status.clear();
    json_base_status["cmd_type"] = Json::Value("base_status");
    json_base_status["pose"]     = json_pose;
    json_base_status["reflector"]     = json_reflector;
    json_base_status["sensor"]   = json_sensor;
    json_base_status["robot"]    = json_robot;
    json_base_status["local"]    = json_loac;
    json_base_status["bms"]      = json_bms;
    json_base_status["version"]    = Json::Value("V3.35.3");
    json_base_status["magnetic"]    = json_magnetic;

    base_status.sensor.laser = false;
    base_status.sensor.imu   = false;
    base_status.sensor.robot = false;

}

/********************************************************************************************************/
//===============================================================
// 语法格式：    void load_vitual_wall_json(void)
// 实现功能：    读取json配置文件数据
// 入口参数：    文件名
// 出口参数：    返回json文件内容
//===============================================================
void load_vitual_wall_json(void)
{
    Json::Value root;
    Json::Reader reader;
    Json::FastWriter swriter;
    
    std::ifstream ifs(invent_wall_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"vitual wall is already open"<<endl;
    }
    
    if(!reader.parse(ifs, root))
    {
        //ROS_ERROR("json_analysis error");
        PLOG_ERROR<<"json_analysis error";
        ifs.close();
    }
    ifs.close();
    if(root["cmd_type"] == "invent_wall_add")
    {
        //deal all vitual wall
        move_base_virtual_wall_server::DeleteAll srv;
        //先清除所有虚拟墙再创建
        if (delete_all_client.call(srv))//服务调用
        {
            //ROS_INFO("clear vitual wall");
            PLOG_INFO<<"local path error!";
        }

        for(int i=0;i<root["point"].size();i++)
        {
            move_base_virtual_wall_server::CreateWall srv;
            srv.request.start_point.x = root["point"][i]["start_x"].asDouble();
            srv.request.start_point.y = root["point"][i]["start_y"].asDouble();
            srv.request.end_point.x   = root["point"][i]["end_x"].asDouble();
            srv.request.end_point.y   = root["point"][i]["end_y"].asDouble();
            srv.request.id = i;
            create_wall_client.call(srv);
        }

    }
    else
    {
        //ROS_ERROR("invent_wall json_analysis  error");
        PLOG_ERROR<<"invent_wall json_analysis  error";
    }

    //
}

/********************************************************************************************************/
//===============================================================
// 语法格式：    void load_setting_json(void)
// 实现功能：    读取json配置文件数据
// 入口参数：    文件名
// 出口参数：    返回json文件内容
//===============================================================
void load_setting_json(void)
{
    Json::Value root;
    Json::Reader reader;
    Json::FastWriter swriter;
    
    std::ifstream ifs(setting_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"setting is already open"<<endl;
    }
    
    if(!reader.parse(ifs, root))
    {
        //ROS_ERROR("json_analysis error");
        PLOG_ERROR<<"json_analysis error";
        ifs.close();

        base_status.robot.main_error = 11;
        base_status.robot.sub_error  = 1;
        base_status.robot.status = Mod_Error;
    }
    ifs.close();
    if(root["cmd_type"] == "setting")
    {
        setting.power_max        = root["power_max"].asDouble();
        setting.power_min        = root["power_min"].asDouble();
    
        setting.go_power         = root["go_power"].asInt();
        setting.exit_power       = root["exit_power"].asInt();
    
        setting.go_power_check   = root["go_power_check"].asBool();
        setting.exit_power_check = root["exit_power_check"].asBool();
  
        setting.radra_check      = root["radra_check"].asBool();
        setting.path_check       = root["path_check"].asBool();
        
        setting.move_base_timer  = root["nav_time"].asDouble();
        setting.go_charge_error_time  = root["go_power_error"].asInt();

        setting.start_point_check = root["start_point"].asBool();
    }
    else
    {
        //ROS_ERROR("setting json_analysis  error");
        PLOG_ERROR<<"setting json_analysis  error";
    }

    //
}


/********************************************************************************************************/
//===============================================================
// 语法格式：    void updata_save_pose_json(void)
// 实现功能：    更新json配置文件数据
// 入口参数：    文件名
// 出口参数：    返回json文件内容
//===============================================================
void updata_save_pose_json(double x,double y,double yaw)
{
    static char cnt = 0;
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;
    Json::Value root;

    root["cmd_type"] = Json::Value("save_pose");
    root["x"] = Json::Value(x);
    root["y"] = Json::Value(y);
    root["z"] = Json::Value(yaw);

    str = swriter.write(root);
    //防止断电时候保存文件未完成，所以保存两个文件
    cnt++;
    if(cnt%2)
    {
            ofs.open(save_pose_filename);
            if(ofs.is_open())
            {
                ofs << str;
                ofs.close();
            }
            else
            {
                 //ROS_ERROR("save_pose_filename open  error");
                PLOG_ERROR<<"save_pose_filename open  error";
            }
            
    }
    else
    {
            ofs.open(save_pose_filename1);
            if(ofs.is_open())
            {
                ofs << str;
                ofs.close();
            }
            else
            {
                //ROS_ERROR("save_pose_filename1 open  error");
                PLOG_ERROR<<"save_pose_filename1 open  error";
            }
    }
    
}

/********************************************************************************************************/
//===============================================================
// 语法格式：    void load_save_pose_json(void)
// 实现功能：    读取json配置文件数据
// 入口参数：    文件名
// 出口参数：    返回json文件内容
//===============================================================
void load_save_pose_json(void)
{
    Json::Value root;
    Json::Reader reader;
    Json::FastWriter swriter;
    
    std::ifstream ifs(save_pose_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"save pose is already open"<<endl;
    }
    
    if(!reader.parse(ifs, root))
    {
        //ROS_ERROR("json_analysis error");
        PLOG_ERROR<<"json_analysis error";

        //load sve_pose1
          std::ifstream ifs1(save_pose_filename1.c_str(), std::ifstream::in);//only read
         if(!reader.parse(ifs1, root))
         {
                //ROS_ERROR("json_analysis1 error");
                PLOG_ERROR<<"json_analysis1 error";
                ifs1.close();
                base_status.robot.main_error = 10;
                base_status.robot.sub_error  = 1;
                base_status.robot.status = Mod_Error;
         }
         else
         {
                ifs1.close();
         }
        
    }
    ifs.close();
    if(root["cmd_type"] == "save_pose")
    {

        geometry_msgs::PoseWithCovarianceStamped pose;
        pose.header.frame_id = "map";
        pose.pose.pose.position.x = root["x"].asDouble();
        pose.pose.pose.position.y = root["y"].asDouble();

        geometry_msgs::Quaternion pose_quat = tf::createQuaternionMsgFromYaw(root["z"].asDouble());
    
        pose.pose.pose.orientation.z = pose_quat.z;
        pose.pose.pose.orientation.w = pose_quat.w;

        float factorPos = 0.03;
        float factorRot = 0.1;
        pose.pose.covariance[6*0+0] = (0.5 * 0.5) * factorPos;
        pose.pose.covariance[6*1+1] = (0.5 * 0.5) * factorPos;
        pose.pose.covariance[6*3+3] = (M_PI/12.0 * M_PI/12.0) * factorRot;
        
        initialpose1_pub.publish(pose);
    }
    else
    {
        //ROS_ERROR("save_pose json_analysis  error");
        PLOG_ERROR<<"save_pose json_analysis  error";
    }

    //
}

/**************************************************************location*****************************************************************************************/
void activeCb1()
{
    //ROS_WARN("autocharge action Active!");
    PLOG_WARNING<<"scan_icp_matcher action Active!";
}

void doneCb1(const actionlib::SimpleClientGoalState &state, const scan_icp_matcher::action1ResultConstPtr &result)
{
    switch (result->result)
    {
        case ac_Cancel: //取消
        {
            base_status.robot.status = Mod_Free;
            break;
        }
        case ac_Success:
        {
            //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 4;
                task_feedback_set("go_charge",result->id,"","process",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            } 
            else
            {
                if(base_status.robot.status == Mod_Location )
                {
                    //base_status.robot.status = Mod_Free;
                }
                //多任务到定时器去处理
                else if(base_status.robot.status == Mod_More_Task )
                {
                    movebase_timer.setPeriod(ros::Duration(2.0),true);
                    movebase_timer.start();
                    //ROS_WARN("will go to next task");
                    PLOG_WARNING<<"will go to next task";
                    cout <<"准备随机前往下一个任务:"<<endl;
                    cout << "--------------------------------------------------------------------" << endl;
                }
                task_feedback_set("location",result->id,"","process",0,0,0,result->pose.x,result->pose.y,result->pose.theta);          
            }
            break;
        }
        case ac_Back_Success:
        {
            //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","success",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                if(base_status.robot.status == Mod_Location )
                {
                    base_status.robot.status = Mod_Free;
                }
                //多任务到定时器去处理
                else if(base_status.robot.status == Mod_More_Task )
                {
                    movebase_timer.setPeriod(ros::Duration(2.0),true);
                    movebase_timer.start();
                    //ROS_WARN("will go to next task");
                    PLOG_WARNING<<"will go to next task";
                    cout <<"准备随机前往下一个任务:"<<endl;
                    cout << "--------------------------------------------------------------------" << endl;
                }
                task_feedback_set("location",result->id,"","success",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            
            break;
        }
        case ac_Loss_Icp:
        {
            //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Loss_Icp - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        }
        case ac_Not_Start:
        {
             //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Not_Start - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        }
        case ac_Not_Align:
        {
             //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Not_Align - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        }
        case ac_Too_Nearl:
        {
             //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Too_Nearl - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        }
        case ac_Pcd_Fail:
        {
             //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Pcd_Fail - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        }
        case ac_Server_Fail:
        {
             //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Server_Fail - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        }
        case ac_Excessive_Error:
        {
             //回充
            if(result->id == 8081)
            {
                base_status.robot.charge = 7;
                task_feedback_set("go_charge",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);
            }
            else
            {
                task_feedback_set("location",result->id,"","failure",0,0,0,result->pose.x,result->pose.y,result->pose.theta);  
            }
            base_status.robot.main_error = Mod_Location;
            base_status.robot.sub_error  = 1 + ac_Excessive_Error - ac_Loss_Icp;
            base_status.robot.status = Mod_Error;
            break;
        } 
        default:break;
    }

    PLOG_WARNING<<"result:"<<result->result<<"  id:"<<result->id;
   
   /*
    //失败
    if(state == actionlib::SimpleClientGoalState::ABORTED)
    {
       base_status.robot.main_error = Mod_Location;
        base_status.robot.sub_error  = 1 + result->error_code;
        base_status.robot.status = Mod_Error;
    }
    //取消
    else if(state == actionlib::SimpleClientGoalState::PREEMPTED )
    {
         base_status.robot.status = Mod_Free;
    }
    //成功
     else if(state == actionlib::SimpleClientGoalState::SUCCEEDED )
    {
        if(base_status.robot.status == Mod_Location )
        {
            base_status.robot.status = Mod_Free;
        }
        //多任务到定时器去处理
        else if(base_status.robot.status == Mod_More_Task )
        {
             movebase_timer.setPeriod(ros::Duration(2.0),true);
             movebase_timer.start();
            //ROS_WARN("will go to next task");
            PLOG_WARNING<<"will go to next task";
            cout <<"准备随机前往下一个任务:"<<endl;
            cout << "--------------------------------------------------------------------" << endl;
        }
         
    }
    //ROS_WARN("Action finished: %s",state.toString().c_str());
    PLOG_WARNING<<"Action finished:"<<state.toString();
    //ROS_WARN("Result: %d", result->error_code);
    PLOG_WARNING<<"Result:"<<result->error_code;
    */

}


void feedbackCb1(const scan_icp_matcher::action1FeedbackConstPtr &feedback)
{
    if(feedback->obstacle)
    {
         //轨迹模式避障停车
        base_status.robot.main_error = base_status.robot.status;
        base_status.robot.sub_error  = 10;
    }
    else
    {
          //轨迹模式避障停车
        if(base_status.robot.status != Mod_Error)
        {
            base_status.robot.main_error = base_status.robot.status;
            base_status.robot.sub_error  = 0;
        }
       
    }
}

//************************************************************************************************************************
void activeCb2()
{
    //ROS_WARN("autocharge action Active!");
    PLOG_WARNING<<"ros_magnetic_nav_service action Active!";
    task_feedback_set("magnetic_nav",magnetic_nav.aim_id,"","start",magnetic_nav.aim_dir,magnetic_nav.aim_action,0,0,0,0); 
}

void doneCb2(const actionlib::SimpleClientGoalState &state, const ros_magnetic_nav_service::action1ResultConstPtr &result)
{
    if(result->result == 0)
    {
        task_feedback_set("magnetic_nav",result->now_rfid,"","success",0,0,0,0,0,0); 
    }
    else
    {
        task_feedback_set("magnetic_nav",result->now_rfid,"","failure",result->result,0,0,0,0,0); 
    }

    if(base_status.robot.status == Mod_Magnetic)
    {
        base_status.robot.status = Mod_Free;
    }
   
}
void feedbackCb2(const ros_magnetic_nav_service::action1FeedbackConstPtr &feedback)
{

}
//************************************************************************************************************************
// 二维码
void activeCb3()
{
    //ROS_WARN("autocharge action Active!");
    PLOG_WARNING<<"ros_qr_tracker_service action Active!";
    // task_feedback_set("qr_tracker",0,"","start",0,0,0,0,0,0); 
}

void doneCb3(const actionlib::SimpleClientGoalState &state, const apriltag_tracker::action1ResultConstPtr &result)
{
    if(result->result == 4)
    {
        task_feedback_set("qr_tracker",result->id,"","success",0,0,0,0,0,0); 
        task_feedback_set("task_feedback",result->id,"","success",0,0,0,0,0,0);
        if(base_status.robot.status == Mod_Qr_Location)
        {
            base_status.robot.status = Mod_Free;
        }
        //多任务到定时器去处理
        else if(base_status.robot.status == Mod_More_Task )
        {
            movebase_timer.setPeriod(ros::Duration(2.0),true);
            movebase_timer.start();
            //ROS_WARN("will go to next task");
            PLOG_WARNING<<"will go to next task";
            cout <<"准备随机前往下一个任务:"<<endl;
            cout << "--------------------------------------------------------------------" << endl;
        }
    }
    else if(result->result == 0)
    {
        task_feedback_set("qr_tracker",result->id,"","stop",0,0,0,0,0,0); 
        task_feedback_set("task_feedback",result->id,"","stop",0,0,0,0,0,0);
        if(base_status.robot.status == Mod_Qr_Location)
        {
            base_status.robot.status = Mod_Free;
        }
    }
    else if(result->result < 0)
    {
        task_feedback_set("qr_tracker",result->id,"","failure",0,0,0,0,0,0); 
        task_feedback_set("task_feedback",result->id,"","failure",0,0,0,0,0,0);
        base_status.robot.main_error = base_status.robot.status;;
        base_status.robot.sub_error  = result->error;
        base_status.robot.status = Mod_Error;
    }
    else if(result->result == 2)
    {
        task_feedback_set("task_feedback",result->id,"","process",0,0,0,0,0,0);
        //多任务到定时器去处理
        if(base_status.robot.status == Mod_More_Task )
        {
            movebase_timer.setPeriod(ros::Duration(2.0),true);
            movebase_timer.start();
            //ROS_WARN("will go to next task");
            PLOG_WARNING<<"will go to next task";
            cout <<"准备随机前往下一个任务:"<<endl;
            cout << "--------------------------------------------------------------------" << endl;
        }
    }
}
void feedbackCb3(const apriltag_tracker::action1FeedbackConstPtr &feedback)
{

}
/***********************************************action*********************************************************/
//task_mode 0对准 1退后 2取消  id 对应特征
void set_client_scan_icp_goal(unsigned char task_mode,unsigned int id)
{
    scan_icp_matcher::action1Goal goal;
    goal.task_mode = task_mode;
    goal.id = id;
    client_scan_icp_ptr->sendGoal(goal,  &doneCb1, &activeCb1, &feedbackCb1);
    location.start_id = id;
}

void set_client_magnetic_nav_goal(unsigned char aim_id,unsigned char aim_dir,unsigned char aim_action)
{
    ros_magnetic_nav_service::action1Goal goal;
    goal.aim_id = aim_id;
    goal.aim_dir = aim_dir;
    goal.aim_action = aim_action;
    magnetic_nav.aim_id = aim_id;
    magnetic_nav.aim_dir = aim_dir;
    magnetic_nav.aim_action = aim_action;
    if(aim_action == 0)  //cancelGoal
    {
        client_magnetic_nav_ptr->cancelGoal();
        base_status.robot.last_status = base_status.robot.status;
        base_status.robot.status = Mod_Free;
    }
    else
    {
        client_magnetic_nav_ptr->sendGoal(goal,  &doneCb2, &activeCb2, &feedbackCb2);
        base_status.robot.last_status = base_status.robot.status;
        base_status.robot.status = Mod_Magnetic;
    }
}

// 二维码
void set_client_tracker_start_goal(int target_id, bool mode)
{
    apriltag_tracker::action1Goal goal;
    goal.id = target_id;
    goal.start = mode;
    client_tracker_start_ptr->sendGoal(goal,  &doneCb3, &activeCb3, &feedbackCb3);
}
/*******************************************************************************************************************************************************/
//===============================================================
// 语法格式：    std::string read_json_file(char *filename)
// 实现功能：    读取json文件数据
// 入口参数：    文件名
// 出口参数：    返回json文件内容
//===============================================================
std::string read_json_file(std::string filename)
{
    Json::Value root;
    Json::Reader reader;
    Json::FastWriter swriter;
    std::string str;
    
    std::ifstream ifs(filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, root))
    {
        //ROS_ERROR("json_analysis error");
        PLOG_ERROR<<"json_analysis error";
        ifs.close();
        //return 0;
    }
    else
    {
        ifs.close();
        return swriter.write(root);
    }
   
    return str;
}

//===============================================================
// 语法格式：    void read_json_file_to_json(std::string filename,Json::Value *root)
// 实现功能：    读取json文件数据
// 入口参数：    文件名 Json文件地址
// 出口参数：    无
//===============================================================
void read_json_file_to_json(std::string filename,Json::Value *root)
{
    Json::Reader reader;
    Json::FastWriter swriter;
    
    std::ifstream ifs(filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
       PLOG_INFO<<filename<<"file is already open";
    }
    
    if(!reader.parse(ifs, *root))
    {
        //ROS_ERROR("json_analysis error");
        PLOG_ERROR<<"json_analysis error";
        ifs.close();
    }
    else
    {
        ifs.close();
    }
}

void json_to_mqtt_pub(struct mg_connection *c,char *topic,Json::Value root,int s_qos)
{
    if(root.empty() == false)
    {
        //pub
        std::string str;
        Json::FastWriter swriter;
        try
        {
            /* code */;
            str = swriter.write(root);
            //std::cout << str << std::endl;
            mg_mqtt_pub(c, mg_str(topic), mg_str(str.c_str()), s_qos, false);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
     }
     else
     {
        MG_INFO(("Json  is empty"));
     }

}

//启动导航
void start_nav(void)
{
    //防止在启动过程中订阅到地图导致显示错误
    deal_back.sub_map_flag = false;
    base_status.robot.status = Mod_Free;
    kill_nodel();
    //标记没定位
    base_status.local.carto = false;
    ros::Duration(2.5).sleep(); 
    //sleep(2);
    int ret = pthread_create(&thread_move_base, NULL, run_move_base, NULL);  //创建线程
    pthread_detach(thread_move_base); // 线程分离，结束时自动回收资源
    if(ret)
    {
        //ROS_ERROR("move base start error");
        PLOG_ERROR<<"move base start error";
    }
    //防止在启动过程中订阅到地图导致显示错误
    deal_back.sub_map_flag = true;
    //等待虚拟墙服务启动
    //sleep(3);
    ros::Duration(2.5).sleep(); 
    load_vitual_wall_json();
    
}

//启动建图
void start_slam(void)
{
    base_status.robot.status = Mod_Slam;
    kill_nodel();
    base_status.local.carto = false;
    //sleep(2);
     ros::Duration(2.5).sleep(); 
    int ret = pthread_create(&thread_slam_karto, NULL, run_slam_karto ,NULL);  //创建线程
    pthread_detach(thread_slam_karto); // 线程分离，结束时自动回收资源
    //等待服务启动
    //sleep(3);
}

//启动顺序巡巡航
void start_Order_Nav(double time,int id)
{
    Json::Reader reader;
    //clear
    //json_interest_point.clear();
    std::ifstream ifs(interest_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
       PLOG_INFO<<interest_point_filename<<"file is already open";
    }
    
    if(!reader.parse(ifs, json_interest_point))
    {
        //ROS_ERROR("start_Order_Nav error");
        PLOG_ERROR<<"start_Order_Nav error";
        ifs.close();

         base_status.robot.main_error = Mod_Order_Interest;
         base_status.robot.sub_error  = 1;
         base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_interest_point["point"].size() == 0)
    {
        //ROS_ERROR("error have no interest point");
        PLOG_ERROR<<"error have no interest point";
        base_status.robot.main_error = Mod_Order_Interest;
        base_status.robot.sub_error  = 2;
        base_status.robot.status = Mod_Error;
        return;
    }
    Order_Interest_index = id;
    if(id > json_interest_point["point"].size())
    {
        //ROS_ERROR("id more the interest point size!");
        PLOG_ERROR<<"id more the interest point size!";
        base_status.robot.main_error = Mod_Order_Interest;
        base_status.robot.sub_error  = 3;
        base_status.robot.status = Mod_Error;
        return;
    }
    geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Order_Interest_index]["z"].asDouble());
    set_goal("map",json_interest_point["point"][Order_Interest_index]["x"].asDouble(),json_interest_point["point"][Order_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
    //
    //导航启动反馈反馈
    task_feedback_set("nav",Order_Interest_index,"","start",last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
    //set timer
    movebase_timer.setPeriod(ros::Duration(time),true);
    base_status.robot.status = Mod_Order_Interest;
}

//启动随机巡航
void start_Random_Nav(double time)
{
    Json::Reader reader;
    //clear
    //json_interest_point.clear();
    std::ifstream ifs(interest_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_interest_point))
    {
        //ROS_ERROR("start_Order_Nav error");
        PLOG_ERROR<<"start_Order_Nav error";
        ifs.close();

        base_status.robot.main_error = Mod_Random_Interest;
        base_status.robot.sub_error  = 1;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_interest_point["point"].size() == 0)
    {
        //ROS_ERROR("error have no interest point");
        PLOG_ERROR<<"error have no interest point";
        base_status.robot.main_error = Mod_Random_Interest;
        base_status.robot.sub_error  = 2;
        base_status.robot.status = Mod_Error;
        return;
    }

    int index = rand() % json_interest_point["point"].size();
    Random_Interest_index = index;
    geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][index]["z"].asDouble());
    set_goal("map",json_interest_point["point"][index]["x"].asDouble(),json_interest_point["point"][index]["y"].asDouble(),goal_quat.z,goal_quat.w);

    //导航启动反馈反馈
    task_feedback_set("nav",Random_Interest_index,"","start",last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);

    //set timer
    movebase_timer.setPeriod(ros::Duration(time),true);

    base_status.robot.status = Mod_Random_Interest;
}

//go charge
void start_GoCharge(int time)
{
    //取消之前的目标
    actionlib_msgs::GoalID cancel_goal;
    movebase_cancel_pub.publish(cancel_goal);

    Json::Reader reader;
    //clear
    //json_charge_point.clear();
    std::ifstream ifs(charge_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_charge_point))
    {
        //ROS_ERROR("start_GoCharge error");
        PLOG_ERROR<<"start_GoCharge error";
        ifs.close();

        base_status.robot.main_error = Mod_Charge;
        base_status.robot.sub_error  = 1;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_charge_point["point"].size() == 0)
    {
        //ROS_ERROR("error have no charge point");
        PLOG_ERROR<<"error have no charge point";
        base_status.robot.main_error = Mod_Charge;
        base_status.robot.sub_error  = 2;
        base_status.robot.status = Mod_Error;
        return;
    }

    int index = 0;
    geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_charge_point["point"][index]["z"].asDouble());
    set_goal("map",json_charge_point["point"][index]["x"].asDouble(),json_charge_point["point"][index]["y"].asDouble(),goal_quat.z,goal_quat.w);

     //导航启动反馈反馈
    task_feedback_set("charge",0,"","start",last_goal.pose.position.x,last_goal.pose.position.y,tf2::getYaw(last_goal.pose.orientation),base_status.pose.x,base_status.pose.y,base_status.pose.yaw);

    base_status.robot.last_status = base_status.robot.status;
    base_status.robot.status = Mod_Charge;
}

//exit charge
void exit_GoCharge()
{
    //取消导航
    move_base_cancel();
    charge_control(0);
}

//start location
void  start_Location(int id)
{
    Json::Reader reader;
    //clear
    //json_location_point.clear();
    std::ifstream ifs(location_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_location_point))
    {
        //ROS_ERROR("start_GoCharge error");
        PLOG_ERROR<<"start location error";
        ifs.close();

        base_status.robot.main_error = Mod_Location;
        base_status.robot.sub_error  = 0;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_location_point["point"].size() == 0)
    {
        //ROS_ERROR("error have no location point");
        PLOG_ERROR<<"error have no location point";
        base_status.robot.main_error = Mod_Location;
        base_status.robot.sub_error  = 0;
        base_status.robot.status = Mod_Error;
        return;
    }

   set_client_scan_icp_goal(0,id);
   task_feedback_set("location",id,"","start",0,0,0,0,0,0);

    base_status.robot.last_status = base_status.robot.status;
    base_status.robot.status = Mod_Location;
}

//start location 
//task_mode 0:对准， 1:退后
void  start_more_task_Location(int task_mode,int id)
{
    Json::Reader reader;
    //clear
    //json_location_point.clear();
    std::ifstream ifs(location_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_location_point))
    {
        //ROS_ERROR("start_GoCharge error");
        PLOG_ERROR<<"start_GoCharge error";
        ifs.close();

        base_status.robot.main_error = Mod_Location;
        base_status.robot.sub_error  = 0;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_location_point["point"].size() == 0)
    {
        //ROS_ERROR("error have no location point");
        PLOG_ERROR<<"error have no location point";
        base_status.robot.main_error = Mod_Location;
        base_status.robot.sub_error  = 0;
        base_status.robot.status = Mod_Error;
        return;
    }

    int index = id;
   set_client_scan_icp_goal(task_mode,id);

    base_status.robot.last_status = base_status.robot.status;
}

void stop_Location(void)
{

    set_client_scan_icp_goal(1,location.start_id);
}

//启动特定巡航点
void start_more_task_interest_point(int point_index)
{
    Json::Reader reader;
    //clear
    //json_interest_point.clear();
    std::ifstream ifs(interest_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_interest_point))
    {
        //ROS_ERROR("start_more_task_interest_point error");
        PLOG_ERROR<<"start_more_task_interest_point error";
        ifs.close();
        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 1;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_interest_point["point"].size() == 0)
    {
        //ROS_ERROR("error have no interest point");
        PLOG_ERROR<<"error have no interest point";
        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 2;
        base_status.robot.status = Mod_Error;
        return;
    }

    geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][point_index]["z"].asDouble());
    set_goal("map",json_interest_point["point"][point_index]["x"].asDouble(),json_interest_point["point"][point_index]["y"].asDouble(),goal_quat.z,goal_quat.w);

}

void start_more_task_track_first(int id,int dir)
{
    Json::Reader reader;
    //clear
    //json_track_point.clear();
    std::ifstream ifs(trajectory_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_track_point))
    {
        //ROS_ERROR("start_track error");
        PLOG_ERROR<<"start_track error";
        ifs.close();

        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 3;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_track_point["track_dis"].size() < 1)
    {
        //ROS_ERROR("error have no trajectory point");
        PLOG_ERROR<<"error have no trajectory point";
        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 4;
        base_status.robot.status = Mod_Error;
        return;
    }

    if(dir == 2)
    {
        int size = json_track_point["track_dis"][id].size();
        if(size <= 2)
        {
            //ROS_ERROR("error trajectory point nide mroe two");
            PLOG_ERROR<<"error trajectory point nide mroe two";
            base_status.robot.main_error = Mod_More_Task;
            base_status.robot.sub_error  = 5;
            base_status.robot.status = Mod_Error;
            return;
        }
        
        track.first_point.x = json_track_point["track_dis"][id][(int)0]["x"].asDouble();
        track.first_point.y = json_track_point["track_dis"][id][(int)0]["y"].asDouble();

        track.second_point.x = json_track_point["track_dis"][id][1]["x"].asDouble();
        track.second_point.y = json_track_point["track_dis"][id][1]["y"].asDouble();
    }
    else
    {
        int size = json_track_point["track_dis"][id].size();
        if(size < 3)
        {
            //ROS_ERROR("error trajectory point nide mroe three");
            PLOG_ERROR<<"error trajectory point nide mroe three";
            base_status.robot.main_error = Mod_More_Task;
            base_status.robot.sub_error  = 5;
            base_status.robot.status = Mod_Error;
            return;
        }

        track.first_point.x = json_track_point["track_dis"][id][size-1]["x"].asDouble();
        track.first_point.y = json_track_point["track_dis"][id][size-1]["y"].asDouble();

        track.second_point.x = json_track_point["track_dis"][id][size-2]["x"].asDouble();
        track.second_point.y = json_track_point["track_dis"][id][size-2]["y"].asDouble();
    }
    

    double theta = atan2((track.second_point.y-track.first_point.y), (track.second_point.x-track.first_point.x)); //

    geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(theta);
    set_goal("map",track.first_point.x,track.first_point.y,goal_quat.z,goal_quat.w);

    track.finish_goal = 5;
     //启动了为步骤0
    track.track_pause_step = 0;
    //base_status.robot.status = Mod_Trajectory;
}

//start  2:正向运行 3:逆向运行
void start_more_task_track(int id,int dir)
{
    Json::Reader reader;
    //clear
    //json_track_point.clear();
    std::ifstream ifs(trajectory_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_track_point))
    {
        //ROS_ERROR("start_track error");
        PLOG_ERROR<<"start_track error";
        ifs.close();
        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 3;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_track_point["track_dis"].size() == 0)
    {
        //ROS_ERROR("error have no trajectory point");
        PLOG_ERROR<<"error have no trajectory point";
        base_status.robot.main_error = Mod_More_Task;
        base_status.robot.sub_error  = 4;
        base_status.robot.status = Mod_Error;
        return;
    }

    geometry_msgs::PoseArray track_point;
    geometry_msgs::Pose pose;
    geometry_msgs::Quaternion goal_quat;
    
    int index = id;

    if(dir == Forward_Direction)
    { 
        //防止越界
        int min_i = 0;
        if(json_track_point["track_dis"][index].size() == 1)
        {
            min_i = 0;
        }

        for(int i=0;i<json_track_point["track_dis"][index].size();i++)
        {
            pose.position.x = json_track_point["track_dis"][index][i]["x"].asDouble();
            pose.position.y = json_track_point["track_dis"][index][i]["y"].asDouble();
             goal_quat  = tf::createQuaternionMsgFromYaw(json_track_point["track_dis"][index][i]["theta"].asDouble());
            pose.orientation = goal_quat;
            track_point.poses.push_back(pose);
        }
    }
    else if(dir == Reverse_Direction)
    {
        //倒数第二个点可能离终点很近+1  防止越界
        int min_i = json_track_point["track_dis"][index].size()-1;
        if(min_i < 0){min_i = 0;}

        for(int i=min_i;i>=0;i--)
        {
            pose.position.x = json_track_point["track_dis"][index][i]["x"].asDouble();
            pose.position.y = json_track_point["track_dis"][index][i]["y"].asDouble();
            if(i != 0)
            {
                goal_quat  = tf::createQuaternionMsgFromYaw(json_track_point["track_dis"][index][i]["theta"].asDouble() - 3.141592);
            }
            else
            {
                goal_quat  = tf::createQuaternionMsgFromYaw(json_track_point["track_dis"][index][i]["theta"].asDouble());
            }
            pose.orientation = goal_quat;
            track_point.poses.push_back(pose);
        }
    }

    int size = track_point.poses.size();
    if(size)
    {
        last_goal.pose = track_point.poses[size-1];
    }
    trajectory_point_pub.publish(track_point);
    ros::param::set("/move_base/TebLocalPlannerROS/plan_need_clear",true);

     //设置轨迹模式
    set_trajectory_goal();
    //启动了为步骤1
    track.track_pause_step = 1;
}

//多任务启动
void start_more_task(void)
{
    //暂停解除
    if(more_task.pause_flag == true)
    {
        more_task.pause_flag = false;
        if(track.track_pause_step == 2)
        {
            track.track_pause_step = 3;
        }
    }
    else
    {
        track.track_pause_step = 0;
    }
    //else
    {
        Json::Reader reader;
        //clear
        //json_more_task.clear();
        std::ifstream ifs(more_task_filename.c_str(), std::ifstream::in);//only read
        if(ifs.is_open())
        {
           std::cout<<"file is already open"<<endl;
        }
        
        if(!reader.parse(ifs, json_more_task))
        {
            //ROS_ERROR("start_more_task error");
            PLOG_ERROR<<"start_more_task error";
            ifs.close();

            base_status.robot.main_error = Mod_More_Task;
            base_status.robot.sub_error  = 6;
            base_status.robot.status = Mod_Error;
            return;
        }
        ifs.close();

        if(json_more_task["task"].size() == 0)
        {
            //ROS_ERROR("error have no task point");
            PLOG_ERROR<<"error have no task point";
            base_status.robot.main_error = Mod_More_Task;
            base_status.robot.sub_error  = 7;
            base_status.robot.status = Mod_Error;
            return;
        }

        //nav point
        if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
        {
            start_more_task_interest_point(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
        }
        //track point
        else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "track")
        {
            int id = json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt();
            int dir = 0;
            if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
            {
                dir = 2;
            }
            else  //back
            {
                dir = 3;
            }

            //track.track_pause_step 循迹启动了为步骤1 取消了为步骤2 在暂停标志下再启动为步骤3
            if(track.track_pause_step == 3)
            {
                start_more_task_track(id,dir);
            }
            else
            {
                start_more_task_track_first(id,dir);
            }
        }
        else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "loc")
        {
            if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
            {
                    start_more_task_Location(0,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
            }
            else  //back
            {
                start_more_task_Location(1,json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
            }
        }
        // 二维码
        // qr_location point
        else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "qr")
        {
            if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
            {
                start_more_task_qr_Location(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
            }
            else  //back
            {
                stop_qr_Location();
            }
        }
        
    }
}

//多任务暂停
void pause_more_task(void)
{
    more_task.pause_flag = true;
    stop_track();

    //如果循迹启动了则标记为取消
    if(track.track_pause_step == 1)
    {
        track.track_pause_step = 2;
    }
    else
    {
        track.track_pause_step = 0;
    }
    base_status.robot.sub_error = -1;
}

//多任务停止
void stop_more_task(void)
{
    more_task.main_task = 0;
    more_task.sub_task = 0;
    more_task.pause_flag = false;
    stop_track();
    
}

//运行速度设置
void set_run_speed(double speed)
{
    double set_speed = 0.5;
    set_speed = speed;
    if(set_speed > 0.8)
    {
        set_speed = 0.8;
    }

    if(set_speed < 0.1)
    {
        set_speed = 0.1;
    }
    //动态参赛服务器设置才行
    //ros::param::set("/move_base/TebLocalPlannerROS/max_vel_x",set_speed);
    std::string str="rosrun dynamic_reconfigure dynparam set /move_base/TebLocalPlannerROS max_vel_x  " + std::to_string(set_speed);
    const char *p = str.c_str();//同上，要加const或者等号右边用char*
    int status = system(p);
    //ROS_WARN("set run speed %.2f",set_speed);
    PLOG_WARNING<<"set run speed:"<<set_speed;
}

/*****************************************deal***************************************************************/

void deal_move_cmd(Json::Value value)
{

    double vx=0.0,vy=0.0,vz=0.0;
    geometry_msgs::Twist cmd_vel;

    //导航模式下
    if(base_status.robot.status == Mod_Free || base_status.robot.status == Mod_Slam)
    {

        vx = value["vx"].asDouble();
        vy = value["vy"].asDouble();
        vz = value["vz"].asDouble();
        
        cmd_vel.linear.x = vx;
        cmd_vel.linear.y = vy;
        cmd_vel.angular.z = vz;
        
        //pub
        cmd_vel_pub.publish(cmd_vel);

        //printf("get move vx:%.2f vy:%.2f vz:%.2f\n",vx,vy,vz);
    }
}
//
void deal_interest_point_control_cmd(Json::Value value)
{
    if(value["cmd"] == "delete")
    {
        std::ofstream ofs;
        ofs.open(interest_point_filename);
        ofs.close();
    }
    else if(value["cmd"] == "start")
    {
        //导航模式下
        if(base_status.robot.status == Mod_Free)
        {
            setting.move_base_timer = value["time"].asDouble();
            nav.stop_time = value["time"].asDouble();
            nav.start_index = value["id"].asInt();
            nav.circul_flag = value["circulates"].asBool(); 
            nav.path_mode_flag = value["path_mode"].asBool(); 
            nav.half_path_stop_time = value["path_stop_time"].asDouble();
            set_run_speed(value["run_speed"].asDouble());
            //赋值避障停车时间
            track.track_stop_time = nav.half_path_stop_time;
            start_Order_Nav(nav.stop_time,nav.start_index);
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("interest_point_error");
            //send
            send_que.push(Json_Feedback);
        }
    }
    else if(value["cmd"] == "random")
    {
        //导航模式下
        if(base_status.robot.status == Mod_Free)
        {
            setting.move_base_timer = value["time"].asDouble();
            nav.stop_time = value["time"].asDouble();
            nav.start_index = value["id"].asInt();
            nav.circul_flag = value["circulates"].asBool();
            nav.path_mode_flag = value["path_mode"].asBool(); 
            set_run_speed(value["run_speed"].asDouble());
            nav.half_path_stop_time = value["path_stop_time"].asDouble();
            //赋值避障停车时间
            track.track_stop_time = nav.half_path_stop_time;
            start_Random_Nav(nav.stop_time);
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("interest_point_error");
            //send
            send_que.push(Json_Feedback);
        }
    }
    else if(value["cmd"] == "stop")
    {
        //取消导航
        move_base_cancel();
        base_status.robot.last_status = Mod_Free;
        task_feedback_set("nav",0,"","stop",0,0,0,0,0,0);
    }
    else if(value["cmd"] == "get")
    {
        read_json_file_to_json(interest_point_filename,&json_interest_point);
        //std::cout << json_interest_point << std::endl;
        //send
        send_que.push(Json_Interest_point);
    }
}

void deal_interest_point_add_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(interest_point_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("interest_point_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer interest point sucess!\n");

}

void deal_charge_point_control_cmd(Json::Value value)
{
    if(value["cmd"] == "set")
    {
        Json::Value value;
        Json::Value point;
        Json::StyledWriter swriter;
        std::ofstream ofs;
        std::string str;

        value["cmd_type"] = Json::Value("charge_point");
        value["cmd"]      = Json::Value("set");
  
        point["x"]   = Json::Value(0.0);
        point["y"]   = Json::Value(0.0);
        point["yaw"] = Json::Value(0.0);
        value["point"]    = point;

        str = swriter.write(value);
        ofs.open(charge_point_filename);
        ofs << str;
        ofs.close();
        printf("set charge point sucess!\n");
    }

    else if(value["cmd"] == "get")
    {
        read_json_file_to_json(charge_point_filename,&json_charge_point);
        //std::cout << json_charge_point << std::endl;
        //send
        send_que.push(Json_Charge_point);
    }

    else if(value["cmd"] == "goto")
    {
        //不在建图模式下和循迹模式下
        if(base_status.robot.status != Mod_Slam && base_status.robot.status != Mod_Trajectory)
        {
            setting.go_charge_error_time = value["time"].asInt();
            nav.path_mode_flag = value["path_mode"].asBool(); 
            set_run_speed(value["run_speed"].asDouble());
            start_GoCharge(setting.go_charge_error_time);
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("goto_charge_error");
            //send
            send_que.push(Json_Feedback);
        }
       
    }
    else if(value["cmd"] == "stop")
    {
       exit_GoCharge();
        task_feedback_set("charge",0,"","stop",0,0,0,0,0,0);
    }
    
}

void deal_charge_point_add_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(charge_point_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("charge_point_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer charge point sucess!\n");
}

void deal_location_point_control_cmd(Json::Value value)
{
    if(value["cmd"] == "set")
    {
        int id = value["id"].asInt();
         //由于同一个进程内的所有线程共享内存和变量，因此在传递参数时需作特殊处理，值传递。
        int ret = pthread_create(&thread_set_location, NULL, run_set_location, &id);  //创建线程
        pthread_detach(thread_set_location); // 线程分离，结束时自动回收资源
        PLOG_INFO<<"set location id:"<< id;
    }

    else if(value["cmd"] == "get")
    {
        read_json_file_to_json(location_point_filename,&json_location_point);
        //std::cout << json_location_point << std::endl;
        //send
        send_que.push(Json_Location_point);
    }

    else if(value["cmd"] == "start")
    {
        //空闲模式下
        if(base_status.robot.status == Mod_Free)
        {
            base_status.robot.status = Mod_Location;
            start_Location(value["id"].asInt());
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("location_start_error");
            //send
            send_que.push(Json_Feedback);
        }
       
    }
    else if(value["cmd"] == "stop")
    {
       //定位模式下
        if(base_status.robot.status == Mod_Location)
        {
            stop_Location();
            task_feedback_set("location",0,"","stop",0,0,0,0,0,0);
        }
        else if(base_status.robot.status == Mod_Error)
        {
            base_status.robot.status = Mod_Free;
        }
    }
}

void deal_location_point_add_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(location_point_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("location_point_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer location point sucess!\n");

}

//二维码
void deal_qr_cmd(Json::Value value)
{
    if(value["cmd"] == "set")
    {
        if(value["cmd_type"] == "qr_location_point_control")
        {
            int id = value["id"].asInt();
            PLOG_INFO<<"set location id:"<< id;
            run_set_qr_location(id);
        }
    }
    
    else if(value["cmd"] == "get")
    {
        if(value["cmd_type"] == "qr_location_point_control")
        {
            read_json_file_to_json(qr_location_point_filename,&json_qr_pose);
            send_que.push(Json_Qr);
        }
    }

    else if(value["cmd"] == "start")
    {
        //空闲模式下
        if(base_status.robot.status == Mod_Free)
        {
            base_status.robot.status = Mod_Qr_Location;
            start_qr_Location(value["id"].asInt());
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("qr_location_start_error");
            //send
            send_que.push(Json_Feedback);
        }
    }

    else if(value["cmd"] == "stop")
    {
        stop_qr_Location();
        task_feedback_set("qr_tracker",0,"","stop",0,0,0,0,0,0);
    }
}

//二维码
void deal_qr_add_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(qr_location_point_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("qr_location_point_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer qr location point sucess!\n");

}

void start_track_first(int id,bool dir)
{
    Json::Reader reader;
    //clear
    //json_track_point.clear();
    std::ifstream ifs(trajectory_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_track_point))
    {
        //ROS_ERROR("start_track error");
        PLOG_ERROR<<"start_track error";
        ifs.close();
        base_status.robot.main_error = Mod_Trajectory;
        base_status.robot.sub_error  = 1;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_track_point["track_dis"].size() < 1)
    {
        //ROS_ERROR("error have no trajectory point");
        PLOG_ERROR<<"error have no trajectory point";
        base_status.robot.main_error = Mod_Trajectory;
        base_status.robot.sub_error  = 2;
        base_status.robot.status = Mod_Error;
        return;
    }

    if(dir == false)
    {
        int size = json_track_point["track_dis"][id].size();
        if(size <= 2)
        {
            //ROS_ERROR("error trajectory point nide mroe two");
            PLOG_ERROR<<"error trajectory point nide mroe two";
            base_status.robot.main_error = Mod_More_Task;
            base_status.robot.sub_error  = 5;
            base_status.robot.status = Mod_Error;
            return;
        }
        
        track.first_point.x = json_track_point["track_dis"][id][(int)0]["x"].asDouble();
        track.first_point.y = json_track_point["track_dis"][id][(int)0]["y"].asDouble();

        track.second_point.x = json_track_point["track_dis"][id][1]["x"].asDouble();
        track.second_point.y = json_track_point["track_dis"][id][1]["y"].asDouble();
    }
    else
    {
        int size = json_track_point["track_dis"][id].size();
        if(size < 3)
        {
            //ROS_ERROR("error trajectory point nide mroe three");
            PLOG_ERROR<<"error trajectory point nide mroe three";
            base_status.robot.main_error = Mod_More_Task;
            base_status.robot.sub_error  = 5;
            base_status.robot.status = Mod_Error;
            return;
        }

        track.first_point.x = json_track_point["track_dis"][id][size-1]["x"].asDouble();
        track.first_point.y = json_track_point["track_dis"][id][size-1]["y"].asDouble();
        //防止最后一个点与倒数第二个点太近
        track.second_point.x = json_track_point["track_dis"][id][size-2]["x"].asDouble();
        track.second_point.y = json_track_point["track_dis"][id][size-2]["y"].asDouble();
    }

    /*
    track.first_point.x = json_track_point["track_dis"][id][(int)0]["x"].asDouble();
    track.first_point.y = json_track_point["track_dis"][id][(int)0]["y"].asDouble();

    track.second_point.x = json_track_point["track_dis"][id][1]["x"].asDouble();
    track.second_point.y = json_track_point["track_dis"][id][1]["y"].asDouble();
    */

    double theta = atan2((track.second_point.y-track.first_point.y), (track.second_point.x-track.first_point.x)); //

    geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(theta);
    set_goal("map",track.first_point.x,track.first_point.y,goal_quat.z,goal_quat.w);

    track.finish_goal = 5;
    base_status.robot.status = Mod_Trajectory;

}
//start  id 第几条轨迹 //dir 2:正向运行 3:逆向运行
void start_track(int id,int dir)
{
    Json::Reader reader;
    //clear
    //json_track_point.clear();
    std::ifstream ifs(trajectory_point_filename.c_str(), std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<endl;
    }
    
    if(!reader.parse(ifs, json_track_point))
    {
        //ROS_ERROR("start_track error");
        PLOG_ERROR<<"start_track error";
        ifs.close();
        base_status.robot.main_error = Mod_Trajectory;
        base_status.robot.sub_error  = 1;
        base_status.robot.status = Mod_Error;
        return;
    }
    ifs.close();

    if(json_track_point["track_dis"].size() == 0)
    {
        //ROS_ERROR("error have no trajectory point");
        PLOG_ERROR<<"error have no trajectory point";
        base_status.robot.main_error = Mod_Trajectory;
        base_status.robot.sub_error  = 2;
        base_status.robot.status = Mod_Error;
        return;
    }

    geometry_msgs::PoseArray track_point;
    geometry_msgs::Pose pose;
    geometry_msgs::Quaternion goal_quat;
    
    
    int index = id;

    if(dir == Forward_Direction)
    { 
        //防止越界
        int min_i = 0;
        if(json_track_point["track_dis"][index].size() == 1)
        {
            min_i = 0;
        }

        for(int i=0;i<json_track_point["track_dis"][index].size();i++)
        {
            pose.position.x = json_track_point["track_dis"][index][i]["x"].asDouble();
            pose.position.y = json_track_point["track_dis"][index][i]["y"].asDouble();
            goal_quat = tf::createQuaternionMsgFromYaw(json_track_point["track_dis"][index][i]["theta"].asDouble());
            pose.orientation = goal_quat;
            track_point.poses.push_back(pose);
        }
    }
    else if(dir == Reverse_Direction)
    {
        //倒数第二个点可能离终点很近+1  防止越界
        int min_i = json_track_point["track_dis"][index].size()-1;
        if(min_i < 0){min_i = 0;}

        for(int i=min_i;i>=0;i--)
        {
            pose.position.x = json_track_point["track_dis"][index][i]["x"].asDouble();
            pose.position.y = json_track_point["track_dis"][index][i]["y"].asDouble();
            //秒和纳秒逆转，最后一个不逆转
            if(i != 0)
            {
                goal_quat = tf::createQuaternionMsgFromYaw(json_track_point["track_dis"][index][i]["theta"].asDouble()- 3.141592);
            }
            else
            {
                goal_quat = tf::createQuaternionMsgFromYaw(json_track_point["track_dis"][index][i]["theta"].asDouble());
            }
            pose.orientation = goal_quat;
            track_point.poses.push_back(pose);
        }
    }
    
    trajectory_point_pub.publish(track_point);
    ros::param::set("/move_base/TebLocalPlannerROS/plan_need_clear",true);
    
    int size = track_point.poses.size();
    if(size)
    {
        last_goal.pose = track_point.poses[size-1];
    }
    
    //设置轨迹模式
    set_trajectory_goal();
    base_status.robot.status = Mod_Trajectory;
}

//stop
void stop_track(void)
{
    try
    {
        //取消导航
        move_base_cancel();
        //取消末端定位
        stop_Location();
    }
    catch (std::exception e ) 
    {

    }
    track_stop_timer.stop();
    base_status.robot.status = Mod_Free;
    //轨迹模式避障停车
    base_status.robot.main_error = Mod_Free;
    base_status.robot.sub_error  = 0;
}

void deal_trajectory_point_control_cmd(Json::Value value)
{
    if(value["cmd"] == "get")
    {
        read_json_file_to_json(trajectory_point_filename,&json_track_point);
        //std::cout << json_track_point << std::endl;
        //send
        send_que.push(Json_Track_point);
    }
    else if(value["cmd"] == "start")
    {
        //不在建图模式下
        if(base_status.robot.status == Mod_Free)
        {
            //正向运行
            track.start_index = value["id"].asInt();
            track.circul_flag = value["circulates"].asBool();
            track.dir_flag    = value["dir"].asBool();
            //为0则避障停车无限等待
            track.track_stop_time = value["stop_time"].asInt();
            set_run_speed(value["run_speed"].asDouble());
            nav.path_mode_flag = false;
            start_track_first(track.start_index,track.dir_flag);

             //轨迹启动反馈反馈
             std::string star_dir;
             if(track.dir_flag)
             {
                star_dir = "forward";
             }
             else
             {
                star_dir = "back";
             }
            task_feedback_set("track",track.start_index,"","start",0,0,0,0,0,0);

            //start_track(track.start_index,2);
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("track_start_error");
            //send
            send_que.push(Json_Feedback);
        }

       
    }
    else if(value["cmd"] == "stop")
    {
        stop_track();
        task_feedback_set("track",track.start_index,"","stop",0,0,0,0,0,0);
    }
}

void deal_trajectory_point_creat_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(trajectory_point_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("trajectory_point_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer trajectory point sucess!\n");
}

void deal_path_point_creat_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(path_point_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("path_point_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer path point sucess!\n");
}

void deal_path_point_control_cmd(Json::Value value)
{
     if(value["cmd"] == "get")
    {
        read_json_file_to_json(path_point_filename,&json_path_point);
        //std::cout << json_path_point << std::endl;
        //send
        send_que.push(Json_Path_point);
    }
}

// 通过stat结构体 获得文件大小，单位字节
unsigned int getFileSize(const char *fileName) 
{
	if (fileName == NULL) return 0;
	
	// 这是一个存储文件(夹)信息的结构体，其中有文件大小和创建时间、访问时间、修改时间等
	struct stat statbuf;

	// 提供文件名字符串，获得文件属性结构体
	stat(fileName, &statbuf);
	
	// 获取文件大小
	unsigned int filesize = statbuf.st_size;
	return filesize;
}

void deal_slam_map_control_cmd(Json::Value value)
{
    if(value["cmd"] == "start")
    {
        //不在建图模式下
        if(base_status.robot.status == Mod_Free)
        {
            int ret_backup = pthread_create(&thread_backup_map, NULL, run_backup_map, NULL); 
      	    pthread_detach(thread_backup_map);

            add_slam_mode = value["add_map"].asBool();
            if(add_slam_mode == false)
            {
                start_slam();
            }
            else    //add mapp
            {
                start_slam();
                //延时2.5
                ros::Duration(2.5).sleep(); 
                //加载当前位置
                load_save_pose_json();
            }
            
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("slam_start_error");
            //send
            send_que.push(Json_Feedback);
        }
         
    }
    else if(value["cmd"] == "stop")
    {
         if(base_status.robot.status == Mod_Slam)
        {
            const char *p = map_pgm_filename.c_str();
            if(getFileSize(p))
            {
      	        PLOG_ERROR<<"pgm size:"<< strerror(getFileSize(p));
            }
            else
            {
                int ret_restore = pthread_create(&thread_restore_map, NULL, run_restore_map, NULL); 
      	        pthread_detach(thread_restore_map); 
            }
            start_nav();
        }
    }
    else if(value["cmd"] == "save")
    {
        //建图模式下才能保存地图
        if(base_status.robot.status == Mod_Slam)
        {
      	    //由于同一个进程内的所有线程共享内存和变量，因此在传递参数时需作特殊处理，值传递。
      	    int ret = pthread_create(&thread_save_map, NULL, run_save_map, NULL);  //创建线程
      	    pthread_detach(thread_save_map); // 线程分离，结束时自动回收资源
        }
        else
        {
            //保存失败
            json_feedback["cmd_type"] = Json::Value("feedback");
      	    json_feedback["cmd"] = Json::Value("save_map_error");
      	    //send
      	    send_que.push(Json_Feedback);
        }
    }
    else if(value["cmd"] == "get")
    {
        /*
        if(send_que.back() != Json_Map)
        {
            send_que.push(Json_Map);
        }*/

        bool have_map_flag = false;
        //遍历队列
        int send_que_size = send_que.size();
        for(int i = 0; i < send_que_size; i++) 
        {   //send_que_size 必须是固定值
            if(send_que.front() == Json_Map)
            {
                //队列中还有未发送的map
                have_map_flag = true;
            }
            send_que.push(send_que.front());
            send_que.pop();
        }

        if(have_map_flag == false)
        {
            //send_que.push(Json_Map);
            map_index = 0;
            deal_back.up_map_flag = true;
        }
    }
    else if(value["cmd"] == "refining")
    {
    	//由于同一个进程内的所有线程共享内存和变量，因此在传递参数时需作特殊处理，值传递。
    	int ret = pthread_create(&thread_refining_map, NULL, run_refining_map, NULL);  //创建线程
    	pthread_detach(thread_refining_map); // 线程分离，结束时自动回收资源
    }
}

void deal_updata_cmd(Json::Value value)
{
    //空闲模式下才能更新系统
    if(base_status.robot.status == Mod_Free)
    {
      	//由于同一个进程内的所有线程共享内存和变量，因此在传递参数时需作特殊处理，值传递。
      	int ret = pthread_create(&thread_updata, NULL, run_updata, NULL);  //创建线程
      	pthread_detach(thread_updata); // 线程分离，结束时自动回收资源

        //更新成功
      	json_feedback["cmd_type"] = Json::Value("feedback");
      	json_feedback["cmd"] = Json::Value("updata_sucess");
      	//send
      	send_que.push(Json_Feedback);
     }
     else
     {
        //更新失败
        json_feedback["cmd_type"] = Json::Value("feedback");
      	json_feedback["cmd"] = Json::Value("updata_error");
      	//send
      	send_que.push(Json_Feedback);
     }
}

//true reset pose
//false cannel reset pose
void pub_reset_pose(bool cmd)
{
    std_msgs::UInt8 data;
    if(cmd == true)
    {
        data.data = 1;
    }
    else
    {
        data.data = 0;
    }
    reset_pose_pub.publish(data);
}

void deal_reset_cmd(Json::Value value)
{
    //导航模式下
    if(base_status.robot.status == Mod_Free)
    {
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("reset_doing");
        //send
        send_que.push(Json_Feedback);

        //pub reset pose
        pub_reset_pose(true);

        //标记没定位
        base_status.local.carto = false;

        //开启超时定时
        reset_pose_timer.start();  
        base_status.robot.doing_reset = true;
    }
    else
    {
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("reset_error");
        //send
        send_que.push(Json_Feedback);
    }
}

void deal_error_reset(Json::Value value)
{
    //错误模式下
    if(base_status.robot.status == Mod_Error)
    {
        //pt error clear
        std_msgs::UInt8 error_msg;
        std_msgs::Bool dt_error_msg;
        error_msg.data = 1;
        pt_error_clear_pub.publish(error_msg);
        four_error_clear_pub.publish(error_msg);
        error_msg.data = 0xff;
        agv_fault_clean_pub.publish(error_msg);

        dt_error_msg.data = true;
        dt_collision_clean_pub.publish(dt_error_msg);
        sleep(1);
        //exit charge
        exit_GoCharge();
        //取消导航
        //move_base_cancel();
        if(client_tracker_start_ptr->getState() == actionlib::SimpleClientGoalState::ACTIVE)
        {
            client_tracker_start_ptr->cancelGoal();
        }
       #ifdef LASER_CHARGE
        #else
        if(client_scan_icp_ptr->getState() == actionlib::SimpleClientGoalState::ACTIVE)
        {
            client_scan_icp_ptr->cancelGoal();
        }
        #endif
        base_status.robot.status = Mod_Free;
    }
    else
    {
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("error_reset_error");
        //send
        send_que.push(Json_Feedback);
    }
}

void deal_save_start_point(Json::Value value)
{
    //只有在各传感器正常才能保存当前位置，且不再见图模式下，只能在空闲模式下
    if(base_status.sensor.laser == true && base_status.sensor.imu == true && base_status.sensor.robot == true  && 
        base_status.robot.status != Mod_Slam && base_status.robot.status == Mod_Free)
    {
        updata_save_pose_json(base_status.pose.x,base_status.pose.y,base_status.pose.yaw);
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("save_start_point_sucess");
        //send
        send_que.push(7);
    }
    else
    {
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("save_start_point_error");
        //send
        send_que.push(7);
    }
}

void deal_invent_wall_control_cmd(Json::Value value)
{
    if(value["cmd"] == "delete")
    {
        //deal all vitual wall
        move_base_virtual_wall_server::DeleteWall srv;
        srv.request.id = value["id"].asInt();
        if (delete_wall_client.call(srv))//服务调用
        {
            //ROS_INFO("clear vitual wall");
            PLOG_INFO<<"clear vitual wall"<<value["id"].asInt();
        }
    }
    else if(value["cmd"] == "delete_all")
    {
        //deal all vitual wall
        move_base_virtual_wall_server::DeleteAll srv;
        if (delete_all_client.call(srv))//服务调用
        {
            //ROS_INFO("clear vitual wall");
            PLOG_INFO<<"clear vitual wall";
        }

        std::ofstream ofs;
        ofs.open(invent_wall_filename);
        ofs.close();
    }
    else if(value["cmd"] == "get")
    {
        read_json_file_to_json(invent_wall_filename,&json_invent_wall);
        //std::cout << json_invent_wall << std::endl;
        //send
        send_que.push(Json_Invent_wall_point);
    }
}

void deal_invent_wall_creat_cmd(Json::Value value)
{
    Json::Value array = value["point"];
    //deal all vitual wall
    move_base_virtual_wall_server::DeleteAll srv;
    //先清除所有虚拟墙再创建
    if (delete_all_client.call(srv))//服务调用
    {
        //ROS_INFO("clear vitual wall");
        PLOG_INFO<<"clear vitual wall";
    }

    for(int i=0;i<value["point"].size();i++)
    {
        move_base_virtual_wall_server::CreateWall srv;
        srv.request.start_point.x = array[i]["start_x"].asDouble();
        srv.request.start_point.y = array[i]["start_y"].asDouble();
        srv.request.end_point.x   = array[i]["end_x"].asDouble();
        srv.request.end_point.y   = array[i]["end_y"].asDouble();
        srv.request.id = i;
        create_wall_client.call(srv);
    }

    //save invent point
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(invent_wall_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
      json_feedback["cmd_type"] = Json::Value("feedback");
      json_feedback["cmd"] = Json::Value("invent_wall_sucess");
      //send
      send_que.push(Json_Feedback);
    }

    printf("writer invent wall sucess!\n");
}

void deal_goal_point_set(Json::Value value)
{
    //导航模式下
    if(base_status.robot.status == Mod_Free)
    {
        geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(value["z"].asDouble());
        set_goal("map",value["x"].asDouble(),value["y"].asDouble(),goal_quat.z,goal_quat.w);
        base_status.robot.status = Mod_Nav;
    }
    else
    {
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("interest_point_error");
        //send
        send_que.push(Json_Feedback);
    }
    
}

void deal_init_pose_set(Json::Value value)
{
    std_msgs::UInt8 msg;
    geometry_msgs::PoseWithCovarianceStamped pose;
    pose.header.frame_id = "map";
    pose.pose.pose.position.x = value["x"].asDouble();
    pose.pose.pose.position.y = value["y"].asDouble();

    geometry_msgs::Quaternion pose_quat = tf::createQuaternionMsgFromYaw(value["z"].asDouble());
    
    pose.pose.pose.orientation.z = pose_quat.z;
    pose.pose.pose.orientation.w = pose_quat.w;

    float factorPos = 0.03;
    float factorRot = 0.1;
    pose.pose.covariance[6*0+0] = (0.5 * 0.5) * factorPos;
    pose.pose.covariance[6*1+1] = (0.5 * 0.5) * factorPos;
    pose.pose.covariance[6*3+3] = (M_PI/12.0 * M_PI/12.0) * factorRot;
    
    initialpose1_pub.publish(pose);

    //标记没定位
    base_status.local.carto = false;
    //手动设置位置后，停止位置更新时间清0，让其更新10s
    stop_up_loca_time = 0;
    msg.data =0;
    reset_pose_pub.publish(msg);
}
//
void deal_setting(Json::Value value)
{
    setting.power_max        = value["power_max"].asDouble();
    setting.power_min        = value["power_min"].asDouble();
    
    setting.go_power         = value["go_power"].asInt();
    setting.exit_power       = value["exit_power"].asInt();
    
    setting.go_power_check   = value["go_power_check"].asBool();
    setting.exit_power_check = value["exit_power_check"].asBool();
  
    setting.radra_check      = value["radra_check"].asBool();
    setting.path_check       = value["path_check"].asBool();

    setting.move_base_timer  = value["nav_time"].asDouble();
    setting.go_charge_error_time  = value["go_power_error"].asInt();

    setting.start_point_check = value["start_point"].asBool();

    //save  setting
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(setting_filename);
    ofs << str;
    ofs.close();
}
//
void deal_more_task_control_cmd(Json::Value value)
{
    if(value["cmd"] == "start")
    {
        if(base_status.robot.status == Mod_Free || base_status.robot.status == Mod_More_Pause)
        {
            more_task.main_loop = value["main_loop"].asBool();
            more_task.sub_loop  = value["sub_loop"].asBool();
            more_task.main_task = value["main_task"].asInt();
            more_task.sub_task  = value["sub_task"].asInt();
            //为0则避障停车无限等待
            track.track_stop_time = value["stop_time"].asInt();
            set_run_speed(value["run_speed"].asDouble());
            //多人物模式不用轨道
            // //多人物模式不用轨道
            // nav.path_mode_flag = false;
            nav.path_mode_flag = value["path_mode"].asBool();;

            base_status.robot.status = Mod_More_Task;
            start_more_task();
        }
        else
        {
            json_feedback["cmd_type"] = Json::Value("feedback");
            json_feedback["cmd"] = Json::Value("more_task_error");
            //send
            send_que.push(Json_Feedback);
        }
        
    }
    else if(value["cmd"] == "pause")
    {
        if(base_status.robot.status == Mod_More_Task)
        {
            pause_more_task();
            base_status.robot.status = Mod_More_Pause;
        }
    }
    else if(value["cmd"] == "stop")
    {
        stop_more_task();
        base_status.robot.status = Mod_Free;
        base_status.robot.last_status = Mod_Free;
    }
    else if(value["cmd"] == "get")
    {
        read_json_file_to_json(more_task_filename,&json_more_task);
        //std::cout << json_more_task << std::endl;
        //send
        send_que.push(Json_More_task);
    }
}

//
void deal_more_task_save_cmd(Json::Value value)
{
    std::string str;
    Json::StyledWriter swriter;
    std::ofstream ofs;

    str = swriter.write(value);
    ofs.open(more_task_filename);
    ofs << str;
    ofs.close();

    if(base_status.robot.status != Mod_Slam)
    {
        json_feedback["cmd_type"] = Json::Value("feedback");
        json_feedback["cmd"] = Json::Value("more_task_sucess");
        //send
        send_que.push(Json_Feedback);
    }
    
    printf("writer more task sucess!\n");
}

void deal_get_slam_map(Json::Value value)
{
    map_index = value["index"].asInt();
    deal_back.up_map_flag = true;
    printf("map get map_index:%d\n",map_index);
}

void deal_software_stop(Json::Value value)
{
    std_msgs::UInt8 stop;
    stop.data = value["stop"].asBool();
    software_stop_pub.publish(stop);
    pt_software_stop_pub.publish(stop);
}

void deal_magnetic_nav(Json::Value value)
{
    unsigned char aim_id = 0,aim_dir = 0,aim_action = 0;
    aim_id = value["aim_id"].asUInt();
    aim_dir = value["aim_dir"].asUInt();
    aim_action = value["aim_action"].asUInt();
    printf("%d %d %d \n",aim_id,aim_dir,aim_action);
    //空闲模式下
    if(base_status.robot.status == Mod_Free || base_status.robot.status == Mod_Magnetic)
    {
        set_client_magnetic_nav_goal(aim_id,aim_dir,aim_action);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//电量监控
void power_monitoring(void)
{
    static int go_power_cnt = 0,exit_power_cnt = 0,time_cnt = 0 ,charge_error = 0,last_charge_status = 0,exit_charge_cnt = 0;
    static bool exit_charge_wait = false;

    //充电成功反馈
    if(base_status.robot.charge == 4 && base_status.robot.charge != last_charge_status)
    {
        task_feedback_set("charge",0,"","success",0,0,0,0,0,0);
    }
    //退出充电判断
    else if(base_status.robot.charge == 7 && base_status.robot.charge != last_charge_status)
    {
        exit_charge_wait = true;
    }
    if(base_status.robot.vx == 0 && exit_charge_wait == true)
    {
        exit_charge_cnt++;
        if(exit_charge_cnt > 10)
        {
             task_feedback_set("exit_charge",0,"","success",0,0,0,0,0,0);
             exit_charge_cnt = 0;
             exit_charge_wait = false;
        }
    }
    else
    {
        exit_charge_cnt = 0;
    }

    last_charge_status = base_status.robot.charge;

    //底盘正常通信
    if(base_status.sensor.robot == true)
    {
        //电量
        double power = (base_status.robot.power - setting.power_min) / (setting.power_max - setting.power_min) * 100;
        
        //低电回充
        if(setting.go_power_check == true)
        {
            //在巡航模式和轨迹模式和多任务模式下
            if(power < setting.go_power && base_status.robot.charge != 4 && 
              (base_status.robot.status == Mod_Order_Interest || base_status.robot.status == Mod_Random_Interest || base_status.robot.status == Mod_More_Task))
            {
                go_power_cnt++;
                if(go_power_cnt%5 == 0)
                {
                    //ROS_INFO("power is low will go charge num:%d",go_power_cnt);
                    PLOG_INFO<<"power is low will go charge num:"<<go_power_cnt;
                }
            }
            else
            {
                go_power_cnt = 0;
                more_task.low_power_flag = false;
            }
            //10s
            if(go_power_cnt > 100)
            {
                go_power_cnt = 100;

                //多任务模式下在任务切换时候处理低电量回充
                if(base_status.robot.status == Mod_More_Task)
                {
                    more_task.low_power_flag = true;
                }
                else
                {
                    start_GoCharge(setting.go_charge_error_time);
                }
                
            }
        }

        //高电退出 在自动回充接触情况下
        if(setting.exit_power_check == true)
        {
            //在回充模式下
            if(power >= setting.exit_power && base_status.robot.charge == 4 && base_status.robot.status == Mod_Charge)
            {
                exit_power_cnt++;
                if(exit_power_cnt%5 == 0)
                {
                    //ROS_INFO("power is height will exit charge num:%d",exit_power_cnt);
                    PLOG_INFO<<"power is height will exit charge num:"<<exit_power_cnt;
                }
            }
            else
            {
                exit_power_cnt = 0;
            }
            //10s
            if(exit_power_cnt > 100)
            {
                exit_power_cnt = 100;
                //取消充电
                exit_GoCharge();
            }
        }

        //exit charge
        if(base_status.robot.charge == 7)
        {
           //回退完成后关闭回充
            if(base_status.robot.vx == 0)
            {
                time_cnt++;
            }
            else
            {
                time_cnt = 0;
            }

            if(time_cnt > 10)
            {
                time_cnt = 0;
                
                if(base_status.robot.status == Mod_Free)
                {
                    switch(base_status.robot.last_status)
                    {
                        case Mod_Order_Interest:
                        {
                			     if(json_interest_point["point"].size() == 0)
                			     {
                			     	//ROS_ERROR("error have no interest point");
                                                PLOG_ERROR<<"error have no interest point";
                			     	base_status.robot.main_error = Mod_Order_Interest;
                			      	base_status.robot.sub_error  = 2;
                			     	base_status.robot.status = Mod_Error;
                			     	return;
                			      }

                            geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Order_Interest_index]["z"].asDouble());
                            set_goal("map",json_interest_point["point"][Order_Interest_index]["x"].asDouble(),json_interest_point["point"][Order_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                            base_status.robot.status = Mod_Order_Interest;
                            break;
                        }
                        case Mod_Random_Interest:
                        {
                            if(json_interest_point["point"].size() == 0)
                  			    {
                  			     	//ROS_ERROR("error have no interest point");
                                    PLOG_ERROR<<"error have no interest point";
                  			     	base_status.robot.main_error = Mod_Order_Interest;
                  			      	base_status.robot.sub_error  = 2;
                  			     	base_status.robot.status = Mod_Error;
                  			     	return;
                  			    }
                            Random_Interest_index = rand() % json_interest_point["point"].size();
                            geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_interest_point["point"][Random_Interest_index]["z"].asDouble());
                            set_goal("map",json_interest_point["point"][Random_Interest_index]["x"].asDouble(),json_interest_point["point"][Random_Interest_index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                            base_status.robot.status = Mod_Random_Interest; 
                            break;
                        }
                        case Mod_Trajectory:
                        {
                            break;
                        }
                        case Mod_More_Task:
                        {
                            if(json_more_task["task"].size() == 0)
                  			    {
                  			        //ROS_ERROR("error have no task point");
                                    PLOG_ERROR<<"error have no task point";
                  			        base_status.robot.main_error = Mod_More_Task;
                  			        base_status.robot.sub_error  = 7;
                  			        base_status.robot.status = Mod_Error;
                  			        return;
                  			    }

                            //nav point
                            if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "nav")
                            {
                               
                                start_more_task_interest_point(json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt());
                                base_status.robot.status = Mod_More_Task; 
                            }
                            //track point
                            else if(json_more_task["task"][more_task.main_task][more_task.sub_task]["type"] == "track")
                            {
                                int id = json_more_task["task"][more_task.main_task][more_task.sub_task]["id"].asInt();
                                int dir = 0;
                                if(json_more_task["task"][more_task.main_task][more_task.sub_task]["dir"] == "forward")
                                {
                                    dir = 2;
                                }
                                else  //back
                                {
                                    dir = 3;
                                }
                               
                                start_more_task_track_first(id,dir);
                                base_status.robot.status = Mod_More_Task; 
                            }
                            else
                            {
                                //error
                            }
                            break;
                        }
                        default:
                        {
                            base_status.robot.last_status = Mod_Free;
                            break;
                        }
                    }
              }
          }
        }
        //charge error
        else if(base_status.robot.charge == 2 || base_status.robot.charge == 5 || base_status.robot.charge == 6)
        {
            //回退完成后关闭回充
            if(base_status.robot.vx == 0)
            {
                time_cnt++;
            }
            else
            {
                time_cnt = 0;
            }
            if(time_cnt > 20)
            {
                time_cnt = 0;
                //回冲模式下对接错误，再次走到回冲点回冲
                if(base_status.robot.status == Mod_Charge )
                {
                   if(charge_error < setting.go_charge_error_time && Go_Charge_Flag == 0)
                   {
                       charge_error++;

                       if(json_charge_point["point"].size() == 0)
            		       {
                			   //ROS_ERROR("error have no charge point");
                               PLOG_ERROR<<"error have no charge point";
                			   base_status.robot.main_error = Mod_Charge;
                			   base_status.robot.sub_error  = 2;
                			   base_status.robot.status = Mod_Error;
                			   return;
            		       }

                       int index = 0;
                       Go_Charge_Flag = 1;
                       geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(json_charge_point["point"][index]["z"].asDouble());
                       set_goal("map",json_charge_point["point"][index]["x"].asDouble(),json_charge_point["point"][index]["y"].asDouble(),goal_quat.z,goal_quat.w);
                       
                       //ROS_WARN("going charge error1 will again!");
                       PLOG_WARNING<<"going charge error1 will again!";
                       cout <<"回充对接错误将再次回充！"<<endl;
                       cout << "--------------------------------------------------------------------" << endl;
                   }
                   else
                   {
                       //ROS_WARN("going charge error1!:%d",charge_error);
                       PLOG_WARNING<<"going charge error1!:"<<charge_error;
                       cout <<"回充对接错误"<<charge_error<<endl;
                       cout << "--------------------------------------------------------------------" << endl;
                       base_status.robot.main_error = Mod_Charge;
                       base_status.robot.sub_error  = 3;
                       base_status.robot.status = Mod_Error;
                   }

                }
                
            }
        }

        if(base_status.robot.status != Mod_Charge )
        {
            charge_error = 0;
        }
      
    }
}
/********************************************************************************************************/
void json_cmd_deal(int cmd,Json::Value value)
{
    switch(cmd)
    {
        //move
        case 0:
        {
            deal_move_cmd(value);
            break;
        }
        case 1:
        {
            deal_interest_point_control_cmd(value);
            break;
        }
        case 2:
        {
            deal_interest_point_add_cmd(value);
            break;
        }
        case 3:
        {
            deal_charge_point_control_cmd(value);
            break;
        }
        case 4:
        {
            deal_charge_point_add_cmd(value);
            break;
        }
        case 5:
        {
            deal_trajectory_point_control_cmd(value);
            break;
        }
        case 6:
        {
            deal_trajectory_point_creat_cmd(value);
            break;
        }
        case 7:
        {
            deal_slam_map_control_cmd(value);
            break;
        }
        case 8:
        {
            deal_reset_cmd(value);
            break;
        }
        case 9:
        {
            deal_invent_wall_control_cmd(value);
            break;
        }
        case 10:
        {
            deal_invent_wall_creat_cmd(value);
            break;
        }
        case 11:
        {
            deal_goal_point_set(value);
            break;
        }
        case 12:
        {
            deal_init_pose_set(value);
            break;
        }
        case 13:
        {
            deal_setting(value);
            break;
        }
        case 14:
        {
            deal_more_task_control_cmd(value);
            break;
        }
        case 15:
        {
            deal_more_task_save_cmd(value);
            break;
        }
        case 16:
        {
                deal_updata_cmd(value);
            break;
        }
        case 17:
        {
            deal_error_reset(value);
            break;
        }
        case 18:
        {
            deal_location_point_add_cmd(value);
            break;
        }
        case 19:
        {
            deal_location_point_control_cmd(value);
            break;
        }
        case 20:
        {
                deal_save_start_point(value);
            break;
        }
         case 21:
        {
            deal_path_point_creat_cmd(value);
            break;
        }
        case 22:
        {
              deal_path_point_control_cmd(value);
            break;
        }
        case 23:
        {
            deal_get_slam_map(value);
            break;
        }
        case 24:
        {
            deal_software_stop(value);
            break;
        }
        case 25:
        {
            deal_magnetic_nav(value);
            break;
        }
        case 26:
        {
            deal_qr_cmd(value);  //二维码
            break;
        }
        case 27:
        {
            deal_qr_add_cmd(value);
            break;
        }
        default:break;
    }
}
void json_analysis(std::string str)
{
    Json::Reader reader;
    Json::Value value;

    //std::cout << str << std::endl;

    if(!reader.parse(str, value))
    {
        //ROS_ERROR("json_analysis error");
        PLOG_ERROR<<"json_analysis error";
        return;
    }
    PLOG_INFO << value["cmd_type"].asString() <<":"<<value["cmd"].asString();
    std::cout << value["cmd_type"] << std::endl;

    //移动指令 vx vy vz
    if(value["cmd_type"] == "move")
    {
       json_cmd_deal(0,value);
    }
    //-----------------------导航模式----------------------------------------
    //兴趣点控制指令 delete start random stop (time) get
    else if(value["cmd_type"] == "interest_point_control")
    {
       json_cmd_deal(1,value);
    }
    //兴趣点增加指令 x y yaw
    else if(value["cmd_type"] == "interest_point_add")
    {
       json_cmd_deal(2,value);
    }

    //回充点控制指令 get goto
    else if(value["cmd_type"] == "charge_point_control")
    {
       json_cmd_deal(3,value);
    }

    //回充点增加指令 x y yaw
    else if(value["cmd_type"] == "charge_point_add")
    {
       json_cmd_deal(4,value);
    }

    //轨迹控制 delete (id) delete_all get (id) start(id)
    else if(value["cmd_type"] == "trajectory_point_control")
    {
       json_cmd_deal(5,value);
    }
    //轨迹创建 id x y yaw
    else if(value["cmd_type"] == "trajectory_point_creat")
    {
       json_cmd_deal(6,value);
    }

    //建图控制 start stop save
    else if(value["cmd_type"] == "slam_map_control")
    {
       json_cmd_deal(7,value);
    }
    //------------------------------------------------------------
    //重定位
    else if(value["cmd_type"] == "reset")
    {
       json_cmd_deal(8,value);
    }

    //虚拟墙控制 delete (id) delete_all get
    else if(value["cmd_type"] == "invent_wall_control")
    {
       json_cmd_deal(9,value);
    }
    //虚拟墙创建 id start_x start_y end_x end_y
    else if(value["cmd_type"] == "invent_wall_add")
    {
       json_cmd_deal(10,value);
    }
  
    //目标点设置 x y z
    else if(value["cmd_type"] == "goal_point_set")
    {
       json_cmd_deal(11,value);
    }
    //初始位置设置 x y z
    else if(value["cmd_type"] == "init_pose_set")
    {
       json_cmd_deal(12,value);
    }

    //初始设置
    else if(value["cmd_type"] == "setting")
    {
       json_cmd_deal(13,value);
    }
    //多任务控制
    else if(value["cmd_type"] == "more_task_control")
    {
       json_cmd_deal(14,value);
    }
    //多任务保存
    else if(value["cmd_type"] == "more_task_save")
    {
       json_cmd_deal(15,value);
    }
    //系统更新重启
    else if(value["cmd_type"] == "updata")
    {
       json_cmd_deal(16,value);
    }
    //错误清除
    else if(value["cmd_type"] == "error_reset")
    {
        json_cmd_deal(17,value);
    }
     //定位点添加
    else if(value["cmd_type"] == "location_point_add")
    {
        json_cmd_deal(18,value);
    }
     //定位点控制
    else if(value["cmd_type"] == "location_point_control")
    {
        json_cmd_deal(19,value);
    }
    //起始点保存
    else if(value["cmd_type"] == "save_start_point")
    {
        json_cmd_deal(20,value);
    }
    //轨道点保存
    else if(value["cmd_type"] == "path_point_creat")
    {
        json_cmd_deal(21,value);
    }
    //轨道点控制
    else if(value["cmd_type"] == "path_point_control")
    {
        json_cmd_deal(22,value);
    }
    //地图分包读取
    else if(value["cmd_type"] == "get_slam_map")
    {
        json_cmd_deal(23,value);
    }
    //软件急停发布
    else if(value["cmd_type"] == "software_stop")
    {
         json_cmd_deal(24,value);
    }
    else if(value["cmd_type"] == "magnetic_nav")
    {
         json_cmd_deal(25,value);
    }
    else if(value["cmd_type"] == "qr_location_point_control")
    {
         json_cmd_deal(26,value);   //二维码
    }
    else if(value["cmd_type"] == "qr_location_point_add")
    {
         json_cmd_deal(27,value);   //二维码
    }
    //std::cout << value << std::endl;
}

//--------------------------------------------------------------------------------------------------------------------------

//===============================================================
// 语法格式：    void ros_init(void)
// 实现功能：    主函数
// 入口参数：    无
// 出口参数：    无
//===============================================================
int ros_init(int argc, char *argv[])
{
    ros::init(argc, argv, "ros_robot_control_node");
    ros::NodeHandle n;
    ros::NodeHandle private_nh("~");

    //获得主机名字
    struct passwd* pwd;
	uid_t userid;
    std:string hostname;
	userid = getuid();
	pwd = getpwuid(userid);
    hostname = pwd->pw_name;
    hostname = "robotcar";
    std::cout << "hostname:" <<hostname  << std::endl;

    private_nh.param<std::string>("interest_point_filename",interest_point_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/interest_point.json"));

    private_nh.param<std::string>("invent_wall_filename",invent_wall_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/invent_wall.json"));

    private_nh.param<std::string>("charge_point_filename",charge_point_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/charge_point.json"));
                                
    private_nh.param<std::string>("location_point_filename",location_point_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/location_point.json"));

    private_nh.param<std::string>("qr_location_point_filename",qr_location_point_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/qr_location_point.json"));

    private_nh.param<std::string>("trajectory_point_filename",trajectory_point_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/trajectory_point.json"));
    
    private_nh.param<std::string>("path_point_filename",path_point_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/path_point.json"));

    private_nh.param<std::string>("setting_filename",setting_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/setting.json"));

    private_nh.param<std::string>("more_task_filename",more_task_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/more_task.json"));

    private_nh.param<std::string>("slam_launch_filename",slam_launch_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/launch/carto_slam.launch"));

    private_nh.param<std::string>("slam_add_launch_filename",slam_add_launch_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/launch/carto_slam_add.launch"));
 
    private_nh.param<std::string>("save_map_sh_filename",save_map_sh_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/sh/savingmap.sh"));

    private_nh.param<std::string>("updata_sh_filename",updata_sh_filename,
                                   std::string("/home/" + hostname + "/mysh/updata.sh"));

    private_nh.param<std::string>("move_base_filename",move_base_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/launch/move_base.launch"));

    private_nh.param<std::string>("golab_path_topic_name",golab_path_topic_name,
                                   std::string("/move_base/GlobalPlanner/plan"));

    private_nh.param<std::string>("local_path_topic_name",local_path_topic_name,
                                   std::string("/move_base/TebLocalPlannerROS/local_plan"));

    private_nh.param<std::string>("save_pose_filename",save_pose_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/save_pose.json"));

    private_nh.param<std::string>("save_pose_filename1",save_pose_filename1,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_robot_control_json/json/save_pose1.json"));

    private_nh.param<std::string>("map_pgm_filename",map_pgm_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/map/map.pgm"));

    private_nh.param<std::string>("backup_map_sh_filename",backup_map_sh_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/sh/backup_map.sh"));

    private_nh.param<std::string>("restore_map_sh_filename",restore_map_sh_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/sh/restore_map.sh"));
                                   
    // private_nh.param<std::string>("refining_map_py_filename",refining_map_py_filename,
    //                                std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/scripts/map_ithin.py"));
                                   
    private_nh.param<std::string>("refining_map_py_filename",refining_map_py_filename,
                                   std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/zkwl_robot_start/sh/thin_map.sh"));

    //listener
    scan_listener   = new  (tf::TransformListener);
    local_listener   = new  (tf::TransformListener);
    robot_listener = new  (tf::TransformListener);

    //timer
    movebase_timer = n.createTimer(ros::Duration(1.0), timer_handler,false);
    reset_pose_timer= n.createTimer(ros::Duration(60.0), reset_pose_timer_hanlde,false);
    track_stop_timer = n.createTimer(ros::Duration(60.0), track_stop_timer_hanlde,false);
    movebase_timer.stop();
    reset_pose_timer.stop();
    track_stop_timer.stop();

    //movebase result
    movebase_result_sub = n.subscribe("move_base/result",10,movebase_resultCallback);
    //move_base status
    movebase_status_sub =  n.subscribe("move_base/status",10,movebase_statusCallback);
    //全剧路径订阅
    gloab_path_sub      = n.subscribe(golab_path_topic_name,10,gloab_path_Callback);
    //局部路径订阅
    //loacl_path_sub      = n.subscribe(local_path_topic_name,10,local_path_Callback);
    //电压订阅和回充状态订阅
    zkwl_robot_sub      = n.subscribe("/agv_info", 1,cmd_agvCallback);
    //电压订阅和回充状态订阅
    zkwl_robot1_sub     = n.subscribe("/four_info", 1,cmd_fourCallback);
    //电压订阅和回充状态订阅
    zkwl_robot2_sub     = n.subscribe("/PT_Robot_info", 1,cmd_ptCallback);
    //电压订阅和回充状态订阅
    zkwl_robot3_sub     = n.subscribe("/DT_Robot_agv1", 1,cmd_dtCallback);
    //电压订阅和回充状态订阅
    zkwl_robot4_sub     = n.subscribe("/agv_info_dt", 1,cmd_agv_dtCallback);
    //电压订阅和回充状态订阅
    zkwl_robot5_sub     = n.subscribe("/dt/state_info", 1,cmd_agv_dt_controlCallback);
    //电压订阅和回充状态订阅
    zkwl_robot6_sub     = n.subscribe("/four_wheel_info", 1,cmd_four_ptCallback);
    //电压订阅和回充状态订阅
    zkwl_robot5_charge_sub = n.subscribe("/dt/charge_info", 1,cmd_agv_dt_control_chargeCallback);
    //激光数据订阅  //多线雷达为SCAN1
    scan_sub            = n.subscribe("/scan", 1,scan_agvCallback);
    //地图订阅
    map_sub             = n.subscribe("/map", 10,map_agvCallback);
    //icp状态订阅
    icp_status_sub     = n.subscribe("/icp_status", 1,icp_status_agvCallback);
    //rosout订阅
    rosout_sub          = n.subscribe("/rosout", 100,rosout_Callback);
    //IMU订阅
    imu_sub             = n.subscribe("/sensor_imu", 1,cmd_imuCallback);
    //特征板icp得分订阅
    icp_score_sub  = n.subscribe("/icp_fitness_score", 1,icp_fitness_scoreCallback);
    ///constraint_list订阅
    constraint_sub     = n.subscribe("/constraint_list", 1,constraint_listCallback);
    //循迹停车订阅
    trajectory_stop_sub	= n.subscribe("/move_base/TebLocalPlannerROS/trajectory_stop_pub", 1,trajectory_stopCallback);
    //电池BMS订阅
    bms_info_sub        = n.subscribe("/kuma_bms_info", 1,bms_infoCallback);
    //轨道路径点订阅
    half_traffic_path_sub    = n.subscribe("/ros_half_planner/traffic_path", 10,half_traffic_pathCallback);
    //轨道反馈订阅
    half_feedback_sub    = n.subscribe("/ros_half_planner/feedback", 10,half_feedbackCallback);
    //全局路径获得轨迹成功订阅
    global_feedback_sub = n.subscribe("/move_base/GlobalPlanner/feedback", 10,global_feedbackCallback);
    //gpio
    gpio_sub = n.subscribe("/gpio_msg", 10,gpio_msgCallback);
    //limitation_stateCallback
    limitation_state_sub = n.subscribe("/motor/limit_status", 10,limitation_stateCallback);


    //导航目标点发送话题
    goal_pub            = n.advertise<geometry_msgs::PoseStamped>("move_base_simple/goal",100);
    goal1_pub           = n.advertise<geometry_msgs::PoseStamped>("move_base_simple/goal1",100);
    //导航取消
    movebase_cancel_pub = n.advertise<actionlib_msgs::GoalID>("move_base/cancel", 1);
    //回充控制
    go_charge_pub       = n.advertise<std_msgs::UInt8>("go_charge", 100);
    // agv_go_charge_pub =  n.advertise<std_msgs::UInt8>("agv_go_charge", 100);
    agv_go_charge_pub =  n.advertise<std_msgs::UInt8>("/dt/charge_ctrl", 100);
    four_go_charge_pub =  n.advertise<std_msgs::UInt8>("four_wheel_go_charge", 100);
    //速度控制发布
    cmd_vel_pub         = n.advertise<geometry_msgs::Twist>("cmd_vel", 10);
    //初始位置发布
    initialpose1_pub    = n.advertise<geometry_msgs::PoseWithCovarianceStamped>("initialpose1", 10);
    //轨迹点发布
    //trajectory_point_pub= n.advertise<geometry_msgs::PoseArray>("trajectory_points", 10);
    trajectory_point_pub= n.advertise<geometry_msgs::PoseArray>("/move_base/GlobalPlanner/trajectory_points", 10);
    //重定位发布
    reset_pose_pub      = n.advertise<std_msgs::UInt8>("/reset_pose", 10);
    //imu
    imu_pub      	= n.advertise<sensor_msgs::Imu>("/robot_imu", 1);
    //pt error clear
    pt_error_clear_pub =  n.advertise<std_msgs::UInt8>("/collision_release",10);
    four_error_clear_pub =  n.advertise<std_msgs::UInt8>("/four_wheel_fault_clean",10);
    agv_fault_clean_pub =  n.advertise<std_msgs::UInt8>("/agv_fault_clean",10);
    dt_collision_clean_pub = n.advertise<std_msgs::Bool>("/dt/collision_clean",10);

    //software  stop   pub
    software_stop_pub =  n.advertise<std_msgs::UInt8>("/go_stop",10);
    pt_software_stop_pub =  n.advertise<std_msgs::UInt8>("/four_wheel_stop_ctrl",10);

    //vitual wall 虚拟墙
    create_wall_client = private_nh.serviceClient<move_base_virtual_wall_server::CreateWall>("/virtual_wall_server/create_wall");
    delete_all_client  = private_nh.serviceClient<move_base_virtual_wall_server::DeleteAll>("/virtual_wall_server/delete_all");
    delete_wall_client = private_nh.serviceClient<move_base_virtual_wall_server::DeleteWall>("/virtual_wall_server/delete_wall");
    clear_costmaps_client = private_nh.serviceClient<std_srvs::Empty>("/move_base/clear_costmaps");
    tracker_save_client = private_nh.serviceClient<apriltag_tracker::save_tag>("/apriltag_tracker/tag_save");  //二维码

    printf("start to ros_robot_control_node\n");

   // ros::Duration(1.0).sleep();
   // get_run_nodel();
    //test
    //ConnectWif("ZKWL-2-5G","11223344");
    //UDP初始化
    UDPSendInit();
    //加载配置
    load_setting_json();
    //开机启动movebase
    start_nav();
    //加载保存位置
    //load_save_pose_json();
    //如果为顶点启动，则自动重定位
    if(setting.start_point_check == true)
    {
        pub_reset_pose(true);
         //开启超时定时
        reset_pose_timer.start();  
        base_status.robot.doing_reset = true;
        PLOG_INFO<<"start reset pose";
    }
}

/***********************************************MQTT*******************************************************************/
static int s_qos = 0;
static struct mg_connection *s_conn;

void mqtt_to_json_pub(struct mg_connection *c)
{
    if (!send_que.empty()) //此处需要判断此时队列是否为空
    {
        switch(send_que.front())
        {
            case Json_Status:  //base_status
            {
                base_status_send_10hz();
                json_to_mqtt_pub(c, (char*)"base_status",json_base_status,s_qos);
                send_que.pop();
                break;
            }
            case Json_Scan:  //scan
            {
                if(setting.radra_check)
                {
                    json_to_mqtt_pub(c, (char*)"scan",json_scan,s_qos);
                }
                else
                {
                    //多次清空会段错误
                    //json_scan.clear();
                    json_scan["cmd_type"] = Json::Value("scan");
                    json_scan["hed"] =  Json::Value("");
                    json_scan["ranges"] =  Json::Value("");
                    json_scan["pose"] =  Json::Value("");

                }
                send_que.pop();
                break;
            }
            case Json_Interest_point: //interest_point
            {
                json_to_mqtt_pub(c, (char*)"interest_point",json_interest_point,s_qos);
                send_que.pop();
                break;
            }
            case Json_Charge_point: //charge_point
            {
                json_to_mqtt_pub(c, (char*)"charge_point",json_charge_point,s_qos);
                send_que.pop();
                break;
            }
            case Json_Location_point: //location_point
            {
                json_to_mqtt_pub(c, (char*)"location_point",json_location_point,s_qos);
                send_que.pop();
                break;
            }
            case Json_Gloab_path:  //gloab_path
            {   
                if(setting.path_check)
                {
                    if(json_gloab_path.empty() == true)
                    {
                        //多次清空会段错误
                        //json_gloab_path.clear();
                        json_gloab_path["point"] = Json::Value("");
                        json_gloab_path["cmd_type"] = Json::Value("gloab_path");
                    }
                    json_to_mqtt_pub(c, (char*)"gloab_path",json_gloab_path,s_qos);
                }
                else
                {
                    //多次清空会段错误
                    //json_gloab_path.clear();
                    json_gloab_path["point"] = Json::Value("");
                    json_gloab_path["cmd_type"] = Json::Value("gloab_path");
                    json_to_mqtt_pub(c, (char*)"gloab_path",json_gloab_path,s_qos);
                } 
                send_que.pop();
                break;
            }
            case Json_Local_path:  //local_path
            {
                if(setting.path_check)
                {
                    json_to_mqtt_pub(c, (char*)"local_path",json_local_path,s_qos);
                }
                else
                {
                    //多次清空会段错误
                    //json_local_path.clear();
                    json_local_path["point"] = Json::Value("");
                    json_local_path["cmd_type"] = Json::Value("local_path");
                    json_to_mqtt_pub(c, (char*)"local_path",json_local_path,s_qos);
                } 
                
                break;send_que.pop();
            }
            case Json_Qr:   //二维码
            {
                json_to_mqtt_pub(c, (char*)"qr_location_point",json_qr_pose,s_qos);
                send_que.pop();
                break;
            }
            case Json_Feedback:   //feedback
            {
                json_to_mqtt_pub(c, (char*)"feedback",json_feedback,s_qos);
                send_que.pop();
                break;
            }
            case Json_Map:   //map
            {
                json_to_mqtt_pub(c, (char*)"map",json_map,s_qos);
                send_que.pop();
                break;
            }
            case Json_Task_feedback:  //task_feedback
            {
                json_to_mqtt_pub(c, (char*)"task_feedback",json_task_feedback,s_qos);
                send_que.pop();
                break;
            }
            case Json_Invent_wall_point: //invent_wall
            {
                json_to_mqtt_pub(c, (char*)"invent_wall",json_invent_wall,s_qos);
                send_que.pop();
                break;
            }
             case Json_More_task: //more_task
            {
                json_to_mqtt_pub(c, (char*)"more_task",json_more_task,s_qos);
                send_que.pop();
                break;
            }
             case Json_Track_point: //trajectory_point
            {
                json_to_mqtt_pub(c, (char*)"trajectory_point",json_track_point,s_qos);
                send_que.pop();
                break;
            }
            case Json_Path_point: //path_point
            {
                json_to_mqtt_pub(c, (char*)"path_point",json_path_point,s_qos);
                send_que.pop();
                break;
            }
            default:
            {
                send_que.pop();
                break;
            }
        }   
    }
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) 
{
  if (ev == MG_EV_OPEN) 
  {
    MG_INFO(("CREATED"));
  } 
  else if (ev == MG_EV_CONNECT) 
  {
    MG_INFO(("MQTT  CONNECTED "));
    PLOG_INFO << "MQTT  CONNECTED ";
  } 
  else if (ev == MG_EV_MQTT_OPEN) 
  {
    // MQTT connect is successful
    MG_INFO(("MQTT  OPEN  AND SUB"));
    PLOG_INFO << "MQTT  OPEN  AND SUB ";
    //sub
    mg_mqtt_sub(s_conn, mg_str("robot_control"), 0);
    // Set a label that we're logged in
    c->label[0] = 'X';  
  } 
  else if (ev == MG_EV_MQTT_MSG)
  {
    // When we get echo response, print it
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    //MG_INFO(("RECEIVED %.*s <- %.*s", (int) mm->data.len, mm->data.ptr,(int) mm->topic.len, mm->topic.ptr));

    std::string rx_data;
    //加入线程锁
    pthread_mutex_lock(&mutex1);
    json_analysis(rx_data.assign(mm->data.ptr,mm->data.len));
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
            TRY //防止崩溃程序
            mqtt_to_json_pub(c);
            END_TRY
            pthread_mutex_unlock(&mutex1);  
            prev_second = now_second;
        }
  }

  if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE)
   {
        MG_INFO(("Got event %d, stopping...", ev));
        PLOG_INFO << "MQTT CLOSE";
        *(bool *) fn_data = true;  // Signal that we're done
  }
}

void mqtt_pub_deal(struct mg_connection *c)
{
    static unsigned int time_cnt = 0;
    time_cnt++;

    //100 ms
    if(time_cnt % 1 == 0)
    {
        //电量监测
        power_monitoring();
    }
    //300 ms
    if(time_cnt % 3 == 0)
    {
        send_que.push(Json_Status);
    }
    //200   500 ms
    if(time_cnt % 2 == 0)
    {
        
        if(base_status.robot.status != Mod_Free)
        {
            send_que.push(Json_Gloab_path);
        }

        if(base_status.sensor.laser)
        {
            send_que.push(Json_Scan);
        }
        
    }
    //1000 ms
    if(time_cnt % 10 == 0)
    {
        if(send_que.size())
        {
            printf("send_que size:%ld  front:%d  cpu:%d\n",send_que.size(),send_que.front(),get_cpu_use());
        }
        //停车位置更新
        local_updata_control_1hz();
        UDPSend();    
    }
    //2000 ms
    if(time_cnt % 20 == 0)
    {
        //pub
        //send_que.push(Json_Map);        
    }
    
    if(time_cnt > 20)
    {
        time_cnt = 0;
    }
}

//MQTT Poll Deal
void *run_mqtt_poll(void *arg)
{
    struct mg_mgr mgr;
    struct mg_mqtt_opts opts;
    bool done = false;
    mg_mgr_init(&mgr);

    opts.user = mg_str("");
    opts.pass = mg_str("");
    opts.clean = true;
    opts.will_topic = mg_str("s_pub_topic");
    opts.will_message = mg_str("robot_topic");
    opts.will_qos = s_qos;
    opts.client_id = mg_str("0");
    opts.version = 4;
    opts.keepalive = 50;//10;
    opts.will_retain = false;
                              
    s_conn = NULL;
    s_conn = mg_mqtt_connect(&mgr, "mqtt://0.0.0.0:1883", &opts, fn, &done);
    
    while (ros::ok() && !done)
    {
        //10s超时
       mg_mgr_poll(&mgr,20); 
    }
    mg_mgr_free(&mgr); 
    MG_INFO(("MQTT Main is close"));
}

//deal call back
void *run_deal_callback(void *arg)
{
    deal_back.up_map_flag = false;
    deal_back.up_scan_flag = false;
    ros::Rate loop_rate(50); 
    while(ros::ok())
    {
        if(deal_back.up_map_flag == true)
        {
            deal_map_data();
            deal_back.up_map_flag = false;
        }

        if(deal_back.up_scan_flag == true)
        {
            deal_scan_data();
            deal_back.up_scan_flag = false;
        }
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
    }
  
}

int main(int argc, char *argv[]) 
{
    //log init
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, "../log/log.log",1024*1024*20,5).addAppender(&consoleAppender); // Step2: initialize the logger //初始化logger
    //ros init
    ros_init(argc, argv);
    //创建MQTT处理线程
    int ret = pthread_create(&thread_mqtt_poll, NULL, run_mqtt_poll, NULL);  //创建线程
    pthread_detach(thread_mqtt_poll); // 线程分离，结束时自动回收资源
    //创建calback处理线程
    int ret1 = pthread_create(&thread_deal_callback, NULL, run_deal_callback, NULL);  //创建线程
    pthread_detach(thread_deal_callback); // 线程分离，结束时自动回收资源
    /**************客户端需要在此初始化，在ros_init中初始化会有闪退的问题，原因还未找到*************************/

    // 定义一个客户端
    Client1 client1("ros_end_control", true);
    client_scan_icp_ptr = &client1;

    Client2 client2("/ros_magnetic_nav_service", true);
    client_magnetic_nav_ptr = &client2;

    Client3 client3("/apriltag_tracker/tag_track", true);
    client_tracker_start_ptr = &client3;

    /***************************************************************************************************************/
    
    //多线程回调  在此处才生效
    ros::AsyncSpinner spinner(8);
    spinner.start();
    
    //设置发送数据的频率为10Hz
    ros::Rate loop_rate(10);    
    while(ros::ok())
    {     
        mqtt_pub_deal(s_conn);   
        //ros::spinOnce();
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
    }
    //ros::shutdown();
    ros::waitForShutdown();
    close(lfd);  //关闭UDP 
}
