#include <iostream>
#include "ros_action_service.h"
#include "ros_magnetic_can.h"
#include "ros_rfid_serial.h"



using namespace std;



//当关闭包时调用，关闭20ms上传
void mySigIntHandler(int sig)
{
   ROS_INFO("close the serial!\n");
   ros::shutdown();
}

//MODBUS线程
void modbusRunner(Action_Service* server)
{
    server->recieveMessages();
}

//MQTT线程
void mqttclientRunner()
{
    run_magnetic_can();
}

//ROLLER线程
void rollerRunner()
{
    get_serial_data();
}


int main(int argc,char **argv)
{
    ros::init(argc,argv,"ros_magnetic_nav_service",ros::init_options::NoSigintHandler);            //解析参数，命名节点为 talker
    
    std::thread mqttSerThread(mqttclientRunner);
    //std::thread rollerSerThread(rollerRunner);
    //后初始化主控程序，方便订阅话题。
    Action_Service modSer;
    std::thread modSerThread(modbusRunner, &modSer);

    modSerThread.join();
    mqttSerThread.join();
    //rollerSerThread.join();
    //std::cout << "Running ModbusTcpSlave" << std::endl;
    return 0;
}
