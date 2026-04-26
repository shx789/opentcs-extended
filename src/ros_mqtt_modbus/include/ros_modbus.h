#ifndef __ROS_MODBUS_H_
#define __ROS_MODBUS_H_

#include <iostream>
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <string.h>
using namespace std;
/*如果是windows平台则要加载相应的静态库和头文件*/
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <modbus.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "modbus.lib")
/*linux平台*/
#else
#include <modbus/modbus.h>
#include <unistd.h>
#include <error.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#endif
#include "ros_mqtt.h"
#include "ros_roller.h"

#define NB_CONNECTION 5

#define BASE_ADDR   1
#define MOVE_CONTROL_ADDR 5
#define POINT_CONTROL_ADDR 16
#define TRACK_CONTROL_ADDR 24
#define MORE_TASK_CONTROL_ADDR 32
#define CHARGE_CONTROL_ADDR 39
#define LOCAT_CONTROL_ADDR 43
#define QR_LOCAT_CONTROL_ADDR 47
#define STOP_CONTROL_ADDR 50
#define CLEAN_CONTROL_ADDR 51
#define RESET_CONTROL_ADDR 52
#define SOF_STOP_CONTROL_ADDR 53
#define ROLLER_CONTROL_ADDR 54
#define MAGNETIC_CONTROL_ADDR 55


#define INPUT_BASE_ADDR         1
#define INPUT_ROLE_ADDR         46
#define INPUT_LIGHT_IN_ADDR     47
#define INPUT_LIGHT_OUT_ADDR    48

#define MATERIAL_LIGHT_ADDR     49
#define UP_LIGHT_ADDR           50
#define LOW_LIGHT_ADDR          51



class RDSModbusSlave
{
public:
    RDSModbusSlave();
    ~RDSModbusSlave();

public:
    void recieveMessages();
    bool modbus_set_slave_id(int id);
    bool initModbus(std::string Host_Ip, int port, bool debugging);
    uint8_t getTab_Input_Bits(int NumBit);
    bool setTab_Input_Bits(int NumBit, uint8_t Value);
    uint16_t getRegisterValue(int registerNumber);
    bool setRegisterValue(int registerNumber, uint16_t Value);
    bool setTab_input_registers(int registerNumber, uint16_t Value);
    bool setTab_input_Floatregisters(int registerStartaddress, float Value);
    bool setRegisterFloatValue(float Value, int registerStartaddress);
    float getRegisterFloatValue(int registerStartaddress);

private:
    std::mutex slavemutex;
    int m_errCount{ 0 };
    int server_socket{ -1 };
    modbus_t* ctx{ nullptr };
    modbus_mapping_t* mapping{ nullptr };
    /*Mapping*/
    int m_numBits{ 500 };
    int m_numInputBits{ 500 };
    int m_numRegisters{ 500 };
    int m_numInputRegisters{ 500 };


public:
    void loadFromConfigFile();
    void run();
	void close_sigint(int dummy);
};
/*annotation:
(1)https://www.jianshu.com/p/0ed380fa39eb
(2)typedef struct _modbus_mapping_t
{
    int nb_bits;                //线圈
    int start_bits;
    int nb_input_bits;          //离散输入
    int start_input_bits;
    int nb_input_registers;     //输入寄存器
    int start_input_registers;
    int nb_registers;           //保持寄存器
    int start_registers;
    uint8_t *tab_bits;
    uint8_t *tab_input_bits;
    uint16_t *tab_input_registers;
    uint16_t *tab_registers;
}modbus_mapping_t;*/

#endif


