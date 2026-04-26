#ifndef MOTOR_485_CONTROL_MOTOR_485_CONTROLLER_H
#define MOTOR_485_CONTROL_MOTOR_485_CONTROLLER_H

// ROS核心头文件
#include <ros/ros.h>
#include <std_msgs/Float32.h>
// 串口库
#include <serial/serial.h>
// 自定义消息
#include <lifting_ros/limitation_state.h>
#include <lifting_ros/speed_control.h>
// C++基础头文件
#include <cstdint>
#include <queue>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
class Motor485Controller
{
public:
    /**
     * @brief 构造函数
     * @param nh ROS节点句柄（用于创建话题、定时器、读取参数）
     */
    explicit Motor485Controller(ros::NodeHandle& nh);

    /**
     * @brief 析构函数
     * 关闭串口，释放资源
     */
    ~Motor485Controller();

    /**
     * @brief 初始化串口和ROS通信
     * @return 成功返回true，失败返回false
     */
    bool init();
    ros::NodeHandle nh_;          
    ros::Subscriber speed_sub_;   
    ros::Publisher limit_pub_;    
    ros::Timer limit_timer_;  
    ros::Timer queue_timer_;    
    serial::Serial ser_;          
    std::mutex ser_mutex_; 
    bool inverse_flag;       

    std::string port_;           
    int baudrate_;               
    int timeout_ms_;             
    int read_interval_ms_;        

    std::queue<std::vector<uint8_t>> cmd_queue_;   
    std::mutex queue_mutex_;                       
    const uint8_t DEVICE_ADDR_ = 0x01; 
    const uint16_t CTRL_REG_ = 0x0042;   
    const uint16_t LIMIT1_REG_ = 0x002C; 
    const uint16_t LIMIT2_REG_ = 0x002D; 
    bool limit1_state_;           
    bool limit2_state_;           


    uint16_t crc16Modbus(const uint8_t* data, size_t len);


    void splitCrc16(uint16_t crc, uint8_t& crc_high, uint8_t& crc_low);


    std::vector<uint8_t> assembleControlCmd(float speed_percent);


    std::vector<uint8_t> assembleReadLimitCmd(uint16_t reg_addr);


    bool parseLimitFeedback(const std::vector<uint8_t>& recv_data, uint16_t reg_addr, bool& limit_state);


    bool sendSerialData(const std::vector<uint8_t>& send_data);


    bool recvSerialData(std::vector<uint8_t>& recv_data, size_t expect_len);


    void speedPercentCallback(const std_msgs::Float32::ConstPtr& msg);

    void queueTimerCallback(const ros::TimerEvent& event);
    void readLimitTimerCallback(const ros::TimerEvent& event);

};

#endif