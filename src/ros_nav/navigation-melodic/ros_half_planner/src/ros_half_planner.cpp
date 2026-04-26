#include <ros/ros.h>                      //类似 C 语言的 stdio.h
#include "ros_half_planner.h"

/*************************************************************************/
//轨道加载
void load_line(void)
{
  
   xju::pnc::lines_type lines;

    Json::Reader reader;
    //clear
    Json::Value json_track_point;
    json_track_point.clear();
    std::ifstream ifs(path_point_filename, std::ifstream::in);//only read
    if(ifs.is_open())
    {
       std::cout<<"file is already open"<<std::endl;
    }
    
    if(!reader.parse(ifs, json_track_point))
    {
        ROS_ERROR("start_track error");
        ifs.close();
    }
    else
    {
      xju::pnc::line_t line;
    
      for(int i=0;i<json_track_point["lines"].size();i++)
      {
        line.a.x = json_track_point["lines"][i]["edge"][(int)0]["x"].asDouble();
        line.a.y = json_track_point["lines"][i]["edge"][(int)0]["y"].asDouble();

        int size = json_track_point["lines"][i]["edge"].size();
        line.b.x = json_track_point["lines"][i]["edge"][size-1]["x"].asDouble();
        line.b.y = json_track_point["lines"][i]["edge"][size-1]["y"].asDouble();

        line.edge.edge.clear();
        for(int j=0;j<size;j++)
        {
           geometry_msgs::PoseStamped temp_pose;
          temp_pose.pose.position.x = json_track_point["lines"][i]["edge"][j]["x"].asDouble();
          temp_pose.pose.position.y = json_track_point["lines"][i]["edge"][j]["y"].asDouble();
          double yaw = json_track_point["lines"][i]["edge"][j]["theta"].asDouble();
          geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(yaw);
          //yaw
          temp_pose.pose.orientation.z = goal_quat.z;
          temp_pose.pose.orientation.w = goal_quat.w;
          line.edge.edge.emplace_back(temp_pose);
        }

        //计算权
        line.edge.cost = 0;
        for(int k = 0;k<line.edge.edge.size();k++)
        {
          if(k+1 < line.edge.edge.size())
          {
            line.edge.cost += std::hypot(line.edge.edge[k].pose.position.x - line.edge.edge[k+1].pose.position.x,
                                                                      line.edge.edge[k].pose.position.y - line.edge.edge[k+1].pose.position.y);
          }
        }
         ROS_WARN("line cost:%.2f edge:%d",line.edge.cost,line.edge.edge.size());

         //golist
        line.go_list.clear();
        int size1 = json_track_point["lines"][i]["go_list"].size();
        for(int k=0;k<size1;k++)
        {
          xju::pnc::go_list_t golist;
          golist.id = json_track_point["lines"][i]["go_list"][k]["id"].asInt();

          int size2 =  json_track_point["lines"][i]["go_list"][k]["points"].size();
          for(int n=0;n<size2;n++)
          {
             xju::pnc::point_t point;
             point.x =  json_track_point["lines"][i]["go_list"][k]["points"][n]["x"].asDouble();
             point.y =  json_track_point["lines"][i]["go_list"][k]["points"][n]["y"].asDouble();
             golist.points.emplace_back(point);
          }
          line.go_list.emplace_back(golist);
        }
        lines.emplace_back(line);
      }

       std::string msg = "read file successful, got " + std::to_string(lines.size()) + " lines";
       ROS_WARN("%s",msg.c_str());
       half_struct_planner_->set_traffic_route(lines);
    }
    ifs.close();
}

void goal_Callback(const geometry_msgs::PoseStamped &pose)
{
  static int count = 0;
  static geometry_msgs::PoseStamped start, goal;

  goal = pose;

  half_struct_planner_ = std::make_shared<xju::pnc::HalfStructPlanner>();
  half_struct_planner_->init();
  //sleep(2);
  load_line();
  half_struct_planner_->set_costmap(costmap_);
  auto poses = half_struct_planner_->get_path(robot_pose, goal);

  //pub reedback
  std_msgs::String str;
  if(poses.empty())
  {
     str.data = "false";
  }
  else
  {
     str.data = "true";

    /**/
     //path pub
    nav_msgs::Path path;
    path.header.frame_id = "map";
    path.header.stamp = ros::Time::now();
    for(auto const& pose : poses)
    {
      path.poses.push_back(pose);
    }
    ros::param::set("/move_base/TebLocalPlannerROS/plan_need_clear",true);
    path_pub_.publish(path);
  }
   feedback_pub_.publish(str);
}

//costmap
void costmap_Callback(const nav_msgs::OccupancyGrid::ConstPtr & msg)
{
    std::lock_guard<std::mutex> lock(map_update_mutex_);
    if (!costmap_) 
    {
      ROS_WARN("Initiate new costmap");
      costmap_ = std::make_shared<costmap_2d::Costmap2D>(msg->info.width, msg->info.height, msg->info.resolution,
                                                        msg->info.origin.position.x, msg->info.origin.position.y);
    } 
    else 
    {
      ROS_WARN("Update costmap!");
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
}

//costmap updata
void costmap_updata_Callback(const map_msgs::OccupancyGridUpdate::ConstPtr & msg) 
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

//获得底盘位置
auto get_robto_pose()->geometry_msgs::PoseStamped
{
  //pose  激光雷达在地图中的位置
    tf::StampedTransform transform;
    tf::Quaternion q;
    geometry_msgs::PoseStamped current_pose_ros;
    try 
    {
        //得到坐标map和坐标laser之间的关系 多线雷达为laser1
        robot_listener->waitForTransform("map","base_link", ros::Time(0), ros::Duration(0.2));
        robot_listener->lookupTransform("map","base_link",ros::Time(0), transform);

        geometry_msgs::TransformStamped transform_pose;
        tf::transformStampedTFToMsg(transform, transform_pose);
        current_pose_ros.pose.position.x = transform.getOrigin().x();
        current_pose_ros.pose.position.y = transform.getOrigin().y();
        current_pose_ros.pose.position.z = 0.0;
        current_pose_ros.pose.orientation= transform_pose.transform.rotation;
    } 
    catch (std::exception e ) 
    {
        ROS_WARN("half planner cannot get robot pose!");
    }
    return current_pose_ros;
}

int main(int argc,char **argv)
{
  ros::init(argc,argv,"ros_half_planner");            //解析参数，命名节点为 ultr_talker
  ros::NodeHandle private_nh("~");
  ros::Subscriber sub1= private_nh.subscribe("/move_base_simple/goal1",100,goal_Callback);  //
  ros::Subscriber sub2 = private_nh.subscribe("/move_base/global_costmap/costmap", 1, costmap_Callback);
  ros::Subscriber sub3 = private_nh.subscribe("/move_base/global_costmap/costmap_updates", 1, costmap_updata_Callback);

  feedback_pub_ =  private_nh.advertise<std_msgs::String>("feedback", 10);
  path_pub_  = private_nh.advertise<nav_msgs::Path>("traffic_path", 10);

  private_nh.param<std::string>("path_point_filename",path_point_filename,
                                   std::string("/home/zkwl/catkin_ws/src/ros_robot_control_json/json/path_point.json"));

  if (!cost_translation_)
  {
    cost_translation_ = new uint8_t[101];
    for (int i = 0; i < 101; ++i) 
    {
      cost_translation_[i] = static_cast<uint8_t>(i * 254 / 100);
    }
  }

  /*
  half_struct_planner_ = std::make_shared<xju::pnc::HalfStructPlanner>();
  half_struct_planner_->init();
  sleep(2);

  load_line();*/

  robot_listener = new  (tf::TransformListener);

  ros::Rate loop_rate(20);   //设置循环的频率为20Hz，50ms
   while(ros::ok())
   {
      robot_pose = get_robto_pose();
      ros::spinOnce();      //集中处理本节点回调函数
      loop_rate.sleep();   //按前面设置的4Hz频率将程序挂起
    }
    return 0;
}
