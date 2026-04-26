#include <lifting_ros.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
/**
 * @author yqy
 * @brief 顶升AGV 核心控制代码
 * @version V1.0.0
 * @date 2026/1/29
 */
Motor485Controller::Motor485Controller(ros::NodeHandle& nh)
    : nh_(nh),
      limit1_state_(false),  // 初始未触发
      limit2_state_(false)   // 初始未触发
{

    nh_.param<std::string>("serial_port", port_, "/dev/ttyUSB1");
    nh_.param<int>("baudrate", baudrate_, 9600);
    nh_.param<int>("serial_timeout_ms", timeout_ms_, 1000);
    nh_.param<int>("limit_read_interval_ms", read_interval_ms_, 200);
}


Motor485Controller::~Motor485Controller()
{
    // 关闭串口
    if (ser_.isOpen())
    {
        ser_.close();
        ROS_INFO("串口已关闭：%s", port_.c_str());
    }
}


bool Motor485Controller::init()
{
    // 1. 配置串口参数
    ser_.setPort(port_);
    ser_.setBaudrate(baudrate_);
    serial::Timeout timeout = serial::Timeout::simpleTimeout(timeout_ms_);
    ser_.setTimeout(timeout);
    serial::bytesize_t bytesize = serial::eightbits;    // 8数据位
    serial::parity_t parity = serial::parity_none;      // 0校验位
    serial::stopbits_t stopbits = serial::stopbits_two; // 2停止位
    ser_.setBytesize(bytesize);
    ser_.setParity(parity);
    ser_.setStopbits(stopbits);

    // 2. 打开串口
    try
    {
        ser_.open();
    }
    catch (const std::exception& e)
    {
        ROS_ERROR_STREAM("串口打开失败：" << port_ << "，错误信息：" << e.what());
        return false;
    }

    if (!ser_.isOpen())
    {
        ROS_ERROR("串口打开失败：端口未正常初始化");
        return false;
    }
    ROS_INFO_STREAM("串口打开成功：" << port_);

    // 3. 创建ROS话题订阅/发布/定时器
    speed_sub_ = nh_.subscribe<std_msgs::Float32>("/motor/speed_percent", 10, &Motor485Controller::speedPercentCallback, this);
    limit_pub_ = nh_.advertise<lifting_ros::limitation_state>("/motor/limit_status", 10);
    limit_timer_ = nh_.createTimer(ros::Duration(read_interval_ms_ / 1000.0), &Motor485Controller::readLimitTimerCallback, this);
    queue_timer_ = nh_.createTimer(ros::Duration(1),&Motor485Controller::queueTimerCallback, this);

    return true;
}

uint16_t Motor485Controller::crc16Modbus(const uint8_t* data, size_t len)
{

    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= static_cast<uint16_t>(data[i]);
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void Motor485Controller::splitCrc16(uint16_t crc, uint8_t& crc_high, uint8_t& crc_low)
{
    crc_high = static_cast<uint8_t>((crc >> 8) & 0xFF); // 高8位
    crc_low = static_cast<uint8_t>(crc & 0xFF);         // 低8位
    
}

std::vector<uint8_t> Motor485Controller::assembleControlCmd(float speed_percent)
{
    std::vector<uint8_t> cmd;
    float clipped_percent;
    uint16_t duty_cycle;
    clipped_percent = speed_percent;
    if (clipped_percent >= 0.0f) //0-100% -> 0-1000,放大10倍
    {
        duty_cycle = static_cast<uint16_t>(clipped_percent * 10.0);
    }
    else
    {
        duty_cycle = 0xFFFF - static_cast<uint16_t>(fabsf(clipped_percent) * 10.0);
    }

    cmd.push_back(DEVICE_ADDR_);               
    cmd.push_back(0x06);                        
    cmd.push_back(static_cast<uint8_t>((CTRL_REG_ >> 8) & 0xFF)); // 00（控制寄存器高）
    cmd.push_back(static_cast<uint8_t>(CTRL_REG_ & 0xFF));        // 42（控制寄存器低）
    cmd.push_back(static_cast<uint8_t>((duty_cycle >> 8) & 0xFF)); // 数据高
    cmd.push_back(static_cast<uint8_t>(duty_cycle & 0xFF));        // 数据低

    uint8_t crc_high, crc_low;
    uint16_t crc = crc16Modbus(cmd.data(), cmd.size());
    splitCrc16(crc, crc_high, crc_low);
    cmd.push_back(crc_low);
    cmd.push_back(crc_high);
    return cmd;
}


std::vector<uint8_t> Motor485Controller::assembleReadLimitCmd(uint16_t reg_addr)
{
    std::vector<uint8_t> cmd;
    cmd.push_back(DEVICE_ADDR_);                // 01
    cmd.push_back(0x03);                        // 03（读保持寄存器）
    cmd.push_back(static_cast<uint8_t>((reg_addr >> 8) & 0xFF)); // 寄存器地址高
    cmd.push_back(static_cast<uint8_t>(reg_addr & 0xFF));        // 寄存器地址低
    cmd.push_back(0x00);                        // 读取寄存器个数高（固定1个）
    cmd.push_back(0x01);                        // 读取寄存器个数低（固定1个）

    // 计算CRC并添加到末尾
    uint8_t crc_high, crc_low;
    uint16_t crc = crc16Modbus(cmd.data(), cmd.size());
    splitCrc16(crc, crc_high, crc_low);
    cmd.push_back(crc_low);
    cmd.push_back(crc_high);
    return cmd;
}

bool Motor485Controller::parseLimitFeedback(const std::vector<uint8_t>& recv_data, uint16_t reg_addr, bool& limit_state)
{
    // 限位反馈固定8字节，格式：01 03 02 00 01 79 84

    if (recv_data.size() != 7)
    {
        return false;
    }
    // 校验2：设备地址和功能码是否正确
    if (recv_data[0] != DEVICE_ADDR_ || recv_data[1] != 0x03)
    {
        return false;
    }
    uint16_t calc_crc = crc16Modbus(recv_data.data(), 5);
    uint16_t buf_crc = ((static_cast<uint8_t>(calc_crc & 0xFF)) << 8) | (static_cast<uint8_t>((calc_crc >> 8) & 0xFF));
    uint16_t recv_crc = (static_cast<uint16_t>(recv_data[5]) << 8) | recv_data[6];
    if (buf_crc != recv_crc)
    {
        return false;
    }
    uint16_t data = (static_cast<uint16_t>(recv_data[3]) << 8) | recv_data[4];

    limit_state = ~data & 0x01;

    
    return true;
}

bool Motor485Controller::sendSerialData(const std::vector<uint8_t>& send_data)
{
  
    if (!ser_.isOpen())
    {
        return false;
    }
    try
    {
        size_t send_len = ser_.write(send_data);
        if (send_len != send_data.size())
        {


            return false;
        }
    }
    catch (const std::exception& e)
    {
        return false;
    }
    return true;
}

bool Motor485Controller::recvSerialData(std::vector<uint8_t>& recv_data, size_t expect_len)
{
    std::lock_guard<std::mutex> lock(ser_mutex_); // 加锁：接收时不发送
    std::vector<uint8_t> usRecv_data;
    recv_data.clear();
    std::vector<uint8_t> buf_data;
    if (!ser_.isOpen())
    {
        return false;
    }
    try
    {
        int len = ser_.available();
        if(len >= expect_len)
        {
        // 读取指定长度的字节：返回std::string，需转换为vector<uint8_t> 
        size_t recv_str = ser_.read(usRecv_data,expect_len);
        if(recv_str == expect_len)
        {
            
            if(usRecv_data[0] != 0x01 && usRecv_data[1] != 0x03)
            {
                usRecv_data.clear();
                ser_.read(buf_data,200);
            }
            else
            {
                recv_data = usRecv_data;
                
            }
        }

        }
    }
    catch (const std::exception& e)
    {
        ROS_ERROR_STREAM("串口接收异常：" << e.what());
        return false;
    }
    return true;
}

void Motor485Controller::speedPercentCallback(const std_msgs::Float32::ConstPtr& msg)
{
    // 话题值映射：如0.52→52%，-0.3→-30%，先×100转换为百分比
    inverse_flag = true;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    float speed_percent = -msg->data;
    // 组装指令并发送
    std::vector<uint8_t> cmd = assembleControlCmd(speed_percent);

    cmd_queue_.push(cmd);

}
void Motor485Controller::queueTimerCallback(const ros::TimerEvent& event)
{
    //定时填充数据
    std::lock_guard<std::mutex> lock(queue_mutex_);
    std::vector<uint8_t> cmd1 = assembleReadLimitCmd(LIMIT1_REG_);
    cmd_queue_.push(cmd1);
    std::vector<uint8_t> cmd2 = assembleReadLimitCmd(LIMIT2_REG_);
    cmd_queue_.push(cmd2);

}
void Motor485Controller::readLimitTimerCallback(const ros::TimerEvent& event)
{

        //std::cout<<"queue size = "<<cmd_queue_.size()<< std::endl;
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::vector<uint8_t> recv_data;
        // 1. 读取限位1（寄存器002C）
       if (!cmd_queue_.empty()) 
       {
            std::vector<uint8_t>cmd = cmd_queue_.front(); 
            cmd_queue_.pop();
            if(cmd[0] == 0x01&&cmd[1] == 0x03)
            {
                if(cmd[3] == 0x2c)
                {
                    if (sendSerialData(cmd) && recvSerialData(recv_data, 7))
                    {

                        parseLimitFeedback(recv_data, LIMIT1_REG_, limit1_state_);
                    }
                }
                else if(cmd[3] == 0x2d)
                {
                    if (sendSerialData(cmd) && recvSerialData(recv_data, 7))
                        {
                            parseLimitFeedback(recv_data, LIMIT2_REG_, limit2_state_);
                        }                
                }
            }   
            else
            {
                sendSerialData(cmd);
            }
        }
        else
        {
        // std::cout<< " queue none "<< std::endl;
        }   
    // 3. 发布限位状态话题
    lifting_ros::limitation_state limit_msg;
     limit_msg.upper_limit_state = limit1_state_;
     limit_msg.low_limit_state = limit2_state_;
     if(limit1_state_ == false && limit2_state_ == false)
     {
        limit_msg.run_state = true;
     }else
     {
        limit_msg.run_state = false;
     }
     
     limit_pub_.publish(limit_msg);
}
int main(int argc, char** argv)
{
    // 1. 初始化ROS节点
    setlocale(LC_ALL, "");
    ros::init(argc, argv, "motor_485_control_node");
    ros::NodeHandle nh("~"); // 私有节点句柄，方便读取参数

    // 2. 实例化485电机控制类
    Motor485Controller motor_controller(nh);

    // 3. 初始化串口和ROS通信
    if (!motor_controller.init())
    {
        return -1;
    }
    // 4. 自旋运行ROS节点
    ros::spin();

    return 0;
}