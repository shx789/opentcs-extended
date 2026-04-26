//
// Created by tony on 2022/12/2.
//

#pragma once

#include <costmap_2d/costmap_2d.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>
#include <ros/ros.h>
#include <visualization_msgs/MarkerArray.h>
#include <tf/transform_listener.h>
#include<geometry_msgs/Quaternion.h>
//#include "astar_planner.h"
#include <geometry_msgs/PoseArray.h>
#include <rrt_star_ros.h>


namespace xju::pnc {
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define WHITE   "\033[37m"      /* White */

typedef struct point
 {
  double x;
  double y;
  bool operator==(point const& rhs) const {
  return std::abs(x - rhs.x) < 1e-6 && std::abs(y - rhs.y) < 1e-6;
}
} point_t;

typedef struct graph
 {
  double cost = std::numeric_limits<double>::max();
  std::vector<geometry_msgs::PoseStamped> edge{};
} graph_t; // 没有用这个结构体实现，因为边都是直线，因此简单替换成长度了，如果支持复杂通道，需要把路径作为边存下来

typedef struct golist
{
  int id;
   std::vector<point_t> points;
} go_list_t; 

typedef struct line {
point_t a; //起点
point_t b; //终点
graph_t edge; //边
std::vector<go_list_t> go_list ;
} line_t;

using lines_type = std::vector<line_t>;

typedef struct nearpoint 
{
  point_t nearest_point; //相邻点
  int line;                                                         //线ID
  int point_id;                                                                    //相邻点id
   std::vector<geometry_msgs::PoseStamped> points; //点集
} nearpoint_t;

typedef struct path
{
  geometry_msgs::PoseStamped  start; //启动点
  nearpoint_t near_start;                              //启动相邻点
  nearpoint_t near_goal;                               //目标相邻点
  geometry_msgs::PoseStamped goal;   //目标点
  std::vector<geometry_msgs::PoseStamped> paths;  //路径点
}path_t; 

typedef struct paths
{
  int  start; //启点
  int end;   //终点
  std::vector<geometry_msgs::PoseStamped> paths;  //路径点
}paths_t; 



constexpr static const int EXTRA_POINTS_NUM = 1;//3;
constexpr static const double EXTRA_POINTS_RANGE = 5.0;

class HalfStructPlanner {
public:
  HalfStructPlanner();

  ~HalfStructPlanner();

  void init();

  void set_costmap(std::shared_ptr<costmap_2d::Costmap2D> const& costmap) {
    costmap_ = costmap;
  }

  void set_traffic_route(lines_type const& lines);

//  auto get_path(geometry_msgs::PoseStamped const& start, geometry_msgs::PoseStamped const& goal) -> nav_msgs::Path;
  auto get_path(geometry_msgs::PoseStamped const& start,
                geometry_msgs::PoseStamped const& goal) -> std::vector<geometry_msgs::PoseStamped>; // 将返回的点序用goto连接，不直接返回路径，不在这个类做地图路径搜索

  auto is_traffic_plan() -> bool;

private:
  void show_traffic_route();

  void show_graph();

  void calculate_pre_graph();

  void calculate_graph();

  //auto nearest_point_of_segment(point_t const& p, point_t const& a, point_t const& b) -> point_t;
  auto nearest_point_of_segment(point_t const& p, graph_t const& edge,int &min_id) -> point_t ;

  auto distance(point_t const& a, point_t const& b) -> double;

  auto distance(geometry_msgs::PoseStamped const& a, geometry_msgs::PoseStamped const& b) -> double {
    auto pa = point_t {a.pose.position.x, a.pose.position.y};
    auto pb = point_t {b.pose.position.x, b.pose.position.y};
    return distance(pa, pb);
  }

  void print_graph();

  auto line_safe(point_t const& a, point_t const& b) -> bool;

  auto line_safe(geometry_msgs::PoseStamped const& a, geometry_msgs::PoseStamped const& b) -> bool {
    auto pa = point_t {a.pose.position.x, a.pose.position.y};
    auto pb = point_t {b.pose.position.x, b.pose.position.y};
    return line_safe(pa, pb);
  }

  auto dijkstra() -> std::vector<int>;

 auto calculateSecond(point_t const& p0,point_t const& p1,point_t const& p2,int point_num)  -> std::vector<point_t>;
 auto getIntersectPoint(point_t point1,point_t point2,point_t point3,point_t point4) -> point_t;

private:
  lines_type traffic_routes_;
  path_t path_;
   std::vector<paths_t> all_paths;

  ros::Publisher vis_pub_;
  ros::Publisher res_pub_;
  //ros::Publisher path_pub_;

//  graph_type **graph_;
  double** graph_;
  double** graph_bk_;
  std::vector<geometry_msgs::PoseStamped> nodes_;

  std::shared_ptr<costmap_2d::Costmap2D> costmap_;
  std::shared_ptr<RRTstar_planner::RRTstarPlannerROS> astar_;

  int node_num_;
};
}
