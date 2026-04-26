#include <ros_end_control.h>

namespace ros_end_control
{
  Ros_end_control::Ros_end_control()
  {
    ros::NodeHandle n;
    ros::NodeHandle private_nh("~");

    private_nh.param<double>("Kp_rho",              Kp_rho,        0.5);  //速度比例
    private_nh.param<double>("Kp_alpha",          Kp_alpha,   1);  //位置角度差比例 2.0
    private_nh.param<double>("Kp_beta",             Kp_beta,     1); //角度差比例 1.0
    private_nh.param<double>("Max_v",                  Max_v,          0.2);  //位置角度差比例
    private_nh.param<double>("Max_w",                 Max_w,         0.2); //角度差比例
    private_nh.param<double>("Min_socre",          Min_socre,         0.000035); //最小匹配得分
    private_nh.param<double>("Before_Target",  Before_Target, 0.4); //目标前点距离(可离目标的最近距离)
    private_nh.param<double>("Forward_obstacle_dis",  Forward_obstacle_dis, 0.05); //前方避障距离
    private_nh.param<double>("Side_obstacle_dis",  Side_obstacle_dis, 0.05); //侧方避障距离
    private_nh.param<std::string>("footprint",  Footprint, "[[0.33,-0.23],[0.33,0.23],[-0.35,0.23],[-0.35,-0.23]]"); //footprint
    private_nh.param<std::string>("scan_topic",  Scan_topic, "scan"); //scan_topic
    
    icp_fitness_score_sub = n.subscribe("/icp_fitness_score",10,&Ros_end_control:: icp_fitness_scoreCallback,this);
    costmap_sub = n.subscribe("/move_base/local_costmap/costmap", 10, &Ros_end_control::costmap_Callback,this);
    costmap_updates_sub = n.subscribe("/move_base/local_costmap/costmap_updates", 10, &Ros_end_control::costmap_updata_Callback,this);
    //需要用costmap的pose才能准确避障，找不到具体原因，可能是cosmap更新问题
    costmap_robot_pose_sub = n.subscribe("/move_base/local_costmap/robot_pose",10,&Ros_end_control:: costmap_robot_poseCallback,this);
    //scan_sub
    scan_sub = n.subscribe(Scan_topic,10,&Ros_end_control:: scan_cb,this);

    goal_pub  = n.advertise<geometry_msgs::PoseStamped>("move_base_simple/goal",100);
    //速度控制发布
    cmd_vel_pub         = n.advertise<geometry_msgs::Twist>("cmd_vel", 10);
    path_pub                 = n.advertise<nav_msgs::Path>("end_path", 10); // 创建发布者

    //servier
    pattern_name_client = n.serviceClient<scan_icp_matcher::server1>("/pattern_file_load");
    ros::service::waitForService("/pattern_file_load");

    robot_listener = new  (tf::TransformListener);

    

    goal_pose.pose.orientation.w = 1.0;
    run_con_clear();

      //设置footprint的距离
     if(costmap_2d::makeFootprintFromString(Footprint,footprint))
     {
      printf("ros_end_control get footprint %s\n",Footprint.data());
      printf("Before_Target:%f\n",Before_Target);
      printf("Forward_obstacle_dis:%f\n",Forward_obstacle_dis);
      printf("Side_obstacle_dis:%f\n",Side_obstacle_dis);

      footprint[0].x += Forward_obstacle_dis;
      footprint[1].x += Forward_obstacle_dis;

      footprint[0].y -= Side_obstacle_dis;
      footprint[1].y += Side_obstacle_dis;
      footprint[2].y += Side_obstacle_dis;
      footprint[3].y -= Side_obstacle_dis;
     }
     else
     {
        printf("not get footprint %s\n",Footprint.data());
     }
     //禁止printf输出
    fclose(stdout);

    printf("ros_end_control is init\n");

    if (!cost_translation_)
    {
      cost_translation_ = new uint8_t[101];
      for (int i = 0; i < 101; ++i) 
      {
        cost_translation_[i] = static_cast<uint8_t>(i * 254 / 100);
      }
    }

    //构建一个action服务，第二个参数是服务的名称，客户端需要根据这个唯一的名称进行连接
    //最后一个参数表示是否构建完成之后就开始运行，一般应该设置为false，并在构建完成之后使用start()方法开始
    // create a server
    as_ = new Server(n, "ros_end_control", boost::bind(&Ros_end_control::actioncb, this, _1),false);
    as_->start();

    ros::Rate loop_rate(20);    //设置发送数据的频率为10Hz
    while(ros::ok())
    { 
        this->run_control();
        ros::spinOnce();
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
    }
  }

  Ros_end_control::~Ros_end_control()
  {

  }

  void Ros_end_control::actioncb(const scan_icp_matcher::action1GoalConstPtr &goal)
  {
    printf("run the action\n");
    ac_result.id = goal->id;
    ac_feed.id =  goal->id;
    if(goal->task_mode == 0)
    {
      if(run_con.run_step != Start )
      {
        printf("the step is not Start\n");
        ac_result.result = ac_Not_Start;
      }
      else
      {
        //8081回充id
        if(ac_result.id == 8081)
        {
          se_pattern_name.request.name.data  = "pattern_charge";
        }
        else
        {
          se_pattern_name.request.name.data  = "pattern_" + std::to_string(goal->id);
        }
        
        // 7.处理响应
        if (pattern_name_client.call(se_pattern_name))
        {
            //加载成功启动对准
            if(se_pattern_name.response.result)
            {
              //解决刚加载导致的tf为之前发布的问题导致tf too nearl报错
              //sleep(1);
              run_con.run_step = Start;
              run_con.will_run = true ;
              run_con.get_goal = true;
              run_con.goal_pose.x =  -0.001 - run_con.laser_x;
              run_con.goal_pose.y = 0.0;
              run_con.goal_pose.yaw = 0.0;
              ac_result.result = 0;
            }
            else
            {
               ROS_ERROR("%s can not load",se_pattern_name.request.name.data.c_str());
               ac_result.result = ac_Pcd_Fail;
            }
        }
        else
        {
            ROS_ERROR("pattern_name_client call fail");
            ac_result.result = ac_Server_Fail;
        }
      }
    }
    else if(goal->task_mode == 1)
    {
      //if(run_con.run_step != Align_Ment_Complete)
      //不再对准模式下后退，则取消action
      if(run_con.run_step != Align_Ment_Complete /*&& run_con.run_step != Start*/)
      {
        printf("the step is not Align_Ment_Complete or \n");
        run_con_clear();
        cmd_vel.linear.x = 0.0;
        cmd_vel.angular.z = 0.0;
        cmd_vel_pub.publish(cmd_vel);
        ac_result.result = ac_Cancel;
        as_->setPreempted(ac_result);
        return;
      }
      else
      {
        //8081回充id
        if(ac_result.id == 8081)
        {
          se_pattern_name.request.name.data  = "pattern_charge";
        }
        else
        {
          se_pattern_name.request.name.data  = "pattern_" + std::to_string(goal->id);
        }
        // 7.处理响应
        if (pattern_name_client.call(se_pattern_name))
        {
              //加载成功启动对准
            if(se_pattern_name.response.result)
            {
              run_con.run_step = Retreat;
              run_con.will_run = true ;
              run_con.get_goal = true;
              run_con.goal_pose.x =   -0.001 - run_con.laser_x;
              run_con.goal_pose.y = 0.0;
              run_con.goal_pose.yaw = 0.0;
              ac_result.result = 0;
            }
            else
            {
              ROS_ERROR("%s can not load",se_pattern_name.request.name.data.c_str());
              ac_result.result = ac_Pcd_Fail;
            }
        }
        else
        {
            ROS_ERROR("pattern_name_client call fail");
            ac_result.result = ac_Server_Fail;
        }
      }
    }
    
    ros::Rate loop_rate(10);    //设置发送数据的频率为10Hz
    while(ros::ok())
    { 
        ros::spinOnce();
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起

        as_->publishFeedback(ac_feed);
         //cancel
        if (as_->isPreemptRequested())
        {
            run_con_clear();
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            cmd_vel_pub.publish(cmd_vel);

            printf("cancel the action!\n");
            ac_result.result = ac_Cancel;
            as_->setPreempted(ac_result);
            return;
        }

        if(ac_result.result)
        {
            printf("ac_result.result:%d\n",ac_result.result);
            ac_result.pose.x = run_con.pose.x + run_con.laser_x*cos(run_con.pose.yaw);
            ac_result.pose.y = run_con.pose.y + run_con.laser_x*sin(run_con.pose.yaw);
            ac_result.pose.theta = run_con.pose.yaw;

            if(ac_result.result == ac_Success)
            {
              if(fabs(ac_result.pose.x) > 0.01 || fabs(ac_result.pose.y) > 0.01 || fabs(ac_result.pose.theta - run_con.goal_pose.yaw) > 0.0523)
              {
                //误差大于1cm 3度
                ac_result.result = ac_Excessive_Error;
                run_con_clear();
              }
            }

            as_->setPreempted(ac_result);
            ac_result.result = 0;
            return;
        }
    }
  }


  /// @brief 计算点在直线的哪一边
  /// @param nx 
  /// @param ny 
  /// @param nyaw 
  /// @param gx 
  /// @param gy 
  /// @param gyaw 
  /// @return 
  bool  Ros_end_control::caul_goal(double nx,double ny,double nyaw,double gx,double gy,double gyaw)
  {
    //-1 = k *(-1.0)/k
    double k = -1.0 / tan(gyaw);
    //y = kx + b
    //Ax+By+C = 0
    double B = -1;
    double A = k;
    double C= -k*gx + gy;
    //D=A*xp+B*yp+C
    double D = A*nx+B*ny+C;
    if(D <= 0)
    {
      return true;
    }
    return false;
  }

  //costmap
void Ros_end_control::costmap_Callback(const nav_msgs::OccupancyGrid::ConstPtr & msg)
{
    std::lock_guard<std::mutex> lock(map_update_mutex_);
    if (!costmap_) 
    {
      ROS_WARN("scan icp Initiate new costmap");
      costmap_ = std::make_shared<costmap_2d::Costmap2D>(msg->info.width, msg->info.height, msg->info.resolution,
                                                        msg->info.origin.position.x, msg->info.origin.position.y);
    } 
    else 
    {
      //ROS_WARN("Update costmap!");
      costmap_->resizeMap(msg->info.width, msg->info.height, msg->info.resolution,msg->info.origin.position.x, msg->info.origin.position.y);
    }

    costmap_->setCost(0, 1, costmap_2d::NO_INFORMATION);
    
    uint32_t x;
    for (uint32_t y = 0; y < msg->info.height; y++) 
    {
      for (x = 0; x < msg->info.width; x++) 
      {
        
        if (msg->data[y * msg->info.width + x] < 0)
         {
          costmap_->setCost(x, y, costmap_2d::NO_INFORMATION);
          continue;
        }
        costmap_->setCost(x, y, cost_translation_[msg->data[y * msg->info.width + x]]);
      }
    }
  
  costmap_model_ = boost::make_shared<base_local_planner::CostmapModel>(*costmap_.get());
  costmap_init = true;
}

//costmap updata
void Ros_end_control::costmap_updata_Callback(const map_msgs::OccupancyGridUpdate::ConstPtr & msg) 
{
  std::lock_guard<std::mutex> lock(map_update_mutex_);
  if (!costmap_) 
  {
    ROS_WARN("Costmap not initiate yet");
    return;
  }

  if (msg->width * msg->height != msg->data.size()  || msg->x + msg->width > costmap_->getSizeInCellsX()  || msg->y + msg->height > costmap_->getSizeInCellsY())
  {
    ROS_ERROR("Costmap update got invalid data set");
    return;
  }

  size_t index = 0;
  int x;
  for (auto y = msg->y; y < msg->y + msg->height; y++) 
  {
    for (x = msg->x; x < msg->x + msg->width; x++) 
    {
      
      if (msg->data[index] < 0) {
        costmap_->setCost(x, y, costmap_2d::NO_INFORMATION);
        index++;
        continue;
      }
      costmap_->setCost(x, y, cost_translation_[msg->data[index++]]);
    }
  }
}

  void Ros_end_control::costmap_robot_poseCallback(const geometry_msgs::Pose2D::ConstPtr &pose)
  {
    run_con.map_pose.x = pose->x;
    run_con.map_pose.y = pose->y;
    run_con.map_pose.yaw = pose->theta; //角度
  }

  void Ros_end_control::icp_fitness_scoreCallback(const std_msgs::Float64::ConstPtr &score)
  {
    run_con.score = score->data;
  }

void Ros_end_control::scan_cb(const sensor_msgs::LaserScan::ConstPtr &msg)
{
  if(get_scan_frame_id_flag == false)
  {
    scan_frame_id = msg->header.frame_id;
    get_scan_frame_id_flag = true;
  }
}

  /// @brief 车子当前位置获得
  void Ros_end_control::robot_pose_get()
  {
      tf::StampedTransform transform;
      tf::Quaternion q;
      double pose_th = 0.0;
      double roll,pitch,yaw;

      try 
      {
          //得到坐标odom和坐标base_link之间的关系
          robot_listener->waitForTransform("result",scan_frame_id, ros::Time(0), ros::Duration(0.5));
          robot_listener->lookupTransform("result",scan_frame_id,ros::Time(0), transform);

          geometry_msgs::TransformStamped transform_pose;
          tf::transformStampedTFToMsg(transform, transform_pose);
          current_pose_ros.position.x = transform.getOrigin().x();
          current_pose_ros.position.y = transform.getOrigin().y();
          current_pose_ros.position.z = 0.0;
          current_pose_ros.orientation= transform_pose.transform.rotation;
      
          tf::quaternionMsgToTF(current_pose_ros.orientation, q);
          pose_th = tf::getYaw(q);  //角度

          //得到坐标base_laser和坐标laser之间的关系
          robot_listener->waitForTransform("base_link",scan_frame_id, ros::Time(0), ros::Duration(0.5));
          robot_listener->lookupTransform("base_link",scan_frame_id,ros::Time(0), transform);
          run_con.laser_x = transform.getOrigin().x();

          
          tf::transformStampedTFToMsg(transform, transform_pose);
          tf::quaternionMsgToTF(transform_pose.transform.rotation, q);
          tf::Matrix3x3(q).getRPY(roll,pitch,yaw);
          //printf("roll:%.2f pitch:%.2f yaw:%.2f\n",roll,pitch,yaw);
          //判断翻滚角接近于3.14则为雷达倒装 角度和y镜像
          roll = fabs(roll);
          if(fabs(roll - 3.14) < 0.5)
          {
            pose_th = -pose_th;
            current_pose_ros.position.y = -current_pose_ros.position.y;
          }
          
        //tf获得的有滞后问题应该是carto转换滞后
        /*
          //得到坐标base_laser和坐标laser之间的关系
          robot_listener->waitForTransform("odom","base_link", ros::Time(0), ros::Duration(0.5));
          robot_listener->lookupTransform("odom","base_link",ros::Time(0), transform);
          run_con.map_pose.x = transform.getOrigin().x();
          run_con.map_pose.y = transform.getOrigin().y();
          run_con.map_pose.yaw = tf::getYaw(transform_pose.transform.rotation);  //角度
          */
          
          
          run_con.pose.x     =  current_pose_ros.position.x - run_con.laser_x*cos(pose_th);
          run_con.pose.y     =  current_pose_ros.position.y - run_con.laser_x*sin(pose_th);
          run_con.pose.yaw   = pose_th;

          // printf("now1 x:%.3f  y:%.3f  yaw:%.3f roll:%.3f\n",  run_con.pose.x, run_con.pose.y , run_con.pose.yaw,roll);

      } 
      catch (std::exception e ) 
      {
          run_con.pose.x     = 0;
          run_con.pose.y     = 0;
          run_con.pose.yaw   = 0;
          ROS_WARN ("robot_pose_get cannot get robot pose!");

          cmd_vel.linear.x = 0.0;
          cmd_vel.angular.z = 0.0;
          run_con_clear();
          ac_result.result = ac_Loss_Icp;
      }
      //printf("now x:%.3f  y:%.3f  yaw:%.3f\n",run_con.pose.x,run_con.pose.y,run_con.pose.yaw);

    
}

bool Ros_end_control::getRobotPose(geometry_msgs::PoseStamped& global_pose) const
{
  static tf2_ros::Buffer tf_;
  tf2::toMsg(tf2::Transform::getIdentity(), global_pose.pose);
  geometry_msgs::PoseStamped robot_pose;
  tf2::toMsg(tf2::Transform::getIdentity(), robot_pose.pose);
  robot_pose.header.frame_id = "base_link";
  robot_pose.header.stamp = ros::Time();
  ros::Time current_time = ros::Time::now();  // save time for checking tf delay later

  // get the global pose of the robot
  try
  {
    tf_.transform(robot_pose, global_pose, "odom");
  }
  catch (std::exception e ) 
  {
    return false;
    printf("can get pose\n");
  }

  return true;
}

/// @brief 计算两点间的距离
/// @param x1 
/// @param y1 
/// @param x2 
/// @param y2 
/// @return 
double Ros_end_control::caul_distance(double x1,double y1,double x2,double y2)
{
  double dis = sqrt(pow(x1 - x2,2) + pow(y1 - y2,2));
  return dis;
}

 /// @brief   判断是否大于0
 /// @param m 
 /// @return true 大于0
 bool Ros_end_control::get_symbol(double m)
 {
    if(m >= 0)
    {
      return true;
    }
    return false;
 }

  /// @brief 参数清除
  void Ros_end_control::run_con_clear()
  {
    run_con.run_step = 0;
    run_con.last_run_step = 0;
    run_con.will_run = false;
    run_con.get_goal = false;
  }

  /// @brief                实际速度计算函数
  /// @param nx       当前位置x坐标
  /// @param ny       当前位置y坐标
  /// @param nyaw 当前位置航向角
  /// @param gx       目标位置x坐标
  /// @param gy       目标位置y坐标
  /// @param gyaw 目标位置航向角
  /// @param cmd_vel 速度输出
  /// @return 
  int Ros_end_control::caul_pi(double nx,double ny,double nyaw,double gx,double gy,double gyaw, geometry_msgs::Twist &cmd_vel,bool goal_status,bool dir)
  {
    static double last_beta = 0.0,be_w = 0.0,al_w = 0.0,last_alpha = 0.0; 
    static int success_cnt = 0;
    double rho = caul_distance(nx,ny,gx,gy);
    double y_diff = gy - ny;
    double x_diff = gx - nx;
    static double v = 0.0;
    double alpha = 0.0;
    double beta = 0.0;

    if(run_con.last_run_step != Align_Ment)
    {
      v= 0.0;
    }
    if(v <= Kp_rho * rho)
    {
      v +=  0.005;
    }
    else
    {
      v = Kp_rho * rho;
    }

    printf("v:%.2f\n",v);

    //抑制y轴偏差导致w抖动厉害
    x_diff = 0.6;

    if(dir == true)
    {
        //alpha = fmod( atan2(y_diff, x_diff) - nyaw + 3*M_PI , 2 * M_PI ) - M_PI ;
        alpha = angles::shortest_angular_distance(nyaw,atan2(y_diff, x_diff));
    }
    else
    {
      alpha -= fmod( atan2(y_diff, x_diff) - nyaw + 2*M_PI, 2 * M_PI ) - M_PI;
      //printf("v:%.2f alpha:%.2f\n", v,alpha);
    }
    
    //beta = fmod(gyaw - nyaw - alpha + 3*M_PI , 2 * M_PI ) - M_PI;
    beta = angles::shortest_angular_distance(alpha,gyaw - nyaw);

    if(v < 0.005){v = 0.005;}
    if(alpha > M_PI / 2 || alpha < -M_PI / 2)
    {
      // v = -v;
    }

    double w = Kp_alpha * alpha - Kp_beta * beta;
   
    /*
    be_w += Kp_beta * (beta - last_beta);
    last_beta = beta;
    if(be_w > 4)
    {
      be_w = 4;
    }
    else if(be_w < -4)
    {
      be_w = -4;
    }
  
    al_w += Kp_alpha * (alpha - last_alpha);
    last_alpha = alpha;
    if(al_w > 4)
    {
      al_w = 4;
    }
    else if(al_w < -4)
    {
      al_w = -4;
    }

    double w = 0.0;
    w = al_w - be_w;
    */
    
  //当速度低于0.05后则只处理角度误差
   if(v < 0.1)
    {
       if(dir == true)
       {
        
          //w = (fmod(gyaw - nyaw  + 3*M_PI , 2 * M_PI ) - M_PI);
          //w = angles::shortest_angular_distance(nyaw,gyaw) * v *15;
          //抑制角速度
          //w = w*v*15;
       }
       else
       {
          w = -(fmod(gyaw - nyaw  + 3*M_PI , 2 * M_PI ) - M_PI);
       }
    
       //printf("v:%.2f   w:%.2f\n",v,w);
      //printf("er_x:%.3f   er_y:%.3f   er_yaw:%.3f\n",fabs(nx - gx),fabs(ny - gy),fabs(nyaw - gyaw));
        
    }
    else
    {
      //printf("be_w:%.2f  al_w:%.2f\n",be_w,al_w);
      //printf("er_x:%.3f   er_y:%.3f   er_yaw:%.3f\n",fabs(nx - gx),fabs(ny - gy),fabs(nyaw - gyaw));
    }
    

    if(w > Max_w)
    {
      w = Max_w;
    }
    else if(w < -Max_w)
    {
      w = -Max_w;
    }

     if(v > Max_v)
    {
      v = Max_v;
    }
    
    else if(v < -Max_v)
    {
      v = -Max_v;
    }
   
    //if(v < -0.001 && fabs(w) < 0.01)
    //到达直线的另外一边则刚好超过位置
    //if(caul_goal(nx,ny,nyaw,gx,gy,gyaw) != goal_status)
    if(nx >= gx)
    {
      v  = 0.0;
      w = 0.0;
      success_cnt++;
      if(success_cnt > 5)
      {
        success_cnt = 0;
        v  = 0.0;
        w = 0.0;
        printf("er_x:%.3f   er_y:%.3f   er_yaw:%.3f\n",fabs(nx - gx),fabs(ny - gy),fabs(nyaw - gyaw));
        cmd_vel.linear.x = v;
        cmd_vel.angular.z = w;
        //PID残余清除
        last_beta = be_w = al_w = last_alpha = 0.0; 
        return 1;
      }
    }
    else
    {
      success_cnt = 0;
    }

     if(dir == true)
      {
          cmd_vel.linear.x = v;
          cmd_vel.angular.z = w;
      }
      else
      {
        cmd_vel.linear.x = -v;
        cmd_vel.angular.z = -w;
      }
    return 0;
  }


  /// @brief               仿真速度计算函数
  /// @param nx       当前位置x坐标
  /// @param ny       当前位置y坐标
  /// @param nyaw 当前位置航向角
  /// @param gx       目标位置x坐标
  /// @param gy       目标位置y坐标
  /// @param gyaw 目标位置航向角
  /// @param cmd_vel 速度输出
  /// @return 
  int Ros_end_control::simula_caul_pi(double nx,double ny,double nyaw,double gx,double gy,double gyaw, geometry_msgs::Twist &cmd_vel ,bool goal_status)
  {
     static double last_beta = 0.0,be_w = 0.0,al_w = 0.0,last_alpha = 0.0; 
    double rho = caul_distance(nx,ny,gx,gy);
    double y_diff = gy - ny;
    double x_diff = gx - nx;
    //fmod不能小于0不然会出错
    double alpha = fmod( atan2(y_diff, x_diff) - nyaw + M_PI , 2 * M_PI ) - M_PI ;
    double beta = fmod(gyaw - nyaw - alpha + M_PI , 2 * M_PI ) - M_PI;
    double v = Kp_rho * rho;

    if(v < 0.01){v = 0.01;}
    if(alpha > M_PI / 2 || alpha < -M_PI / 2)
    {
       //v = -v;
    }

    be_w += Kp_beta * (beta - last_beta);
    last_beta = beta;
    if(be_w > 1)
    {
      be_w = 1;
    }
    else if(be_w < -1)
    {
      be_w = -1;
    }
    //double w = Kp_alpha * alpha - Kp_beta * beta;
    al_w += Kp_alpha * (alpha - last_alpha);
    last_alpha = alpha;
    if(al_w > 4)
    {
      al_w = 4;
    }
    else if(al_w < -4)
    {
      al_w = -4;
    }

    double w = al_w - be_w;

  //当速度低于0.05后则只处理角度误差
   if(v < 0.05)
    {
      w = fmod(gyaw - nyaw  + M_PI , 2 * M_PI ) - M_PI;
    }

    if(w > Max_w)
    {
      w = Max_w;
    }
    else if(w < -Max_w)
    {
      w = -Max_w;
    }

     if(v > Max_v)
    {
      v = Max_v;
    }
    
    else if(v < -Max_w)
    {
      v = -Max_v;
    }
   
    if(caul_goal(nx,ny,nyaw,gx,gy,gyaw) != goal_status)
    {
      v  = 0.0;
      w = 0.0;
      printf("er_x:%.3f   er_y:%.3f   er_yaw:%.3f\n",fabs(nx - gx),fabs(ny - gy),fabs(nyaw - gyaw));
      cmd_vel.linear.x = v;
      cmd_vel.angular.z = w;
      //PID残余清除
      last_beta = be_w = al_w = last_alpha = 0.0; 
      return 1;
    }

    cmd_vel.linear.x = v;
    cmd_vel.angular.z = w;
    return 0;
  }

  /// @brief 仿真路径计算
  /// @param nx 
  /// @param ny 
  /// @param nyaw 
  /// @param gx 
  /// @param gy 
  /// @param gyaw 
  /// @param path 
  /// @param goal_status 
  void Ros_end_control::simulation_path(double nx,double ny,double nyaw,double gx,double gy,double gyaw, nav_msgs::Path &path,bool goal_status)
  {
    double dt = 0.25;
    double sum_dt = 0.0;
    double pose_x = nx;
    double pose_y = ny;
    double pose_yaw = nyaw;
    geometry_msgs::Twist cmd_vel;
    geometry_msgs::PoseStamped pose;

    path.header.stamp = ros::Time::now();
    path.header.frame_id = "map";

    pose.header.stamp = ros::Time::now();
    pose.header.frame_id = "map";

    while(simula_caul_pi(pose_x,pose_y,pose_yaw,gx,gy,gyaw,cmd_vel,goal_status) != 1 || sum_dt > 60)
    {
      double v = cmd_vel.linear.x;
      double w =  cmd_vel.angular.z;
      pose_yaw +=  w * dt;
      pose_x += v * cos(pose_yaw) * dt;
      pose_y += v * sin (pose_yaw) * dt;
      sum_dt += dt;

      pose.pose.position.x = pose_x;
      pose.pose.position.y = pose_y;
      geometry_msgs::Quaternion q = tf::createQuaternionMsgFromYaw(pose_yaw);
      pose.pose.orientation = q;
      path.poses.push_back(pose);
    }
    printf("sum_dt:%.2f\n",sum_dt);
    
  }

  /// @brief 旋转控制
  /// @param cmd_vel  速度输入
  /// @param n_angel  当前角度
  /// @param goal_angel   目标角度
  /// @param max_w  最大角速度
  /// @return  true到达目标角度
  bool Ros_end_control::rotation_control(geometry_msgs::Twist &cmd_vel,double n_angel,double goal_angel,double max_w)
  {
      double err_anglle = angles::shortest_angular_distance(n_angel,goal_angel);
      double w = err_anglle * 0.8;
      if(w > max_w){w =max_w;}
      if(w < -max_w){w = -max_w;}

      cmd_vel.linear.x = 0.0;
      cmd_vel.angular.z = w;

      if(fabs(err_anglle) < 0.05)
      {
          cmd_vel.linear.x = 0.0;
          cmd_vel.angular.z = 0.0;
          return true;
      }
      return false;
  }

  /// @brief  直行控制
  /// @param cmd_vel  速度输入
  /// @param n_x  当前x
  /// @param n_y  当前y
  /// @param goal_x   目标x
  /// @param goal_y   目标y
  /// @param max_v  最大速度
  ///@param first_y  开始y的符号
  /// @return true为到达目标点
  bool Ros_end_control::straight_control(geometry_msgs::Twist &cmd_vel,double n_x,double n_y,double goal_x,double goal_y,double max_v,bool first_y)
  {
      static int fina_cnt = 0;
      double rho = caul_distance(n_x,n_y,goal_x,goal_y);
      double v = rho * 0.8;
      if(v > max_v){v = max_v;}
      cmd_vel.linear.x = v;
      cmd_vel.angular.z = 0.0;

      if(fabs(n_y - goal_y) < 0.01 ||  get_symbol(n_y) != first_y)
      {
          fina_cnt++;
          if(fina_cnt > 5)
          {
            fina_cnt = 0;
            cmd_vel.linear.x = 0.0;
            cmd_vel.angular.z = 0.0;
            return true;
          }
      }
      else
      {
        fina_cnt = 0;
      }
      return false;
  }

  /// @brief 直行避障
  /// @param  
  /// @return  true有障碍
  bool Ros_end_control::direct_obstacle_avoidance(void)
  {
    static int stop_cnt = 0;
    if(costmap_init)
    {
      double footprint_cost = costmap_model_->footprintCost(run_con.map_pose.x, run_con.map_pose.y, run_con.map_pose.yaw, footprint, 0.0, 0.0);
      //printf("footprint_cost:%f\n",footprint_cost);
      //printf("x:%.2f  y:%.2f  yaw:%.2f\n",run_con.map_pose.x,run_con.map_pose.y,run_con.map_pose.yaw);
      if(footprint_cost == -1)
      {
        stop_cnt = 10;
      }
      else if(stop_cnt)
      {
        stop_cnt--;
      }
      if(stop_cnt)
      {
        return true;
      }
      return false;
    }
  }

  /// @brief  后退控制
  /// @param cmd_vel  速度输入
  /// @param n_x  当前x
  /// @param n_y  当前y
  /// @param goal_x   目标x
  /// @param goal_y   目标y
  /// @param max_v  最大速度
  /// @return true为到达目标点
  bool Ros_end_control::retreat_control(geometry_msgs::Twist &cmd_vel,double n_x,double n_y,double goal_x,double goal_y,double max_v)
  {
    double rho = caul_distance(n_x,n_y,goal_x,goal_y);
    double v = rho * 0.8;
    if(v > max_v){v = max_v;}
    cmd_vel.linear.x = -v;
    cmd_vel.angular.z = 0.0;

    if(n_x  < goal_x)
    {
        cmd_vel.linear.x = 0.0;
        cmd_vel.angular.z = 0.0;
        return true;
    }
    return false;
  }

  /// @brief 运动控制
  void Ros_end_control::run_control()
  {
    static double first_angle = 0.0;
    static int socr_err_cnt = 0,socr_err1_cnt = 0,nearl_err_cnt = 0,first_cnt = 0;
    static bool first_y_state = false;

    //get pose
    if(get_scan_frame_id_flag)
    {
       robot_pose_get();
    }

    //整体控制
     switch (run_con.run_step)
     {
      case Start:   //开始
      {
        if(run_con.will_run == true &&  run_con.get_goal == true)
        {
          //匹配分数大于0.000035则表示匹配丢失
          if(run_con.score >  Min_socre)
          {
            socr_err1_cnt++;
            //5次滤波
            if(socr_err1_cnt > 20)
            {
              cmd_vel.linear.x = 0.0;
              cmd_vel.angular.z = 0.0;
              socr_err_cnt = 0;
              socr_err1_cnt = 0;
              run_con_clear();
              printf("loss icp socre\n");
              ac_result.result = ac_Loss_Icp;
            }
            first_cnt = 0;
            return;
          }
          else
          {
            socr_err1_cnt = 0;
            first_cnt++;
            //虑出之前pcd文件的干扰
            if(first_cnt > 5)
            {
              first_cnt = 0;
            }
            else
            {
              return;
            }
          }

          //判断离得太近
          if(run_con.pose.x > - (Before_Target + run_con.laser_x))
          {
              nearl_err_cnt++;
              if(nearl_err_cnt > 10)
              {
                printf("the pose is too nearl!\n");
                run_con_clear();
                ac_result.result = ac_Too_Nearl;
                nearl_err_cnt = 0;
              }
              return;
          }
          else
          {
            nearl_err_cnt = 0;
          }
     
        //距离y轴在+-10cm内直接跳到对准中心角度步骤
          if(fabs(run_con.pose.y) <= 0.1)
          {
            run_con.run_step = Second_Rotation;
            return;
          }

          double y_diff = run_con.goal_pose.y - run_con.pose.y;
          double x_diff = run_con.goal_pose.x - run_con.pose.x - Before_Target;

          first_angle = atan2(y_diff, x_diff);
          run_con.run_step = First_Rotation;
        } 
        break;
      }
      case First_Rotation:    //第一次旋转到定位点前n米的角度
      {
        bool res = rotation_control(cmd_vel,run_con.pose.yaw,first_angle,0.3);
        if(res )
        {
          run_con.run_step = Straight_Travel;
          //标记当前y的正负，用于直行防止走过头
          first_y_state = get_symbol(run_con.pose.y);
        }
        break;
      }
      case Straight_Travel:   //直行到定位n米的位置
      {
          bool res = straight_control(cmd_vel,run_con.pose.x,run_con.pose.y,run_con.goal_pose.x - Before_Target,run_con.goal_pose.y,0.2,first_y_state);
          if(res )
          {
            run_con.run_step = Second_Rotation;
          }
          break;
      }
      case Second_Rotation:   //旋转到定位点方向
      {
        //当前点与目标点作直角三角形。旋转到夹角。
          double y_diff = run_con.goal_pose.y - run_con.pose.y;
          double x_diff = run_con.goal_pose.x - run_con.pose.x;
          double aim_yaw = atan2(y_diff,x_diff);
          printf("aim_yaw%f\n",aim_yaw);
          bool res = rotation_control(cmd_vel,run_con.pose.yaw,aim_yaw,0.3);
          // bool res = rotation_control(cmd_vel,run_con.pose.yaw,0.0,0.3);
          
          if(res )
          {
             run_con.run_step = Align_Ment;
          }
          break;
      }
      case Align_Ment:     //对准
      {
           if(run_con.will_run == true &&  run_con.get_goal == true)
          {
            int res = caul_pi(run_con.pose.x,run_con.pose.y,run_con.pose.yaw,run_con.goal_pose.x,run_con.goal_pose.y, run_con.goal_pose.yaw,cmd_vel,run_con.first_goal_status,true);
            if(res == 1)
            {
              run_con.run_step = Align_Ment_Complete;
              ac_result.result = ac_Success;
            }
          }
          break;
      }
      case Align_Ment_Complete: //对准完成
      {
        //printf("er_x:%.3f   er_y:%.3f   er_yaw:%.3f\n",fabs(run_con.pose.x - run_con.goal_pose.x),fabs(run_con.pose.y - run_con.goal_pose.y),fabs(run_con.pose.yaw - run_con.goal_pose.yaw));
        break;
      }
      case  Retreat:  //后退
      {
        bool res = retreat_control(cmd_vel,run_con.pose.x,run_con.pose.y,run_con.goal_pose.x - Before_Target,run_con.goal_pose.y,0.2);
        if(res )
        {
          run_con_clear();
          cmd_vel_pub.publish(cmd_vel);
          printf("retreat is success\n");
          ac_result.result = ac_Back_Success;
        }
        break;
      }

     default:break;
     }

    run_con.last_run_step = run_con.run_step;

    if(run_con.run_step && run_con.run_step != Align_Ment_Complete)
    {

      //匹配分数大于0.000035则表示匹配丢失
      if(run_con.score <  Min_socre)
      {
        socr_err_cnt = 0;
      }
      else
      {
        socr_err_cnt++;
      }

      //车子转到无法扫描的地方去了，信号丢失
      if(socr_err_cnt > 20)
      {
        cmd_vel.linear.x = 0.0;
        cmd_vel.angular.z = 0.0;
        socr_err_cnt = 0;
        run_con_clear();
        printf("loss icp socre\n");
        ac_result.result = ac_Loss_Icp;
      }

      //遇到障碍物。只检测直行方面避障，旋转方向目标角度不好确定，扩大车身模型实现避障
      if(direct_obstacle_avoidance())
      {
        cmd_vel.linear.x = 0.0;
        cmd_vel.angular.z = 0.0;
        //printf("direct_obstacle_avoidance\n");
        ac_feed.obstacle = true;
      }
      else
      {
        ac_feed.obstacle = false;
      }

      cmd_vel_pub.publish(cmd_vel);
    }
    else
    {
      socr_err_cnt = 0;
    }
    ac_feed.step = run_con.run_step;
    ac_feed.socre = run_con.score;
    ac_feed.pose.x = current_pose_ros.position.x;
    ac_feed.pose.y = current_pose_ros.position.y;
    ac_feed.pose.theta = run_con.pose.yaw;
  }
}

int main(int argc,char **argv)
{    
   ros::init(argc,argv,"ros_end_control_nodel");            //解析参数，命名节点为 talker
  ros::NodeHandle n;
   std::shared_ptr<ros_end_control::Ros_end_control> robot_end_control_;
   robot_end_control_ =  std::make_shared<ros_end_control::Ros_end_control>();
   ros::spin();
   return 0;
}
/*************************************************************************/
