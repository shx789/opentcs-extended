#include <iostream>
#include "ros_mqtt.h"
#include "ros_modbus.h"
#include "ros_roller.h"

using namespace std;

RDSModbusSlave modSer;

//MODBUS线程
void modbusRunner(RDSModbusSlave* server)
{
    server->recieveMessages();
}

//MQTT线程
void mqttclientRunner()
{
    mqtt_poll();
}

//ROLLER线程
void rollerRunner()
{
    //get_serial_data();
}


int main()
{
    std::thread modSerThread(modbusRunner, &modSer);
    std::thread mqttSerThread(mqttclientRunner);
    std::thread rollerSerThread(rollerRunner);
    modSerThread.join();
    mqttSerThread.join();
    rollerSerThread.join();
    std::cout << "Running ModbusTcpSlave" << std::endl;
    return 0;
}
