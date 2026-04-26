#include "apriltag_tracker.h"


// 初始化设置
Apriltag_tracker::Apriltag_tracker()
{
  //获得主机名字
  struct passwd* pwd;
  uid_t userid;
  userid = getuid();
  pwd = getpwuid(userid);
  std:string hostname = pwd -> pw_name;
  
  ros::NodeHandle n;
  n.param<std::string>("tag_save_filename", this -> tag_save_filename, 
                       std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/ros_qr_location/apriltag_tracker/json/tag_save.json"));
  n.param<std::string>("noise_fit_filename", this -> noise_fit_filename, 
                       std::string("/home/" + hostname + "/catkin_ws/src/ros_nav/ros_qr_location/apriltag_tracker/json/noise_fit.json"));

  this -> tag_sub = n.subscribe("/apriltag_process/tag_pose", 10, &Apriltag_tracker::Tag_callback, this);
  this -> tag_pub = n.advertise<geometry_msgs::Point>("/apriltag_tracker/tag_pose", 10);
  this -> cmd_vel_pub = n.advertise<geometry_msgs::Twist>("/cmd_vel", 10);
  this -> set_id_client = n.serviceClient<apriltag_tracker::set_id>("/apriltag_process/set_id");
  this -> tag_save_service = n.advertiseService("/apriltag_tracker/tag_save", &Apriltag_tracker::Tag_save_service, this);
  this -> noise_fit_service = n.advertiseService("/apriltag_tracker/noise_fit", &Apriltag_tracker::Noise_fit_service, this);
  this -> custom_shift_service = n.advertiseService("/apriltag_tracker/custom_shift", &Apriltag_tracker::Custom_shift_service, this);

  tracker_as_ = new Server(n, "/apriltag_tracker/tag_track", boost::bind(&Apriltag_tracker::Apriltag_tracker_actioncb, this, _1), false);
  tracker_as_ -> start();

  Read_target_pose();
  save_list = load_list;
}


// 接收搜索目标二维码状态
void Apriltag_tracker::Tag_callback(const apriltag_tracker::tag::ConstPtr & tag_msg)
{
  double time_diff = fabs((tag_msg -> header.stamp - ros::Time::now()).toSec());
  if(time_diff < 0.5)
  {
    tag.id = tag_msg -> id;
    // tag.pose = tag_msg -> pose.pose;
    tag.pose = Tf_sub("base_footprint", "tag_link");
  }
  else tag.id = -1;
}


// 接收TF变换
geometry_msgs::Pose Apriltag_tracker::Tf_sub(std::string parent_frame, std::string child_frame)
{
  try
  {
    static tf::TransformListener listener;
    static tf::StampedTransform transform;
    listener.waitForTransform(parent_frame, child_frame, ros::Time(0), ros::Duration(1));
    listener.lookupTransform(parent_frame, child_frame, ros::Time(0), transform);
    geometry_msgs::Pose pose;
    pose.position.x = transform.getOrigin().x();
    pose.position.y = transform.getOrigin().y();
    pose.position.z = transform.getOrigin().z();
    pose.orientation.x = transform.getRotation().getX();
    pose.orientation.y = transform.getRotation().getY();
    pose.orientation.z = transform.getRotation().getZ();
    pose.orientation.w = transform.getRotation().getW();
    return pose;
  }
  catch(std::exception e)
  {
    ROS_WARN("Can not find the frame");
    geometry_msgs::Pose null;
    return null;
  }
}


//发布目标tf
void Apriltag_tracker::Tf_pub(geometry_msgs::Pose pose, std::string parent_frame, std::string child_frame)
{
  static  tf::Transform transform;
  static tf::TransformBroadcaster tran2tar;
  transform = tf::Transform(tf::Quaternion(pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w), 
                            tf::Vector3(pose.position.x, pose.position.y, pose.position.z));
  tran2tar.sendTransform(tf::StampedTransform(transform, ros::Time::now(), parent_frame, child_frame));
}


//四元数转欧拉角
geometry_msgs::Point Apriltag_tracker::Get_RPY(geometry_msgs::Quaternion msg)
{
  tf::Quaternion quat;
  tf::quaternionMsgToTF(msg, quat);
  quat.normalize();
  double roll, pitch, yaw;
  tf::Matrix3x3(quat).getRPY(roll, pitch, yaw);
  geometry_msgs::Point RPY;
  RPY.x = roll;
  RPY.y = pitch;
  RPY.z = yaw;
  return RPY;
}


//数值映射
double Apriltag_tracker::Remap(double src, double src_min, double src_max, double dst_min, double dst_max)
{
  double dst = (dst_max - dst_min) / (src_max - src_min) * (src - src_min) + dst_min;
  return dst;
}


//最小二乘法
geometry_msgs::Point Apriltag_tracker::leastSquares(vector<double> x, vector<double> y)
{
  geometry_msgs::Point result;

  if (x.size() != y.size() || x.empty()) 
  {
    ROS_WARN("Error: Input vectors must be of the same size and not empty.");
    return result;
  }

  double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXX = 0.0;
  int n = x.size();

  // 计算各项和
  for (int i = 0; i < n; ++i) 
  {
    sumX += x[i];
    sumY += y[i];
    sumXY += x[i] * y[i];
    sumXX += x[i] * x[i];
  }

  // 计算斜率a和截距b
  double denominator = n * sumXX - sumX * sumX;
  if (fabs(denominator) < 1e-10) 
  {
    ROS_WARN("Error: Denominator is zero, cannot solve the equation.");
    return result;
  }

  result.x = (n * sumXY - sumX * sumY) / denominator;
  result.y = (sumY * sumXX - sumX * sumXY) / denominator;
  return result;
}


//发布速度
void Apriltag_tracker::Cmd_vel_pub(double v, double w)
{
  geometry_msgs::Twist control_vel;
  control_vel.linear.x = v;
  control_vel.angular.z = w;
  cmd_vel_pub.publish(control_vel);
}


//计算变换
geometry_msgs::Point Apriltag_tracker::Get_tran(tag_data base_tag, tag_data tran_tag)
{
  double base_pitch = Get_RPY(base_tag.pose.orientation).z;
  double tran_pitch = Get_RPY(tran_tag.pose.orientation).z;

  geometry_msgs::Point result;
  result.x = base_tag.pose.position.x - tran_tag.pose.position.x;
  result.y = base_tag.pose.position.y - tran_tag.pose.position.y;
  result.z = base_pitch - tran_pitch;
  return result;
}


//追踪控制
bool Apriltag_tracker::Pure_pursuit_control(geometry_msgs::Point target_pose, geometry_msgs::Point prev_pose)
{
  // 极坐标转换
  double rho = sqrt(std::pow(target_pose.x, 2) + std::pow(target_pose.y, 2));

  // 误差计算
  double dx = target_pose.x + (rho * noise_x_k + noise_x_b) - custom_x_b;
  double dy = target_pose.y + (rho * (noise_y_k - custom_y_k) + noise_y_b);
  double dtheta = target_pose.z;
  double gamma = std::atan(dy/dx);

  // std::cout << "dx:" << dx << std::endl;
  // std::cout << "dy:" << dy << std::endl;
  // std::cout << "dtheta:" << dtheta << std::endl;
  // std::cout << "gamma:"  << gamma << std::endl;

  if(step_1 && step_2)
  {
    if(sqrt(std::pow(dx, 2) + std::pow(dy, 2)) > 0.45)
    {
      Cmd_vel_pub(0.05, dtheta * 0.1);
      return false;
    }
    else
    {
      bias_gamma = std::atan(dy / dx);
      bias_dtheta = dtheta;
      max_x = dx;
      if(std::abs(bias_gamma) < std::atan(0.020 / 0.45)) 
      {
        step_2 = false;
        return false;
      }
      else
      {
        bias_odom = Get_RPY(Tf_sub("odom", "base_footprint").orientation).z;
        step_1 = false;
        return false;
      }
    }
  }

  else if(!step_1 && step_2)
  {
    // std::cout << "bias_gamma:"  << bias_gamma << std::endl;
    double err = std::abs(bias_odom - Get_RPY(Tf_sub("odom", "base_footprint").orientation).z);
    if(err > std::abs(bias_gamma)) step_2 = false;
    Cmd_vel_pub(0, 0.03 * bias_gamma / std::abs(bias_gamma));
    prev_pose.z = gamma;
    return false;
  }

  else if(step_1 && !step_2)
  {
    double w = 0;
    double v = 0.10 * dx;
    if(dx <= 0.005) return true;
    w = Remap(dx, max_x, 0, 0.8, 0) * gamma + Remap(dx, max_x, 0, 0, 0.2) * dtheta;
    if(v < 0.01) v = 0.01; if(v > 0.10) v = 0.10;
    Cmd_vel_pub(v, w);
    prev_pose.z = gamma;
    return false;
  }

  else
  {
    double w = 0;
    double v = 0.10 * dx;
    if(dx <= 0.005) return true;
    if(rho > 0.20) w = (2 * v * dy) / std::pow(rho, 2);
    else w = Remap(dx, 0.20, 0.005, 0.8, 0) * gamma + Remap(dx, 0.20, 0.005, 0, 0.2) * dtheta;
    if(v < 0.01) v = 0.01; if(v > 0.10) v = 0.10;
    Cmd_vel_pub(v, w);
    prev_pose.z = gamma;
    return false;
  }
}


// 储存噪音拟合
void Apriltag_tracker::Save_noise_fit(double x_k, double x_b, double y_k, double y_b, double custom_x, double custom_y)
{
  Json::Value root;
  Json::StyledWriter swriter;
  std::ofstream ofs;
  std::string str;
  root["x_k"] = Json::Value(x_k);
  root["x_b"] = Json::Value(x_b);
  root["y_k"] = Json::Value(y_k);
  root["y_b"] = Json::Value(y_b);
  root["custom_x"] = Json::Value(custom_x);
  root["custom_y"] = Json::Value(custom_y);
  str = swriter.write(root);
  ofs.open(noise_fit_filename);
  ofs << str;
  ofs.close();
}


// 读取噪音拟合
void Apriltag_tracker::Read_noise_fit()
{
  Json::Reader reader;
  Json::Value root;
  std::ifstream ifs(noise_fit_filename.c_str(), std::ifstream::in);
  if(ifs.is_open()) ROS_WARN("Noise file is already open");
  if(reader.parse(ifs, root))
  {
    noise_x_k = root["x_k"].asDouble();
    noise_x_b = root["x_b"].asDouble();
    noise_y_k = root["y_k"].asDouble();
    noise_y_b = root["y_b"].asDouble();
    custom_x_b = root["custom_x"].asDouble();
    custom_y_k = root["custom_y"].asDouble();
  }
  else ROS_WARN("File open failure");
  ifs.close();
}


// 储存目标列表
void Apriltag_tracker::Save_target_pose()
{
  Json::Value root;
  Json::StyledWriter swriter;
  std::ofstream ofs;
  std::string str;
  // 打印点位列表
  for(int i = 0; i < save_list.size(); i++)
  {
    Json::Value id_data;
    id_data["id"] = Json::Value(save_list[i].id);
    id_data["position.x"] = Json::Value(save_list[i].pose.position.x);
    id_data["position.y"] = Json::Value(save_list[i].pose.position.y);
    id_data["position.z"] = Json::Value(save_list[i].pose.position.z);
    id_data["orientation.x"] = Json::Value(save_list[i].pose.orientation.x);
    id_data["orientation.y"] = Json::Value(save_list[i].pose.orientation.y);
    id_data["orientation.z"] = Json::Value(save_list[i].pose.orientation.z);
    id_data["orientation.w"] = Json::Value(save_list[i].pose.orientation.w);
    root["pose_list"].append(id_data);
  }
  str = swriter.write(root);
  ofs.open(tag_save_filename);
  ofs << str;
  ofs.close();
}


// 读取目标列表
void Apriltag_tracker::Read_target_pose()
{
  Json::Reader reader;
  Json::Value root;
  std::ifstream ifs(tag_save_filename.c_str(), std::ifstream::in);
  if(ifs.is_open()) ROS_WARN("Pose file is already open");
  if(reader.parse(ifs, root))
  {
    load_list.clear();
    for(int i = 0; i < root["pose_list"].size(); i++)
    {
      tag_data pose;
      pose.id = root["pose_list"][i]["id"].asInt();
      pose.pose.position.x = root["pose_list"][i]["position.x"].asDouble();
      pose.pose.position.y = root["pose_list"][i]["position.y"].asDouble();
      pose.pose.position.z = root["pose_list"][i]["position.z"].asDouble();
      pose.pose.orientation.x = root["pose_list"][i]["orientation.x"].asDouble();
      pose.pose.orientation.y = root["pose_list"][i]["orientation.y"].asDouble();
      pose.pose.orientation.z = root["pose_list"][i]["orientation.z"].asDouble();
      pose.pose.orientation.w = root["pose_list"][i]["orientation.w"].asDouble();
      load_list.push_back(pose);
    }
  }
  else ROS_WARN("File open failure");
  ifs.close();
}


//搜索列表中的id
int Apriltag_tracker::Search_id_list(int id, std::vector<tag_data> tag_list)
{
  //搜索相同id
  std::vector<int> id_list;
  for(int i = 0; i < tag_list.size(); i++)
  {
    if(tag_list[i].id == id) id_list.push_back(i);
  }
  if(id_list.size() > 0) //出现相同id时仅保留最后一个
  {
    for(int j =  0; j < id_list.size() - 1; j++)
    {
      id_list.erase(id_list.begin() + id_list[j]);
    }
    return id_list[0]; //输出搜索id在列表中的位置
  }
  else return -1; //未搜索到id返回-1
}


//搜索列表中的id
int Apriltag_tracker::Search_num_list(int num, std::vector<tag_data> tag_list)
{
  //搜索相同id序号
  if(num <= tag_list.size() - 1) return num; //输出搜索id在列表中的位置
  else return -1; //未搜索到id返回-1
}


//启用二维码位姿保存
bool Apriltag_tracker::Tag_save_service(apriltag_tracker::save_tag::Request &req, apriltag_tracker::save_tag::Response &res)
{
  apriltag_tracker::set_id set_id_srv;
  set_id_srv.request.id = 0;
  if(set_id_client.call(set_id_srv))
  {	
    sleep(1);
    if(tag.id >= 0)
    {
      Read_target_pose();
      int if_same = Search_num_list(req.id, save_list);
      if(if_same >= 0) save_list[if_same] = tag;
      else save_list.push_back(tag);
      Save_target_pose();
      res.result = true;
    }
    else
    {
      res.result = false;
      ROS_INFO("Failed to save");
    }
  }
  else 
  {
    res.result = false;
    ROS_INFO("Failed to call service");
  }
  return true;
}


//启用定位噪音拟合
bool Apriltag_tracker::Noise_fit_service(apriltag_tracker::noise_fit::Request &req, apriltag_tracker::noise_fit::Response &res)
{
  ros::spinOnce();
  ros::Rate loop_rate(100);

  apriltag_tracker::set_id set_id_srv;
  set_id_srv.request.id = req.id;
  if(set_id_client.call(set_id_srv))
  {
    sleep(1);
    geometry_msgs::Point start_pose;
    start_pose.x = Tf_sub("odom", "base_footprint").position.x;
    start_pose.y = Tf_sub("odom", "base_footprint").position.y;
    start_pose.z = Get_RPY(Tf_sub("odom", "base_footprint").orientation).z;
    vector<double> rho_odom, x_bias, y_bias;
    tag_data start_tag = tag;
    double rho = 0.0;
    while(rho < 0.5)
    {
      ros::spinOnce();
      geometry_msgs::Point end_pose;
      end_pose.x = Tf_sub("odom", "base_footprint").position.x;
      end_pose.y = Tf_sub("odom", "base_footprint").position.y;
      end_pose.z = Get_RPY(Tf_sub("odom", "base_footprint").orientation).z;
      rho = sqrt(std::pow(start_pose.x - end_pose.x, 2) + std::pow(start_pose.y - end_pose.y, 2));
      tag_data end_tag = tag;
      if(!req.show)
      {
        double dx =  rho * std::cos(end_pose.z - start_pose.z) - (end_tag.pose.position.x - start_tag.pose.position.x);
        double dy = -rho * std::sin(end_pose.z - start_pose.z) - (end_tag.pose.position.y - start_tag.pose.position.y);
        double dr = sqrt(std::pow(end_tag.pose.position.x - start_tag.pose.position.x, 2) 
                       + std::pow(end_tag.pose.position.y - start_tag.pose.position.y, 2));
        x_bias.push_back(dx);
        y_bias.push_back(dy);
        rho_odom.push_back(dr);
        if(rho > 0.5)
        {
          geometry_msgs::Point result_x, result_y;
          result_x = leastSquares(rho_odom, x_bias);
          result_y = leastSquares(rho_odom, y_bias);
          Save_noise_fit(result_x.x, result_x.y, result_y.x, result_y.y, custom_x_b, custom_y_k);
          break;
        }
        Cmd_vel_pub(-0.05, 0);
      }
      else
      {
        Read_noise_fit();
        double dr = sqrt(std::pow(end_tag.pose.position.x - start_tag.pose.position.x, 2) 
                       + std::pow(end_tag.pose.position.y - start_tag.pose.position.y, 2));
        double dx = (end_tag.pose.position.x - start_tag.pose.position.x) + (dr * noise_x_k + noise_x_b) - custom_x_b;
        double dy = (end_tag.pose.position.y - start_tag.pose.position.y) + (dr * (noise_y_k - custom_y_k) + noise_y_b);
        dx =  rho * std::cos(end_pose.z - start_pose.z) - dx;
        dy = -rho * std::sin(end_pose.z - start_pose.z) - dy;
        ROS_WARN("dx: %f, dy; %f", dx, dy);
        if(rho > 0.7) break;
      }
      loop_rate.sleep();
    }
  }
  else 
  {
    res.result = false;
    ROS_INFO("Failed to call service");
  }
  return true;
}


//启用二维码偏移保存
bool Apriltag_tracker::Custom_shift_service(apriltag_tracker::custom_shift::Request &req, apriltag_tracker::custom_shift::Response &res)
{
  Read_noise_fit();
  custom_x_b = req.custom_x;
  custom_y_k = req.custom_y;
  Save_noise_fit(noise_x_k, noise_x_b, noise_y_k, noise_y_b, custom_x_b, custom_y_k);
  return true;
}


// 服务端结果发布
void Apriltag_tracker::Apriltag_tracker_action_result_publish(int step_flag, int error_flag, int id)
{
  if (tracker_as_ -> isActive())
  {
    tracker_ac_result.id = id;
    tracker_ac_result.result = step_flag;
    tracker_ac_result.error = error_flag;
    tracker_as_ -> setPreempted(tracker_ac_result);
  }
}


// 反馈追踪服务进程
void Apriltag_tracker::Apriltag_tracker_action_feedback_publish(int step_flag, int id)
{
  tracker_ac_feed.id = id;
  tracker_ac_feed.step = step_flag;
  tracker_as_ -> publishFeedback(tracker_ac_feed);
}


// 服务端任务执行
bool Apriltag_tracker::Apriltag_tracker_actioncb(const apriltag_tracker::action1GoalConstPtr &goal)
{
  ros::spinOnce();
  ros::Rate loop_rate(100);
  
  if(goal -> start)
  {
    step_1 = true; step_2 = true;
    error = No_error; step = Track;
    Read_noise_fit();
    Read_target_pose();
    geometry_msgs::Point prev;
    int same_id = Search_num_list(goal -> id, load_list); //在保存列表中搜索目标id
    if(same_id < 0) 
    {
      error = List_Error;
      Apriltag_tracker_action_result_publish(-1, error, goal -> id); // 列表读取失败
      return 0;
    }
    else
    {
      tran = load_list[same_id]; //读取目标id数据
      apriltag_tracker::set_id set_id_srv;
      set_id_srv.request.id = tran.id;
      if(!set_id_client.call(set_id_srv))
      {
        error = Service_Error;
        Apriltag_tracker_action_result_publish(-1, error, goal -> id); // 服务调用失败
        return 0;
      }
    }
    sleep(1);

    while(step != Finish_Back)
    {
      ros::spinOnce();
      geometry_msgs::Point target = Get_tran(tag, tran);

      // 追踪
      if(step == Track)
      {
        if(Pure_pursuit_control(target, prev)) step = Finish_Track; // 开始追踪
        if(tracker_as_-> isPreemptRequested()) 
        {
          error = Track_Cancel;
          Apriltag_tracker_action_result_publish(step, error, goal -> id); //取消追踪
          return 0;
        }
        if(tag.id < 0)
        {
          error = Goal_Lose;
          Apriltag_tracker_action_result_publish(-1, error, goal -> id); // 目标丢失
          return 0;
        }
      }

      // 到达目标点
      else if(step == Finish_Track)
      {
        ROS_WARN("dx: %f, dy: %f, dz: %f", target.x, target.y, target.z / pi * 180);
        if(target.x <= 0.03 && target.y <= 0.01)
        {
          step = Standby;
          Apriltag_tracker_action_result_publish(step, error, goal -> id); // 到达目标点, 转入待机模式
          return 0; 
        }
        else 
        {
          error = Track_Fail;
          Apriltag_tracker_action_result_publish(-1, error, goal -> id); // 追踪失败
          return 0;
        }
      }

      else return 0;

      Apriltag_tracker_action_feedback_publish(step, goal -> id);
      tag_pub.publish(target);
      loop_rate.sleep();
    }
  }

  else
  {
    while(step != Finish_Back)
    {
      ros::spinOnce();
      geometry_msgs::Point start_pose;
      geometry_msgs::Point target = Get_tran(tag, tran);

      // 待机
      if(step == Standby)
      {
        ROS_WARN("Apriltag_Tracker standby");
        start_pose = target;
        step = Back_Off; // 
      }

      // 后退
      else if(step == Back_Off)
      {     
        double rho = sqrt(std::pow(start_pose.x - target.x, 2) + std::pow(start_pose.y - target.y, 2));
        // ROS_WARN("Going back, rho: %f", rho);
        if(rho > 0.5)
        {
          step = Finish_Back;
          Apriltag_tracker_action_result_publish(step, error, tag.id); // 后退完成
          return 0;
        }
        Cmd_vel_pub(-0.05, 0);
      }

      else return 0;

      Apriltag_tracker_action_feedback_publish(step, tag.id);
      loop_rate.sleep();
    }
  }
}


int main(int argc, char *argv[])
{
  ros::init(argc, argv, "apriltag_tracker");
  ros::NodeHandle nh("~");
  Apriltag_tracker apriltag_tracker;
  ros::spin();
  return 0;
}
