#include <ros/ros.h>
#include "wj_716_lidar_protocol.h"
#include "udp_driver.h"
using namespace wj_lidar;

/* ------------------------------------------------------------------------------------------
 * 
 * Version: wanji_716_lidar_UDP
 *
 * ------------------------------------------------------------------------------------------ */

wj_716_lidar_protocol *protocol_net = NULL;
udp_driver *udp_drv = NULL;

void callback(wj_716_lidar::wj_716_lidarConfig &config,uint32_t level)
{
  protocol_net->setConfig(config,level);
}

void GetMAC()
{
    unsigned char getmaccommandbuf[34] = {0xFF,0xAA,0x00,0x1E,0x00,0x00,0x00,0x00,0x00,
                                          0x00,0x01,0x01,0x00,0x05,0x00,0x00,0x00,0x00,
                                          0x00,0x00,0x00,0x00,0x06,0x08,0x00,0x00,0x00,
                                          0x00,0x00,0x00,0x00,0x15,0xEE,0xEE};
    udp_drv->SendData(getmaccommandbuf,34);
    cout << "Sending command for getting MAC!"<<endl;
}

void GetScan()
{
    unsigned char scandatacomm[34]= {0xFF,0xAA,0x00,0x1E,0x00,0x00,0x00,0x00, 
                                     0x00,0x00,0x01,0x01,0x00,0x05,0x00,0x00,
                                     0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x01,
                                     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,
                                     0xEE,0xEE};
    udp_drv->SendData(scandatacomm,34);
    cout << "Sending command for getting scanned data!"<<endl;
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "wj_716_lidar_01");
  std::string lidar_ip, host_ip;
  int lidar_port, host_port;
  
  ros::NodeHandle nh("~");

  nh.getParam("lidar_ip",lidar_ip);
  nh.getParam("lidar_port",lidar_port);
  nh.getParam("host_ip",host_ip);
  nh.getParam("host_port",host_port);

  cout << "host_ip: " << host_ip << ",host_port:" << host_port <<endl;
  cout << "lidar_ip: " << lidar_ip << ",lidar_port:" << lidar_port <<endl;

  protocol_net = new wj_716_lidar_protocol();

  dynamic_reconfigure::Server<wj_716_lidar::wj_716_lidarConfig> server;
  dynamic_reconfigure::Server<wj_716_lidar::wj_716_lidarConfig>::CallbackType f;
  f = boost::bind(&callback,_1,_2);
  server.setCallback(f);

  udp_drv = new udp_driver(host_ip, host_port);
  udp_drv->udp_open(lidar_ip, lidar_port,protocol_net);

  //getting MAC. You can read MAC from a global variable named wj_716_lidar_protocol.DeviceMAC.
  GetMAC();

  while(ros::ok())
  {
    try 
    {
      if(protocol_net->heartstate)
      {
        protocol_net->heartstate = false;
      }
      else
      {
        if(udp_drv==NULL)
        {
          udp_drv = new udp_driver(host_ip, host_port);
          udp_drv->udp_open(lidar_ip, lidar_port,protocol_net);
        }
        GetScan();
      }
    }
    catch(...)
    {
        cout << "udp close!" << endl;
        delete udp_drv; 
    }
    ros::spinOnce();
    ros::Duration(2).sleep();
  }
  delete udp_drv;
}
