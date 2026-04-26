#ifndef UDP_DRIVER_H
#define UDP_DRIVER_H
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/scoped_ptr.hpp>
#include <string>
#include <unistd.h>
#include "wj_716_lidar_protocol.h"

using namespace std ;
using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using namespace boost::asio;
using namespace wj_lidar;

class udp_driver
{
public:
    udp_driver(std::string dest_ip, uint16_t dest_port);
    ~udp_driver();
    bool udp_open(std::string device_ip, uint16_t device_port,wj_716_lidar_protocol *protocol1);
    void udp_close();
    bool reset();
    bool reset(std::string device_ip, uint16_t device_port, std::string dest_ip, uint16_t dest_port);
    int SendData(unsigned char* buffer, int size);

private:
    bool Initialize();
    void UnInitialize();
    void StartThread();
    void StopIOService();
    void StartReceiveService();
    void HandReceive(const boost::system::error_code& error, std::size_t rxBytes);
    
private:
	boost::asio::io_service IOService;
	boost::scoped_ptr<boost::thread> Thread;
	boost::asio::ip::udp::socket* ReceiveSocket; 
    boost::asio::ip::udp::endpoint LIDAREndpoint;   //LIDAREndpoint
	boost::asio::ip::udp::endpoint RemoteEndpoint;  //RemoteEndpoint

	bool IsReceiving;
	bool ShouldStop;
    bool ReceiveSocketStatus;
	bool IsResponseReceived;
		
	unsigned char ReceiveBuffer[1500];
	unsigned char SendBuffer[1500];

    boost::mutex IsReceivingMtx;
	boost::condition_variable IsReceivingCond;
    boost::scoped_ptr<boost::asio::io_service::work> DummyWork;

    std::string DestIp;
	uint16_t DestPort;
	std::string LidarIp;
	uint16_t LidarPort; 
    wj_716_lidar_protocol *protocol;
 
};

#endif // UDP_DRIVER_H