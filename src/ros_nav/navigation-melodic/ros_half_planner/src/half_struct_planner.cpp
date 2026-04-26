//
// Created by tony on 2022/12/2.
//

#include "half_struct_planner.h"


namespace xju::pnc {
HalfStructPlanner::HalfStructPlanner() : node_num_(0),
                                         costmap_(nullptr),
                                         graph_(nullptr),
                                         graph_bk_(nullptr) {
  ROS_WARN("Start HalfStructPlanner!");
}

HalfStructPlanner::~HalfStructPlanner() {
  if (node_num_ == 0) return;

  for (auto i = 0; i < node_num_; ++i) 
  {
    delete [] graph_[i];
    delete [] graph_bk_[i];
  }

  delete [] graph_;
  delete [] graph_bk_;
  ROS_WARN(" ~HalfStructPlanner()");
};

//初始化
void HalfStructPlanner::init() 
{
  ros::NodeHandle nh("~");
  vis_pub_      = nh.advertise<visualization_msgs::MarkerArray>("traffic_route", 1);
  res_pub_     = nh.advertise<nav_msgs::Path>("traffic_route_points", 1);
  //path_pub_  = nh.advertise<nav_msgs::Path>("traffic_path", 10);

  astar_ = std::make_shared<RRTstar_planner::RRTstarPlannerROS>();
}

//设置线段
void HalfStructPlanner::set_traffic_route(lines_type const& lines) {
  traffic_routes_.clear();
  traffic_routes_ = lines;
  show_traffic_route();
  printf("show_traffic_route:%d\n",traffic_routes_.size());
  calculate_pre_graph();
}

//根据起点终点获得路径
auto HalfStructPlanner::get_path(geometry_msgs::PoseStamped const& start,
                                 geometry_msgs::PoseStamped const& goal) -> std::vector<geometry_msgs::PoseStamped> {
  std::vector<geometry_msgs::PoseStamped> result {};
  if (!is_traffic_plan()) return result;


  for (auto i = 0; i < node_num_; ++i)
  {
    for (auto j = 0; j < node_num_; ++j) 
    {
      graph_[i][j] = graph_bk_[i][j];
    }
  }

  //add zxp 
  path_.start = start;
  path_.goal = goal;

  // 0 起点 1 终点
  nodes_[0] = start;
  nodes_[1] = goal;

  //如果起点和终点直线连接安全。且距离小于1m则直接rrt路径规划
  auto rrt_start = point_t {nodes_[0].pose.position.x, nodes_[0].pose.position.y};
  auto rrt_goal = point_t {nodes_[1].pose.position.x, nodes_[1].pose.position.y};
  if(line_safe(rrt_start, rrt_goal ) && distance(rrt_start,rrt_goal) <= 1.0)
  {
    //rrt inint
    astar_->initialize(costmap_);
    if(astar_->makePlan(start,goal,result) == true )
    {
      //ROS_ERROR("astart planner is false");
      ROS_WARN("start and goal is nearn rrt planner is true! ");
      return result;
    }
  }

  calculate_graph();

  //rrt 起始规划或者末端规划失败
   if(path_.near_start.points.empty()  || path_.near_goal.points.empty())
    {
      ROS_ERROR("rrt planner is failse! ");
      return result;
    }

  auto node_list = dijkstra();

  //add zxp
   std::vector<geometry_msgs::PoseStamped> path_poses;
   path_poses.clear();
   static int  last_node = -1;
   for (auto const& node : node_list)
  {
    if(last_node == 2 && node == 3)  //2->3
    {
      path_poses = path_.paths;
    }
    else if(last_node == 2)  //2->n
    {
      for(auto const& pose : all_paths)
      {
        //node == start
        if(pose.start == node+1 && pose.end == node && node % 2 == 0)
        {
          printf("add start:%d  end:%d  point_id:%d\n",pose.start,pose.end,path_.near_start.point_id);
           //反排列
          for(int me=pose.paths.size() - path_.near_start.point_id -1;me <  pose.paths.size();me++)
          {
            path_poses.emplace_back(pose.paths[me]);
          }
          
          break;
        }
        //node == end
        else  if(pose.start == node-1 && pose.end == node && node % 2 )
        {
          printf("add start:%d  end:%d  point_id:%d\n",pose.start,pose.end,path_.near_start.point_id);
          for(auto ms=path_.near_start.point_id;ms<pose.paths.size();ms++)
          {
            path_poses.emplace_back(pose.paths[ms]);
          }
          break;
        }
      }
    }
    else if(node == 3)  //n->3
    {
      
      for(auto const& pose : all_paths)
      {
        //last_node == start
        if(pose.start == last_node && pose.end == last_node+1  && last_node % 2 == 0)
        {
          printf("add start:%d  end:%d  point_id:%d\n",pose.start,pose.end,path_.near_goal.point_id);
          for(auto ms=0;ms<= path_.near_goal.point_id;ms++)
          {
            path_poses.emplace_back(pose.paths[ms]);
          }
          break;
        }
        //last_node == end
        else if(pose.start == last_node && pose.end == last_node-1  && last_node % 2)
        {
         printf("add start:%d  end:%d  point_id:%d\n",pose.start,pose.end,path_.near_goal.point_id);
          //反排列
          for(int me= 0 ;me < pose.paths.size()  - path_.near_goal.point_id ;me++)
          {
            path_poses.emplace_back(pose.paths[me]);
          }
          break;
        }
      }
    }
    else
    {
      
      for(auto const& pose : all_paths)  //n->n
      {
        if(pose.start == last_node && pose.end == node )
        {
           printf("add start:%d  end:%d\n",pose.start,pose.end);
          for(auto const& path : pose.paths)
          {
            path_poses.emplace_back(path);
          }
          break;
        }
      }
    }
    last_node = node; 
  }
  printf("path_poses size:%d\n",path_poses.size());

   //添加角度
    geometry_msgs::PoseStamped last_pose;
    for(auto i=0;i<path_poses.size();i++)
    {
      double theta = atan2((path_poses[i].pose.position.y - last_pose.pose.position.y ), path_poses[i].pose.position.x - (last_pose.pose.position.x ));
      //printf("theta:%.2f\n",theta);
      geometry_msgs::Quaternion goal_quat = tf::createQuaternionMsgFromYaw(theta);
      path_poses[i].pose.orientation.x = goal_quat.x;
      path_poses[i].pose.orientation.y = goal_quat.y;
      path_poses[i].pose.orientation.z = goal_quat.z;
      path_poses[i].pose.orientation.w = goal_quat.w;
      last_pose = path_poses[i];
    }

   
   //start end 要剔除一些重复点，不然teb会报错。
  if( path_.near_start.points.size() > 1)
  {
     //path_.near_start.points.erase(path_.near_start.points.begin());
    //path_.near_start.points.erase(path_.near_start.points.end());
  }

  if(path_poses.size() > 1)
  {
     path_poses.erase(path_poses.begin());
  }
  
   for(int  i = path_.near_start.points.size()-1; i>=0;i-- )
   {
       path_poses.insert(path_poses.begin(), path_.near_start.points[i]);
   }

  if( path_.near_goal.points.size() > 2)
  {
     path_.near_goal.points.erase(path_.near_goal.points.end());
     path_.near_goal.points.erase(path_.near_goal.points.begin());
  }
 
  if(path_poses.size() > 1)
  {
    //path_poses.erase(path_poses.end());
  }
  
   for(auto const& point : path_.near_goal.points )
   {
       path_poses.emplace_back(point);
   }
   //加入终点
  path_poses.emplace_back(path_.goal);

/*
//path pub
  nav_msgs::Path path;
  path.header.frame_id = "map";
  path.header.stamp = ros::Time::now();
  path.poses = result;
  for(auto const& pose : path_poses)
  {
    path.poses.push_back(pose);
  }
  path_pub_.publish(path);
  */
  

  /*
  nav_msgs::Path path1;
  path1.header.frame_id = "map";
  path1.header.stamp = ros::Time::now();
  path1.poses = result;
  path1.poses.emplace(path1.poses.begin(), nodes_[0]);
  res_pub_.publish(path1);
  */

  /*
  for (auto const& node : node_list) 
  {
    result.emplace_back(nodes_[node]);
  }*/

  //return result;
  return path_poses;
}

auto HalfStructPlanner::is_traffic_plan() -> bool {
  return !traffic_routes_.empty();
}

//现实线段
void HalfStructPlanner::show_traffic_route() 
{
  visualization_msgs::MarkerArray marker_array;
  int id = 0;
  int serial = 0;
  geometry_msgs::Point p1, p2;
  geometry_msgs::Pose p;
  auto make_arrow_marker = [&](double r, double g, double b, geometry_msgs::Point const& p1, geometry_msgs::Point const& p2) {
    visualization_msgs::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = ros::Time::now();
    marker.type = visualization_msgs::Marker::ARROW;
    marker.action = visualization_msgs::Marker::ADD;
    marker.scale.x = 0.08;
    marker.scale.y = 0.2;
    marker.scale.z = 0.4;
    marker.color.a = 0.5;
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.pose.orientation.w = 1.0;
    marker.points.resize(2);
    marker.ns = "arrow";
    marker.id = id++;
    marker.points[0] = p1;
    marker.points[1] = p2;
    marker_array.markers.emplace_back(marker);
  };

  auto make_text_marker = [&](double r, double g, double b, geometry_msgs::Pose const& p) {
    visualization_msgs::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = ros::Time::now();
    marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
    marker.action = visualization_msgs::Marker::ADD;
    marker.scale.z = 1.0;
    marker.color.a = 0.5;
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.pose = p;
    marker.ns = "text";
    marker.id = id++;
    marker.text = std::to_string(serial++);
    marker_array.markers.emplace_back(marker);
  };
  for (auto const& line : traffic_routes_) 
  {
    p1.x = line.a.x;
    p1.y = line.a.y;
    p2.x = line.b.x;
    p2.y = line.b.y;
    p.position.x = (p1.x + p2.x) / 2;
    p.position.y = (p1.y + p2.y) / 2;
    auto o = std::atan2(p2.y - p1.y, p2.x - p2.x);
    p.orientation.z = std::sin(o / 2);
    p.orientation.w = std::cos(o / 2);
    //make_arrow_marker(0.0, 1.0, 0.0, p1, p2);
    make_text_marker(0.0, 0.0, 0.0, p);
    //change zxp
    //if (line.go_list.empty()) return;
    //add zxp
    for(int i=0;i<line.edge.edge.size()-1;i++)
    {
      p1.x = line.edge.edge[i].pose.position.x;
      p1.y = line.edge.edge[i].pose.position.y;
      p2.x = line.edge.edge[i+1].pose.position.x;
      p2.y = line.edge.edge[i+1].pose.position.y;
       make_arrow_marker(0.0, 1.0, 0.0, p1, p2);
    }

     for (auto const& node : line.go_list) 
     {
      if (node.id >= traffic_routes_.size()) continue;

      geometry_msgs::Point k1,k2;
      for(int x=0;x<node.points.size()-1;x++)
      {
        k1.x = node.points[x].x;
        k1.y = node.points[x].y;
        k2.x = node.points[x+1].x;
        k2.y = node.points[x+1].y;
        make_arrow_marker(1.0, 0.0, 0.0, k1, k2);
      }
      //make_arrow_marker(0.0, 0.0, 1.0, p2, pg);
      //make_arrow_marker(0.0, 0.0, 1.0, pg, p1);
     }
    
  }
  vis_pub_.publish(marker_array);
}

//显示图
void HalfStructPlanner::show_graph() {
  visualization_msgs::MarkerArray marker_array;
  int id = 0;
  geometry_msgs::Point p1, p2;
  geometry_msgs::Pose p;
  p.orientation.w = 1.0;
  auto make_arrow_marker = [&](double r, double g, double b, geometry_msgs::Point const& p1, geometry_msgs::Point const& p2) {
    visualization_msgs::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = ros::Time::now();
    marker.type = visualization_msgs::Marker::ARROW;
    marker.action = visualization_msgs::Marker::ADD;
    marker.scale.x = 0.08;
    marker.scale.y = 0.2;
    marker.scale.z = 0.4;
    marker.color.a = 0.5;
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.pose.orientation.w = 1.0;
    marker.points.resize(2);
    marker.ns = "arrow";
    marker.id = id++;
    marker.points[0] = p1;
    marker.points[1] = p2;
    marker_array.markers.emplace_back(marker);
  };
  auto make_text_marker = [&](double r, double g, double b, geometry_msgs::Pose const& p, std::string const& t) {
    visualization_msgs::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = ros::Time();
    marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
    marker.action = visualization_msgs::Marker::ADD;
    marker.scale.z = 1.0;
    marker.color.a = 0.5;
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.pose = p;
    marker.ns = "text";
    marker.id = id++;
    marker.text = t;
    marker_array.markers.emplace_back(marker);
  };
  for (auto i = 0; i < node_num_; ++i) {
    p.position.x = nodes_[i].pose.position.x;
    p.position.y = nodes_[i].pose.position.y;
    make_text_marker(1.0, 0.0, 0.0, p, std::to_string(i));
    for (auto j = 0; j < node_num_; ++j) {
      if (graph_[i][j] == std::numeric_limits<double>::max()) continue;
      p1.x = nodes_[i].pose.position.x;
      p1.y = nodes_[i].pose.position.y;
      p2.x = nodes_[j].pose.position.x;
      p2.y = nodes_[j].pose.position.y;
      //make_arrow_marker(1.0, 0.0, 0.0, p1, p2);
    }
  }
  vis_pub_.publish(marker_array);
}

//创建图
void HalfStructPlanner::calculate_pre_graph() {
  if (traffic_routes_.empty()) {
    ROS_WARN("Receive empty traffic routes, planner not working!");
    return;
  }

  // 快速通道的起终点数目+真实起终点+带来的新节点
  node_num_ = static_cast<int>(traffic_routes_.size() * 2 + 2 + EXTRA_POINTS_NUM * 2);
  // 0: 起点 1: 终点
  // 2 - 1+EXTRA_POINTS_NUM: 起点带来的新节点
  // 2+EXTRA_POINTS_NUM - 1+2*EXTRA_POINTS_NUM: 终点带来的新节点
  // 2+2*EXTRA_POINTS_NUM - end: traffic routes节点 (本函数处理)
  nodes_.resize(node_num_);
  graph_ = new double *[node_num_];
  graph_bk_ = new double *[node_num_];
  for (auto i = 0; i < node_num_; ++i) {
    graph_[i] = new double[node_num_];
    graph_bk_[i] = new double[node_num_];
    for (auto j = 0; j < node_num_; ++j) graph_[i][j] = std::numeric_limits<double>::max();
    graph_[i][i] = 0.0;
  }
  // 处理traffic routes预设节点和边
  for (auto i = 0; i < traffic_routes_.size(); ++i) 
  {
    auto start = 2 + 2 * EXTRA_POINTS_NUM + 2 * i;
    auto end = 2 + 2 * EXTRA_POINTS_NUM + 2 * i + 1;
    auto o = std::atan2(traffic_routes_[i].b.y - traffic_routes_[i].a.y,traffic_routes_[i].b.x - traffic_routes_[i].a.x);
    geometry_msgs::PoseStamped p;
    p.header.frame_id = "map";
    p.pose.orientation.z = std::sin(o / 2);
    p.pose.orientation.w = std::cos(o / 2);
    p.pose.position.x = traffic_routes_[i].a.x;
    p.pose.position.y = traffic_routes_[i].a.y;
    nodes_[start] = p;
    p.pose.position.x = traffic_routes_[i].b.x;
    p.pose.position.y = traffic_routes_[i].b.y;
    nodes_[end] = p;
    // 所有traffic routes都是直线，如果支持任意曲线的话，需要计算曲线长度
    graph_[start][end] = traffic_routes_[i].edge.cost;//distance(nodes_[end], nodes_[start]);
    //add zxp 
     graph_[end][start] = traffic_routes_[i].edge.cost;//distance(nodes_[end], nodes_[start]);

    //add zxp 保存路径点
     paths_t path,path_r;
     path.start = start;
     path.end  = end;
     path.paths = traffic_routes_[i].edge.edge;
     all_paths.emplace_back(path);

    //全向图，反向保存
     path_r.start = end;
     path_r.end  = start;
     //path_r.paths = traffic_routes_[i].edge.edge;
     //反转数据
     for(auto const& ed : traffic_routes_[i].edge.edge)
     {
        path_r.paths.insert(path_r.paths.begin(),ed);
     }

     all_paths.emplace_back(path_r);

  }

  // 处理traffic routes终点能连接的起点和边
  for (auto i = 0; i < traffic_routes_.size(); ++i) 
  {
    auto end = 2 + 2 * EXTRA_POINTS_NUM + 2 * i + 1;
    for (auto const& node : traffic_routes_[i].go_list) 
    {
      if (node.id >= traffic_routes_.size()) 
      {
        ROS_ERROR("line %d go list %d out of range", i, node.id);
        continue;
      }
      auto start = 2 + 2 * EXTRA_POINTS_NUM + 2 * node.id;
      //轨迹相互相连，则终点连接起点。
      graph_[end][start] = distance(nodes_[end], nodes_[start]);
      //add zxp 
      graph_[start][end] =  distance(nodes_[end], nodes_[start]);


    std::vector<geometry_msgs::PoseStamped> path_points;
     geometry_msgs::PoseStamped path_point;
      //1 and  -1除去首位重复的点
      for(int j=1;j<node.points.size()-1;j++)
      {
        path_point.pose.position.x = node.points[j].x;
        path_point.pose.position.y = node.points[j].y;
        path_points.emplace_back(path_point);
      }

      //add zxp 保存路径点
      paths_t path,path_r;
      path.start = end;
      path.end  = start;
      path.paths = path_points;
      all_paths.emplace_back(path);

      //全向图，反向保存
      path_r.start = start;
      path_r.end  = end;
      //path_r.paths = path_points;
      //反转数据
      for(auto const& point : path_points)
      {
        path_r.paths.insert(path_r.paths.begin(),point);
      }
      all_paths.emplace_back(path_r);

    }
  }

  for (auto i = 0; i < node_num_; ++i) 
  {
    for (auto j = 0; j < node_num_; ++j) 
    {
      graph_bk_[i][j] = graph_[i][j];
    }
  }
  printf("all_paths:%d\n",all_paths.size());
  //print_graph();
  //show_graph();
}

//计算图
void HalfStructPlanner::calculate_graph() {
  struct setnode {
  point_t nearest_point;
  int line;
  int point_id;
  double nearest_dist;
  std::vector<geometry_msgs::PoseStamped> plan;

  //定义排序比较，返回小的
  bool operator<(setnode const& rhs) const {
    return nearest_dist < rhs.nearest_dist;
  }
  };

  //std::set 具有排序功能，begin为最小的
  std::set<setnode> start_set, goal_set,temp_start_set,temp_goal_set;
  auto start = point_t {nodes_[0].pose.position.x, nodes_[0].pose.position.y};
  auto goal = point_t {nodes_[1].pose.position.x, nodes_[1].pose.position.y};
  setnode sn {}, gn {},ssn{},ggn{};
 
  for (auto i = 0; i < traffic_routes_.size(); ++i) 
  {
    sn.line = i;
    sn.nearest_point = nearest_point_of_segment(start, traffic_routes_[i].edge,sn.point_id);

    //如果直线能通，则代价距离直接缩小10倍 优先走直线
    if(line_safe(sn.nearest_point, start))
    {
       sn.nearest_dist = distance(start, sn.nearest_point)/10;
    }
    else
    {
       sn.nearest_dist = distance(start, sn.nearest_point);
    }
    temp_start_set.insert(sn);
   
    gn.line = i;
    gn.nearest_point = nearest_point_of_segment(goal, traffic_routes_[i].edge,gn.point_id);

    //如果直线能通，则代价距离直接缩小10倍 优先走直线
    if(line_safe(gn.nearest_point, goal))
    {
        gn.nearest_dist = distance(goal, gn.nearest_point)/10;
    }
    else
    {
        gn.nearest_dist = distance(goal, gn.nearest_point);
    }
    temp_goal_set.insert(gn);
  }

  //----------------------------------rrt---------------------------------------------------------
  static unsigned char roll_cnt = 0;
  for(auto const& st:temp_start_set)
  {
    roll_cnt++;
    ssn.line = st.line;
    ssn.nearest_point = st.nearest_point;
    ssn.point_id = st.point_id;

    geometry_msgs::PoseStamped rrt_start,rrt_goal;
    rrt_start.pose.position.x = start.x;
    rrt_start.pose.position.y = start.y;
    rrt_goal.pose.position.x = ssn.nearest_point.x;
    rrt_goal.pose.position.y = ssn.nearest_point.y;
    //rrt inint
    astar_->initialize(costmap_);
    if(astar_->makePlan(rrt_start,rrt_goal,ssn.plan) == false )
    {
      //ROS_ERROR("astart planner is false");
      ssn.nearest_dist = std::numeric_limits<double>::max();
    }
    else
    {
      double dist = 0.0;
      for(int j=0;j<ssn.plan.size()-1;j++)
      {
        point_t point1,point2;
        point1.x = ssn.plan[j].pose.position.x;
        point1.y = ssn.plan[j].pose.position.y;
        point2.x = ssn.plan[j+1].pose.position.x;
        point2.y = ssn.plan[j+1].pose.position.y;
        dist+=  distance(point1,point2);
      }
      ssn.nearest_dist = dist;
      printf("dist:%.2f\n",ssn.nearest_dist);
    }
    start_set.insert(ssn);

    if(line_safe(st.nearest_point, start) || roll_cnt >1)
    {
      roll_cnt = 0;
      break;
    }
  }

  //
  for(auto const& st:temp_goal_set)
  {
    roll_cnt++;
    ggn.line = st.line;
    ggn.nearest_point = st.nearest_point;
    ggn.point_id = st.point_id;

    geometry_msgs::PoseStamped rrt_start,rrt_goal;
    rrt_start.pose.position.x = ggn.nearest_point.x;
    rrt_start.pose.position.y = ggn.nearest_point.y;
    rrt_goal.pose.position.x = goal.x;
    rrt_goal.pose.position.y = goal.y;
    //rrt inint
    astar_->initialize(costmap_);
    if(astar_->makePlan(rrt_start,rrt_goal,ggn.plan) == false )
    {
      //ROS_ERROR("astart planner is false");
      ggn.nearest_dist = std::numeric_limits<double>::max();
    }
    else
    {
      double dist = 0.0;
      for(int j=0;j<ggn.plan.size()-1;j++)
      {
        point_t point1,point2;
        point1.x = ggn.plan[j].pose.position.x;
        point1.y = ggn.plan[j].pose.position.y;
        point2.x = ggn.plan[j+1].pose.position.x;
        point2.y = ggn.plan[j+1].pose.position.y;
        dist+=  distance(point1,point2);
      }
      ggn.nearest_dist = dist;
      printf("dist:%.2f\n",ggn.nearest_dist);
    }
    goal_set.insert(ggn);

    if(line_safe(st.nearest_point, goal) || roll_cnt >1)
    {
      roll_cnt = 0;
      break;
    }
  }
  //----------------------------------------------------------------------------------------------
 //取出排序好的数据
  path_.near_start.nearest_point = start_set.begin()->nearest_point;
  path_.near_start.line = start_set.begin()->line;
  path_.near_start.point_id = start_set.begin()->point_id;
  path_.near_start.points = start_set.begin()->plan;

  path_.near_goal.nearest_point = goal_set.begin()->nearest_point;
  path_.near_goal.line = goal_set.begin()->line;
  path_.near_goal.point_id = goal_set.begin()->point_id;
  path_.near_goal.points = goal_set.begin()->plan;

  int line_s, line_g;
  geometry_msgs::PoseStamped p;
  p.header.frame_id = "map";
  for (auto i = 0; i < EXTRA_POINTS_NUM; ++i) {
    if (i < start_set.size()) {
      auto sen = 2 + i;
      p.pose.position.x = std::next(start_set.begin(), i)->nearest_point.x;
      p.pose.position.y = std::next(start_set.begin(), i)->nearest_point.y;
      line_g = 2 + 2 * EXTRA_POINTS_NUM + 2 * std::next(start_set.begin(), i)->line + 1;
      p.pose.orientation = nodes_[line_g].pose.orientation;
      if (i < 1 || std::next(start_set.begin(), i)->nearest_dist < EXTRA_POINTS_RANGE) {
        nodes_[sen] = p;
        graph_[0][sen] = std::next(start_set.begin(), i)->nearest_dist;
        graph_[sen][line_g] = distance(nodes_[sen], nodes_[line_g]);
        //add zxp 为修改为全向图
        graph_[sen][0] = std::next(start_set.begin(), i)->nearest_dist;
        graph_[line_g][sen] = distance(nodes_[sen], nodes_[line_g]);
        graph_[sen][line_g-1] = distance(nodes_[sen], nodes_[line_g-1]);
        graph_[line_g-1][sen] = distance(nodes_[sen], nodes_[line_g-1]);
      }
    }

    if (i < goal_set.size()) {
      auto gen = 2 + EXTRA_POINTS_NUM + i;
      p.pose.position.x = std::next(goal_set.begin(), i)->nearest_point.x;
      p.pose.position.y = std::next(goal_set.begin(), i)->nearest_point.y;
      line_s = 2 + 2 * EXTRA_POINTS_NUM + 2 * std::next(goal_set.begin(), i)->line;
      p.pose.orientation = nodes_[line_s].pose.orientation;
      if (i < 1 || std::next(goal_set.begin(), i)->nearest_dist < EXTRA_POINTS_RANGE) {
        nodes_[gen] = p;
        graph_[gen][1] = std::next(goal_set.begin(), i)->nearest_dist;
        graph_[line_s][gen] = distance(nodes_[line_s], nodes_[gen]);
        //add zxp 为修改为全向图
        graph_[1][gen] = std::next(goal_set.begin(), i)->nearest_dist;
        graph_[gen][line_s] = distance(nodes_[line_s], nodes_[gen]);
        graph_[line_s+1][gen] = distance(nodes_[line_s+1], nodes_[gen]);
        graph_[gen][line_s+1] = distance(nodes_[line_s+1], nodes_[gen]);
      }
    }
  }

  // 更新起点带来的新点到终点带来的新点的代价，解决起终点在同一条line的情况
  for (auto i = 0; i < EXTRA_POINTS_NUM; ++i) {
    if (i < start_set.size()) {
      auto sen = 2 + i;
      line_s = 2 + 2 * EXTRA_POINTS_NUM + 2 * std::next(start_set.begin(), i)->line;
      line_g = line_s + 1;

      for (auto j = 0; j < EXTRA_POINTS_NUM; ++j) {
        if (j < goal_set.size()) {
          auto gen = 2 + EXTRA_POINTS_NUM + j;
          //change zxp 为修改为全向图
          if (std::next(goal_set.begin(), j)->line == std::next(start_set.begin(), i)->line
              /*&& distance(nodes_[gen], nodes_[line_g]) < distance(nodes_[sen], nodes_[line_g])*/) 
          {
            graph_[sen][gen] = distance(nodes_[sen], nodes_[gen]);
            //add zxp  为修改为全向图
            graph_[gen][sen] = distance(nodes_[sen], nodes_[gen]);

            //id add  取在同一条线上的点
            path_.paths.clear();
            if(path_.near_goal.point_id > path_.near_start.point_id) //id add
            {
              for(int n = path_.near_start.point_id;n<=path_.near_goal.point_id;n++)
              {
                path_.paths.emplace_back( traffic_routes_[path_.near_start.line].edge.edge[n]);
              }
            }
            else //id cut
            {
              for(int n = path_.near_start.point_id;n>=path_.near_goal.point_id;n--)
              {
                path_.paths.emplace_back( traffic_routes_[path_.near_start.line].edge.edge[n]);
              }
            }
          }
        }
      }
    }
  }

  //print_graph();
  show_graph();
}

//从轨迹中选出最近的点
auto HalfStructPlanner::nearest_point_of_segment(point_t const& p, graph_t const& edge, int &min_id) -> point_t 
{
  point_t edge_point,result;
  double temp_dis = 100.0;
  int tem_min_i = 0;
  for(int i=0;i< edge.edge.size();i++)
  {
    edge_point.x = edge.edge[i].pose.position.x;
    edge_point.y = edge.edge[i].pose.position.y;
    double dis = distance(p,edge_point);
    if(temp_dis > dis)
    {
      temp_dis = dis;
      tem_min_i = i;
    }
  }
  edge_point.x = edge.edge[tem_min_i].pose.position.x;
  edge_point.y = edge.edge[tem_min_i].pose.position.y;
  result = edge_point;
  min_id = tem_min_i;
  return result;
}


auto HalfStructPlanner::distance(point_t const& a, point_t const& b) -> double {
  return std::hypot(a.x - b.x, a.y - b.y);
}

//打印图
void HalfStructPlanner::print_graph() {
  // 打印代价矩阵
  std::cout << "node num: " << node_num_ << std::endl;
  for (auto i = 0; i < node_num_; ++i) std::cout << GREEN << std::setw(3) << i << " " << WHITE;
  std::cout << "\n";
  for (auto i = 0; i < node_num_; ++i) {
    for (auto j = 0; j < node_num_; ++j) {
      if (graph_[i][j] != std::numeric_limits<double>::max()) {
        std::string str {"000 "};
        auto gij = std::to_string(graph_[i][j]);
        for (auto k = 0; k < 3; ++k) {
          if (gij.size() > k) str[k] = gij[k];
        }
        std::cout << str.c_str();
      } else {
        std::cout << "*** ";
      }
    }
    std::cout << GREEN << " " << i << "\n" << WHITE;
  }
}

//利用代价地图计算线路是否安全
auto HalfStructPlanner::line_safe(point_t const& a, point_t const& b) -> bool {
  if (!costmap_) return true;

  auto num = static_cast<int>(distance(a, b) / 0.05);
  uint32_t mx, my;
  double x, y;
  for (auto i = 0; i < num; ++i) {
    x = a.x + i * (b.x - a.x) / num;
    y = a.y + i * (b.y - a.y) / num;
    if (!costmap_->worldToMap(x, y, mx, my)) return false;
    if (costmap_->getCost(mx, my) >= 66) return false;
  }

  return true;
}

//查找最短路径D*
auto HalfStructPlanner::dijkstra() -> std::vector<int> {
  std::vector<int> result;

  struct dijnode {
  int node;
  int prev_node;
  double cost;

  bool operator<(dijnode const& rhs) const {
    return cost < rhs.cost;
  }

  bool operator==(dijnode const& rhs) const {
    return node == rhs.node;
  }
  };

  // 从graph_找到0-1的路径
  std::set<dijnode> open_list;
  std::vector<dijnode> close_list;
  open_list.insert(dijnode{0, 0, 0.0});
  while (!open_list.empty()) {
    auto node = open_list.extract(open_list.begin()).value();
    if (node.node == 1) {
      while (node.prev_node != 0) {
        result.emplace_back(node.node);
        auto prev = std::find_if(close_list.begin(), close_list.end(),
                                 [&](dijnode const& a) { return a.node == node.prev_node; });
        if (prev == close_list.end()) {
          ROS_ERROR("No whole path while graph search success, that should not happen!");
          break;
        }
        node = *prev;
      }
      result.emplace_back(node.node);
      //result.emplace_back(node.prev_node); // 不需要特意走到起点
      std::reverse(result.begin(), result.end());
      std::cout << RED << "0";
      for (auto const& r : result) std::cout << RED << "->" << r;
      std::cout << WHITE << std::endl;
      break;
    }

    close_list.emplace_back(node);

    for (auto i = 0; i < node_num_; ++i)
     {
      if (i == node.node) 
      {
         //ROS_WARN("continue:1");
        continue;
      }
      //change zxp
      if (std::find_if(close_list.begin(), close_list.end(),[&](dijnode const& a) { return a.node == i; }) != close_list.end()) 
      {
        //ROS_WARN("continue:2");
        continue;
      }
      if (graph_[node.node][i] == std::numeric_limits<double>::max())
      {
        //ROS_WARN("continue:3");
        continue;
      } 

      dijnode open_node {i, node.node, node.cost + graph_[node.node][i]};
      auto iter2 = std::find(open_list.begin(), open_list.end(), open_node);
      if (iter2 != open_list.end()) {
        if (iter2->cost > open_node.cost) {
          open_list.erase(iter2);
          open_list.insert(open_node);
        }
      } else {
        open_list.insert(open_node);
      }
    }
  }
  return result;
}

//画贝塞尔二阶曲线
//p0 start
//p1 control
//p2 end
auto HalfStructPlanner::calculateSecond(point_t const& p0,point_t const& p1,point_t const& p2,int point_num)  -> std::vector<point_t>
{
  double t = 0;
  std::vector<point_t> path_points;
  point_t  path_point;
  double delta_t = 1.0 / point_num;
  double f1, f2, f3;
  for (double i = 0; i <= point_num; ++i)
   {
    t = delta_t * i;
    f1 = (1 - t) * (1 - t);
    f2 = 2 * (1 - t) * t;
    f3 = t * t;
    path_point.x = f1 * p0.x +f2 * p1.x + f3 * p2.x;
    path_point.y =  f1 * p0.y + f2 * p1.y + f3 * p2.y;
    path_points.emplace_back(path_point);
  }
  return path_points;
}

//求两直线的交点
//point1 point2 
//point3 point4
auto HalfStructPlanner::getIntersectPoint(point_t point1,point_t point2,point_t point3,point_t point4) -> point_t
{
  point_t point;
  double a1 = point1.y - point2.y, b1 = point2.x - point1.x, c1 = point1.x * point2.y - point2.x * point1.y;
  double a2 = point3.y - point4.y, b2 = point4.x - point3.x, c2 = point3.x * point4.y - point4.x * point3.y;
  point.x = (c1*b2-c2*b1)/(a2*b1-a1*b2);
  point.y = (a2*c1-a1*c2)/(a1*b2-a2*b1);

  double k1=(point2.y - point1.y) / (point2.x - point1.x);
  double k2=(point4.y - point3.y) / (point4.x - point3.x);
  //两条直线夹角 小于0.5
  double yaw = atan2((k2-k1),(1+k1*k2));
  if(abs(yaw) < 0.5)
  {
    point.x = (point2.x + point3.x)/2.0;
    point.y = (point2.y + point3.y)/2.0;
  }
  return point;
}

}
