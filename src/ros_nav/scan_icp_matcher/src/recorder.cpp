#include "ros/ros.h"
#include "ros/package.h"

#include <pcl_ros/transforms.h>

#include <pcl/io/pcd_io.h>
#include <pcl/io/png_io.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/extract_indices.h>
#include <pcl_conversions/pcl_conversions.h>

#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>

#include "sensor_msgs/LaserScan.h"
#include "tf/transform_listener.h"
#include "laser_geometry/laser_geometry.h"

laser_geometry::LaserProjection laser_projector;

std::vector<sensor_msgs::LaserScan> scan_msgs;
void scan_cb(const sensor_msgs::LaserScan::ConstPtr &msg)
{
  scan_msgs.push_back(*msg);
}

sensor_msgs::LaserScan calculateMeanScan(std::vector<sensor_msgs::LaserScan> &scans)
{
  sensor_msgs::LaserScan mean_scan;
  int scan_count = scans[0].ranges.size();

  for(int i = 0; i < scans.size(); i++)
  {
    if(i == 0)
    {
      mean_scan = scans[i];
      continue;
    }

    for(int j = 0; j < scan_count; j++)
    {
      mean_scan.ranges[j] += scans[i].ranges[j];
    }
  }

  for(auto &range : mean_scan.ranges)
  {
    range /= scans.size();
  }

  return mean_scan;
}

pcl::PointCloud<pcl::PointXYZ> scanToPCL(sensor_msgs::LaserScan &scan, tf::TransformListener &tfListener)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  sensor_msgs::PointCloud2 pointCloud;
  laser_projector.transformLaserScanToPointCloud(scan.header.frame_id, scan, pointCloud, tfListener);
  pcl::fromROSMsg (pointCloud, cloud);
  return cloud;
}

void locationFilter(pcl::PointCloud<pcl::PointXYZ> &cloud, double filter_x, double filter_y)
{
  try
  {
    pcl::PointIndices::Ptr inliers(new pcl::PointIndices);

    int count = 0;
    for(int i = 0; i < cloud.size(); i++)
    {
      if(abs(cloud.points[i].x) > filter_x || abs(cloud.points[i].y) > filter_y)
      {
        inliers->indices.push_back(i);
        count++;
      }
    }
    pcl::ExtractIndices<pcl::PointXYZ> extract;
    extract.setInputCloud(cloud.makeShared());
    extract.setIndices(inliers);
    extract.setNegative(true);
    extract.filter(cloud);
  }
  catch(const std::exception& e)
  {
    ROS_ERROR("%s",e.what());
  }
}

void voxelFilter(pcl::PointCloud<pcl::PointXYZ> &cloud, double leaf_size)
{
  pcl::PCLPointCloud2::Ptr cloud2 (new pcl::PCLPointCloud2 ());
  pcl::toPCLPointCloud2(cloud, *cloud2);
  pcl::VoxelGrid<pcl::PCLPointCloud2> sor;
  sor.setInputCloud (cloud2);
  sor.setLeafSize (leaf_size, leaf_size, leaf_size);
  sor.filter (*cloud2);
  pcl::fromPCLPointCloud2(*cloud2, cloud);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "pattern_recorder");
    ros::NodeHandle n;

    tf::TransformListener tfListener;

    // Parameters
    int mean_queue_size;

    double location_filter_x;
    double location_filter_y;
    double voxel_leaf_size;

    std::string scan_topic;
    std::string output_file;

    // Get Parameters from ROS Parameter Server
    ros::NodeHandle n_private("~");
    if(!n_private.getParam("mean_queue_size", mean_queue_size)) mean_queue_size = 20; 

    if(!n_private.getParam("location_filter_x", location_filter_x)) location_filter_x = 0.3;
    if(!n_private.getParam("location_filter_y", location_filter_y)) location_filter_y = 0.15;
    if(!n_private.getParam("voxel_leaf_size", voxel_leaf_size)) voxel_leaf_size = 0.0075f;

    if(!n_private.getParam("scan_topic", scan_topic)) scan_topic = "scan";
    if(!n_private.getParam("output_file", output_file)) output_file = ros::package::getPath("scan_icp_matcher") + "/pcd/pattern_charge.pcd";

    // Scan Subscriber
    ros::Subscriber scan_sub = n.subscribe<sensor_msgs::LaserScan>(scan_topic, 1, scan_cb);

    ROS_INFO("Collecting Scans");
    
    ros::Rate rate(6);
    while(ros::ok() && scan_msgs.size() < mean_queue_size)
    {
      ros::spinOnce();
      rate.sleep();
    }
    ROS_INFO("Collected Scans");

    // Calculate mean of collected scans
    sensor_msgs::LaserScan final = calculateMeanScan(scan_msgs);

    // Convert mean scan to PCL cloud
    pcl::PointCloud<pcl::PointXYZ> cloud = scanToPCL(final, tfListener);

    /*
    // Apply location and voxel filter
    locationFilter(cloud, location_filter_x, location_filter_y);
    voxelFilter(cloud, voxel_leaf_size);
    */
   pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZ>);
   pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
   pcl::VoxelGrid<pcl::PointXYZ> vg;
   pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;

    //ec.setClusterTolerance(0.02);//欧氏距离的阈值
    ec.setClusterTolerance(0.05);//欧氏距离的阈值
    //ec.setClusterTolerance(0.1);
    ec.setMinClusterSize(20);
    ec.setMaxClusterSize(70);

    vg.setLeafSize (voxel_leaf_size, voxel_leaf_size, voxel_leaf_size);
    // Apply voxel filter
    vg.setInputCloud (cloud.makeShared());
    vg.filter (*cloud_filtered);

    // Set filtered cloud as input for KD Tree
    tree->setInputCloud (cloud_filtered);

    // Extract clusters
    std::vector<pcl::PointIndices> cluster_indices;
    ec.setSearchMethod (tree);
    ec.setInputCloud (cloud_filtered);
    ec.extract (cluster_indices);

    pcl::PointCloud<pcl::PointXYZ>::Ptr nearl_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    int index = 0;
    double min_distance = 10;
    for(auto cluster_indice : cluster_indices)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZ>);
  
        // Create two PCL clouds from cluster_indice
        // Give different color for every cluster
        for (const auto& indice : cluster_indice.indices)
        {
            pcl::PointXYZ point;

            point.x = (*cloud_filtered)[indice].x;
            point.y = (*cloud_filtered)[indice].y;
            point.z = (*cloud_filtered)[indice].z;

            cloud_cluster->push_back(point);
        }
        // Compute cluster centroid
        pcl::PointXYZ centroid;
        pcl::computeCentroid(*cloud_cluster, centroid);

        double distance = sqrt(centroid.x * centroid.x + centroid.y * centroid.y);
        if(min_distance > distance)
        {
          nearl_cloud = cloud_cluster;
          min_distance = distance;
        }
        printf("x:%.3f  y:%.3f index:%d size:%d dis:%.3f\n",centroid.x,centroid.y,index,cluster_indice.indices.size(),min_distance);
        
        index++;
    }
    //pcl::io::savePCDFileASCII("/home/robotcar/3.pcd", *nearl_cloud);
    // Save cloud to pcd file
    pcl::io::savePCDFileASCII(output_file, *nearl_cloud);
    //pcl::io::savePCDFileASCII(output_file, cloud);
    ROS_WARN("Cloud written to %s", output_file.c_str());
}
