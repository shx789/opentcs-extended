#include <laser_udp/laser_common_udp.h>
#include <laser_udp/laser_parser.h>

#include <ros/ros.h>
#include <nodelet/nodelet.h>

#include <boost/thread.hpp>
#include <boost/smart_ptr.hpp>

namespace laser_udp
{
class laserNodelet: public nodelet::Nodelet
{
private:
    boost::shared_ptr<boost::thread> publish_scan_thread_;
    bool enabled_;
public:
    virtual void onInit()
    {
        ROS_INFO("Bringup laser nodelet");
        enabled_ = true;
        publish_scan_thread_ = boost::make_shared<boost::thread>(boost::bind(&laserNodelet::publishScan, this));
    }

    void publishScan()
    {
        ros::NodeHandle nh  = getNodeHandle();
        ros::NodeHandle pnh = getPrivateNodeHandle();

        /*Check whether hostname is provided*/
        bool isTcpConnection = false;
        std::string strHostName;
        std::string strPort;

        if(pnh.getParam("hostname", strHostName))
        {
            isTcpConnection = true;
            pnh.param<std::string>("port", strPort, "2112");
        }

        /*Get configured time limit*/
        int iTimeLimit = 5;
        pnh.param("timelimit", iTimeLimit, 5);

        bool isDataSubscribed = false;
        pnh.param("subscribe_datagram", isDataSubscribed, false);

        int iDeviceNumber = 0;
        pnh.param("device_number", iDeviceNumber, 0);

        /*Create and initialize parser*/
        laser_udp::ClaserParser *pParser = new laser_udp::ClaserParser();
        double param;
        std::string frame_id;

        if(pnh.getParam("range_min", param))
        {
            ROS_INFO("range_min: %f", param);
            pParser->SetRangeMin(param);
        }
        if(pnh.getParam("range_max", param))
        {
            ROS_INFO("range_max: %f", param);
            pParser->SetRangeMax(param);
        }
        if(pnh.getParam("time_increment", param))
        {
            ROS_INFO("time_increment: %f", param);
            pParser->SetTimeIncrement(param);
        }

        if(pnh.getParam("frame_id", frame_id))
        {
            ROS_INFO("frame_id: %s", frame_id.c_str());
            pParser->SetFrameId(frame_id);
        }

        /*Setup TCP connection and attempt to connect/reconnect*/
        laser_udp::ClaserCommon *plaserLs = NULL;
        int result = laser_udp::ExitError;
        while(ros::ok() && enabled_ == true)
        {
            if(plaserLs != NULL)
            {
                delete plaserLs;
            }

            plaserLs = new laser_udp::ClaserCommonUdp(strHostName, strPort, iTimeLimit, pParser, nh, pnh);
            result = plaserLs->Init();

            /*Device has been initliazed successfully*/
            while(ros::ok() && (result == laser_udp::ExitSuccess) && enabled_ == true)
            {
                ros::spinOnce();
                result = plaserLs->LoopOnce();
            }

            if(result == laser_udp::ExitFatal)
            {
                ROS_ERROR("Grab laser data error:ExitFatal");
                return ;
            }
        }

        if(plaserLs != NULL)
        {
            delete plaserLs;
        }

        if(pParser != NULL)
        {
            delete pParser;
        }
    }

    ~laserNodelet()
    {
        enabled_ = false;
        publish_scan_thread_->join();
    }
};
}/*laser_udp*/

// watch the capitalization carefully
#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(laser_udp::laserNodelet, nodelet::Nodelet)
