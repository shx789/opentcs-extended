#include "ros_scan_shadow.h"

  void configure(const float min_angle, const float max_angle)
  {
    min_angle_tan_ = tanf(min_angle);
    max_angle_tan_ = tanf(max_angle);

    // Correct sign of tan around singularity points
    if (min_angle_tan_ < 0.0)
      min_angle_tan_ = -min_angle_tan_;
    if (max_angle_tan_ > 0.0)
      max_angle_tan_ = -max_angle_tan_;
  }

bool isShadow(const float r1, const float r2, const float included_angle)
{
const float perpendicular_y_ = r2 * sinf(included_angle);
const float perpendicular_x_ = r1 - r2 * cosf(included_angle);
const float perpendicular_tan_ = fabs(perpendicular_y_) / perpendicular_x_;

if (perpendicular_tan_ > 0)
{
    if (perpendicular_tan_ < min_angle_tan_)
    return true;
}
else
{
    if (perpendicular_tan_ > max_angle_tan_)
    return true;
}
return false;
}

 bool update(const sensor_msgs::LaserScan& scan_in, sensor_msgs::LaserScan& scan_out)
  {
    // copy across all data first
    scan_out = scan_in;
    std::set<int> indices_to_delete;
    // For each point in the current line scan
    for (unsigned int i = 0; i < scan_in.ranges.size(); i++)
    {
      for (int y = -window_; y < window_ + 1; y++)
      {
        int j = i + y;
        if (j < 0 || j >= (int)scan_in.ranges.size() || (int)i == j)
        {  // Out of scan bounds or itself
          continue;
        }

        if (isShadow(scan_in.ranges[i], scan_in.ranges[j], y * scan_in.angle_increment))
        {
          for (int index = std::max<int>(i - neighbors_, 0); index <= std::min<int>(i + neighbors_, (int)scan_in.ranges.size() - 1); index++)
          {
            if (scan_in.ranges[i] < scan_in.ranges[index])
            {  // delete neighbor if they are farther away (note not self)
              indices_to_delete.insert(index);
            }
          }
          if (remove_shadow_start_point_)
          {
            indices_to_delete.insert(i);
          }
        }
      }
    }

    for (std::set<int>::iterator it = indices_to_delete.begin(); it != indices_to_delete.end(); ++it)
    {
      scan_out.ranges[*it] = std::numeric_limits<float>::quiet_NaN();  // Failed test to set the ranges to invalid value
    }
    return true;
  }


void scan_cb(const sensor_msgs::LaserScan::ConstPtr &msg)
{
    sensor_msgs::LaserScan out_scan;
    if(update(*msg,out_scan))
    {
        output_pub_.publish(out_scan);
    }
}


int main(int argc, char **argv)
{
    ros::init(argc, argv, "ros_scan_shadown");
    ros::NodeHandle n;
    ros::NodeHandle private_nh("~");

    private_nh.param<double>("min_angle",                min_angle_,        10);  //
    private_nh.param<double>("max_angle",               max_angle_,        170);  //
    private_nh.param<std::string>("scan_topic",         scan_topic_,     "scan0"); //
    private_nh.param<std::string>("out_topic",           out_topic_,     "scan_filtered"); //
    private_nh.param<int>("window",                              window_,          1);  //
    private_nh.param<int>("neighbors",                          neighbors_,         10); //

    configure(angles::from_degrees(min_angle_),angles::from_degrees(max_angle_));

    ros::Subscriber scan_sub = n.subscribe(scan_topic_, 1, scan_cb);
    output_pub_ = n.advertise<sensor_msgs::LaserScan>(out_topic_, 1000);
    ros::spin();
    return 0;
}
