#include "ros/ros.h"
#include "ros/package.h"

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/search/kdtree.h>
#include <pcl/filters/filter.h>
#include <pcl/registration/icp.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/segmentation/extract_clusters.h>

#include "sensor_msgs/LaserScan.h"
#include "sensor_msgs/PointCloud2.h"

#include "tf/transform_listener.h"
#include "tf/transform_broadcaster.h"
#include "laser_geometry/laser_geometry.h"
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>

#include "scan_icp_matcher/server1.h"

bool scan_msg_came,get_pattern_name;
sensor_msgs::LaserScan scan_msg;
std::string filepath;
pcl::PointCloud<pcl::PointXYZ>::Ptr pattern(new pcl::PointCloud<pcl::PointXYZ>);

void scan_cb(const sensor_msgs::LaserScan::ConstPtr &msg)
{
    scan_msg = *msg;
    scan_msg_came = true;
}

bool pattern_filepath_cb(scan_icp_matcher::server1::Request & request,
                                                    scan_icp_matcher::server1::Response & response)
{
    
    filepath = ros::package::getPath("scan_icp_matcher") + "/pcd/" + request.name.data + ".pcd";
    if (pcl::io::loadPCDFile<pcl::PointXYZ> (filepath, *pattern) == -1)
    {
        ROS_ERROR ("Could not read pattern file");
        get_pattern_name = false;
        response.result = false;
    }
    else
    {
        get_pattern_name = true;
        ROS_WARN("load %s",filepath.c_str());
        response.result = true;
    }
   return true;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "pattern_matcher");
    ros::NodeHandle n;

    // Parameters
    std::string scan_topic;
    std::string pattern_topic;
    std::string clustered_points_topic;
    std::string pattern_filepath;
    std::string icp_fitness_score;
    double min_score;
    int first_flag = 10;

    // Get Parameters from ROS Parameter Server
    ros::NodeHandle n_private("~");
    if(!n_private.getParam("scan_topic", scan_topic)) scan_topic = "scan";
    if(!n_private.getParam("pattern_topic", pattern_topic)) pattern_topic = "pattern";
    if(!n_private.getParam("clustered_points_topic", clustered_points_topic)) clustered_points_topic = "clustered_points";
    if(!n_private.getParam("pattern_filepath", pattern_filepath)) pattern_filepath = ros::package::getPath("scan_icp_matcher") + "/pcd/pattern.pcd";
    if(!n_private.getParam("icp_fitness_score", icp_fitness_score)) icp_fitness_score = "icp_fitness_score";
    if(!n_private.getParam("min_score", min_score)) min_score = 0.000035;

    // ROS Subscribers and Publishers
    ros::Subscriber scan_sub = n.subscribe(scan_topic, 1, scan_cb);
    ros::Publisher pattern_pub = n.advertise<sensor_msgs::PointCloud2>(pattern_topic, 100);
    ros::Publisher clustered_pub = n.advertise<sensor_msgs::PointCloud2>(clustered_points_topic, 100);
    ros::Publisher icp_fitness_score_pub = n.advertise<std_msgs::Float64>(icp_fitness_score, 100);

    // 创建一个服务对象
    ros::ServiceServer serve = n.advertiseService("pattern_file_load",pattern_filepath_cb);

    scan_msg_came = false;
    get_pattern_name = false;
    laser_geometry::LaserProjection projector;

    // TF Listener and Broadcaster
    tf::TransformListener tfListener;
    static tf::TransformBroadcaster tfBroadcaster;

    // Read pattern cloud from file
    //pcl::PointCloud<pcl::PointXYZ>::Ptr pattern(new pcl::PointCloud<pcl::PointXYZ>);
    if (pcl::io::loadPCDFile<pcl::PointXYZ> (pattern_filepath, *pattern) == -1)
    {
        ROS_ERROR ("Could not read pattern file");
        return 0;
    }

    // Calculate pattern centroid
    pcl::PointXYZ pattern_centroid;
    pcl::computeCentroid(*pattern, pattern_centroid);

    // PCL Spesific
    pcl::console::setVerbosityLevel(pcl::console::L_ALWAYS);

    pcl::VoxelGrid<pcl::PointXYZ> vg;
    vg.setLeafSize (0.0001f, 0.0001f, 0.0001f);

    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);

    pcl::PointCloud<pcl::PointXYZ> cloud;
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr icp_result (new pcl::PointCloud<pcl::PointXYZ>);

    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    //ec.setClusterTolerance(0.03);//欧氏距离的阈值
    ec.setClusterTolerance(0.1);
    ec.setMinClusterSize(20);
    ec.setMaxClusterSize(500);
    //ec.setMaxClusterSize(50);  //设置最大点数和最小点数

    pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
    icp.setInputSource(pattern);
    icp.setMaxCorrespondenceDistance(0.04);

    sensor_msgs::PointCloud2 pointCloud;
    sensor_msgs::PointCloud2 cloud_reconstructed_msg;

    // Convert pattern cloud from PCL Cloud to ROS PointCloud2
    sensor_msgs::PointCloud2 pattern_msg;
    pcl::toROSMsg(*pattern, pattern_msg);
    pattern_msg.header.frame_id = "result";

    // Colors
    int color_index = 0;
    int colors[] = {255, 0, 0,          // RED
                    0, 255, 0,          // GREEN
                    0, 0, 255,          // BLUE
                    255, 255, 0,        // ???
                    0, 255, 255,        // ???
                    255, 255, 255       // WHITE
                    };
    int color_count = sizeof(colors) / sizeof(int) / 3;

    // For transforming between PCL and TF
    Eigen::Affine3f affine_transform;
    Eigen::Matrix4f initial_aligment;
    float tx, ty, tz, roll, pitch, yaw;

    // Best ICP fitness score
    double best_fitness;

    ros::Rate rate(30);
    while(ros::ok())
    {
        ros::spinOnce();
        rate.sleep();

        // Check if new scan msg arrived
        if(!scan_msg_came) continue;
        scan_msg_came = false;

        //lload pattern file
        if(get_pattern_name)
        {
            get_pattern_name = false;
            pcl::computeCentroid(*pattern, pattern_centroid);
            icp.setInputSource(pattern);
            pcl::toROSMsg(*pattern, pattern_msg);
            pattern_msg.header.frame_id = "result";
        }

        // Convert sensor_msgs::LaserScan to PCL Cloud
        projector.transformLaserScanToPointCloud(scan_msg.header.frame_id, scan_msg, pointCloud, tfListener);
        pcl::fromROSMsg (pointCloud, cloud);

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
        
        pcl::PointCloud<pcl::PointXYZRGB> cloud_reconstructed;
        std::vector<std::pair<pcl::PointCloud<pcl::PointXYZ>::Ptr, pcl::PointXYZ>> clusters;

        best_fitness = 1.0;

        /**
         * For Every Cluster:
         * 1. Give different color
         * 2. Run ICP on it
         * 3. Check if ICP result is the best
         */
        for(auto cluster_indice : cluster_indices)
        {
            pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZ>);
            pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_cluster_rgb (new pcl::PointCloud<pcl::PointXYZRGB>);

            // Create two PCL clouds from cluster_indice
            // Give different color for every cluster
            for (const auto& indice : cluster_indice.indices)
            {
                pcl::PointXYZ point;
                pcl::PointXYZRGB point_rgb;

                point.x = point_rgb.x = (*cloud_filtered)[indice].x;
                point.y = point_rgb.y = (*cloud_filtered)[indice].y;
                point.z = point_rgb.z = (*cloud_filtered)[indice].z;

                point_rgb.r = *(colors + (color_index*3));
                point_rgb.g = *(colors + (color_index*3) + 1);
                point_rgb.b = *(colors + (color_index*3) + 2);

                cloud_cluster->push_back(point);
                cloud_cluster_rgb->push_back(point_rgb);
            }

            color_index += 1;
            if(color_index >= color_count) color_index = 0;

            cloud_cluster_rgb->width = cloud_cluster_rgb->size();
            cloud_cluster_rgb->height = 1;
            cloud_cluster_rgb->is_dense = true;
            
            cloud_reconstructed += *cloud_cluster_rgb;

            pcl::toROSMsg(cloud_reconstructed, cloud_reconstructed_msg);
            cloud_reconstructed_msg.header.stamp = ros::Time::now();
            cloud_reconstructed_msg.header.frame_id = scan_msg.header.frame_id;
            clustered_pub.publish(cloud_reconstructed_msg);
            
            // Compute cluster centroid
            pcl::PointXYZ centroid;
            pcl::computeCentroid(*cloud_cluster, centroid);

            // Calculate initial alignment
            pcl::getTransformation(centroid.x - pattern_centroid.x, centroid.y - pattern_centroid.y, 0, 0, 0, 0, affine_transform);
            initial_aligment = affine_transform.matrix();

            // Run ICP
            icp.setInputTarget(cloud_cluster);
            icp.align (*icp_result, initial_aligment);

            // Check if ICP converged
            if(!icp.hasConverged()) continue;            

            // Check if this convergence is better
            if(best_fitness < icp.getFitnessScore()) continue;

            // Store best fitness and transformation values
            best_fitness = icp.getFitnessScore();
            pcl::getTranslationAndEulerAngles(Eigen::Affine3f(icp.getFinalTransformation()), tx, ty, tz, roll, pitch, yaw);
        }

        // Check if a good match were found
        if(best_fitness >= 1.0) continue;

        // Pattern Match Transformation
        tf::Transform transformation;
        transformation.setOrigin(tf::Vector3(tx, ty, 0.0));
        transformation.setRotation(tf::createQuaternionFromYaw(yaw));
        //printf("best_fitness:%f	x:%.3f	y:%.3f	yaw:%.3f\n",best_fitness,tx,ty,yaw);

        //pub icp fitness score
        std_msgs::Float64 temp_fitness;
        temp_fitness.data = best_fitness;
        icp_fitness_score_pub.publish(temp_fitness);

        
        if(best_fitness < min_score)
        {  
            // Publish TF and Pattern PointCloud2
            tfBroadcaster.sendTransform(tf::StampedTransform(transformation, ros::Time::now(), scan_msg.header.frame_id, "result"));
        }
        else if(first_flag) //第一次得发布一次保证不会tf获得不了
        {
            first_flag --;
             // Publish TF and Pattern PointCloud2
            tfBroadcaster.sendTransform(tf::StampedTransform(transformation, ros::Time::now(), scan_msg.header.frame_id, "result"));
            pattern_msg.header.stamp = ros::Time::now();
            pattern_pub.publish(pattern_msg);
        }
        pattern_msg.header.stamp = ros::Time::now();
        pattern_pub.publish(pattern_msg);
       
    }
}
