#include "udp_driver.h"

udp_driver::udp_driver(std::string dest_ip, uint16_t dest_port)
	: IOService(),
	  Thread(),
	  ReceiveSocket(nullptr), //any ip
	  IsReceiving(false),
	  ShouldStop(false),
	  DummyWork(new boost::asio::io_service::work(IOService))
{
	DestIp = dest_ip;
	DestPort = dest_port;
}

udp_driver::~udp_driver()
{
	UnInitialize();
}

bool udp_driver::udp_open(std::string device_ip, uint16_t device_port, wj_716_lidar_protocol *protocol1)
{
	try
	{
		LidarIp = device_ip;
		LidarPort = device_port;
		protocol = protocol1;
		bool ret = Initialize();
	}
	catch (boost::system::system_error &e)
	{
		boost::system::error_condition ecnd = e.code().default_error_condition();
		if (ecnd.value() == boost::system::errc::errc_t::address_not_available)
		{
			std::stringstream ss;
			ss << "The specified address could not be binded, IP:Port " << device_ip << ":" << device_port;
			std::string cause(ss.str());
		}
		return false;
	}
}

void udp_driver::udp_close()
{
	UnInitialize();
}

bool udp_driver::Initialize()
{
	if (LidarIp.empty())
		return false;
	IOService.reset();
	DummyWork.reset(new boost::asio::io_service::work(IOService));

	ReceiveSocketStatus = true;
	boost::asio::ip::udp::endpoint ReceiveEndpoint(boost::asio::ip::udp::v4(), DestPort);
	ReceiveEndpoint.address(boost::asio::ip::address::from_string(DestIp));
	try
	{
		ReceiveSocket = new boost::asio::ip::udp::socket(IOService, ReceiveEndpoint);
	}
	catch (...)
	{
	}
	LIDAREndpoint.address(boost::asio::ip::address::from_string(LidarIp));
	LIDAREndpoint.port(LidarPort);
	IsReceiving = false;
	ShouldStop = false;

	StartThread();
	StartReceiveService();

	return true;
}

void udp_driver::UnInitialize()
{
	if (ReceiveSocketStatus == false)
		return;
	if (this->ReceiveSocket != nullptr && this->ReceiveSocket->is_open())
	{
		{
			boost::unique_lock<boost::mutex> guard(this->IsReceivingMtx);
			this->ShouldStop = true;
			this->ReceiveSocket->close();
			while (this->IsReceiving)
			{
				this->IsReceivingCond.wait(guard);
			}
		}
	}

	delete this->ReceiveSocket;
	this->ReceiveSocket = nullptr;

	IOService.stop();

	if (this->Thread)
	{
		this->Thread->join();
		this->Thread.reset();
	}

	DummyWork.reset();
}

void udp_driver::StopIOService()
{
	IOService.stop();
	while (!IOService.stopped())
	{
		boost::this_thread::sleep(boost::posix_time::milliseconds(10));
		IOService.stop();
	}
}

void udp_driver::StartThread()
{
	if (this->Thread)
		return;

	this->Thread.reset(new boost::thread(
		boost::bind(&boost::asio::io_service::run, &this->IOService)));
}

void udp_driver::StartReceiveService()
{
	ReceiveSocket->async_receive_from(boost::asio::buffer(this->ReceiveBuffer), RemoteEndpoint,
									  boost::bind(&udp_driver::HandReceive, this,
									  boost::asio::placeholders::error,
									  boost::asio::placeholders::bytes_transferred));
}

void udp_driver::HandReceive(const boost::system::error_code &error, std::size_t rxBytes)
{
	protocol->dataProcess(ReceiveBuffer, rxBytes);
	this->StartReceiveService();
}

bool udp_driver::reset()
{
	UnInitialize();
	return Initialize();
}

bool udp_driver::reset(std::string device_ip, uint16_t device_port, std::string dest_ip, uint16_t dest_port)
{
}

int udp_driver::SendData(unsigned char *buffer, int size)
{
	try
	{
		return this->ReceiveSocket->send_to(boost::asio::buffer(buffer, size), LIDAREndpoint);
	}
	catch (...)
	{
		return 0;
	}
}
