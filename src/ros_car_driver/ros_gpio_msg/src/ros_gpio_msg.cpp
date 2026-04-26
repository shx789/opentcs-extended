#include "ros_gpio_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <string>

#include <ros/ros.h>
#include "ros_gpio_msg/gpio.h"
#include "ros_gpio_msg/set_gpio.h"

#define LED_NUM 4

// GPIO 控制的相关路径
#define SYSFS_GPIO_EXPORT "/sys/class/gpio/export"
#define SYSFS_GPIO_DIR_PREFIX "/sys/class/gpio/gpio"
#define SYSFS_GPIO_VALUE_SUFFIX "/value"

// 设置 GPIO 方向
void set_gpio_direction(int gpio, const char *dir) {
    char gpio_path[50];
    sprintf(gpio_path, "%s%d/direction", SYSFS_GPIO_DIR_PREFIX, gpio);
    printf("path:%s\n",gpio_path);
    int fd = open(gpio_path, O_WRONLY);
    if (fd == -1) {
        perror("Error opening direction file");
        exit(EXIT_FAILURE);
    }
    write(fd, dir, strlen(dir));
    close(fd);
}

// 控制 GPIO 输出
void set_gpio_value(int gpio, int value) {
    char gpio_path[50];
    sprintf(gpio_path, "%s%d/value", SYSFS_GPIO_DIR_PREFIX, gpio);
    
    int fd = open(gpio_path, O_WRONLY);
    if (fd == -1) {
        perror("Error opening value file");
        exit(EXIT_FAILURE);
    }
    char str_value = value ? '1' : '0';
    write(fd, &str_value, 1);
    close(fd);
}

// 读取 GPIO 文件内容
std::string read_gpio(int gpio)
{
    std::ifstream gpio_file("/sys/class/gpio/gpio" + std::to_string(gpio) + "/value");
    std::string value;

    if (gpio_file.is_open())
    {
        gpio_file >> value;
        gpio_file.close();
    }
    else
    {
        ROS_ERROR("Failed to open GPIO file: gpio%d", gpio);
        return "";
    }

    return value;
}

void set_gpio_callback(const ros_gpio_msg::set_gpio::ConstPtr &set_gpio_msg)
{
    int leds[LED_NUM] = {116, 125, 135,27};  // 假设使用的 GPIO 引脚编号
    int  value1 , value2 , value3 , value4;
    // for (i = 0; i < LED_NUM; i++) {
        
    if(direction1 == "out")
    {
        value1 = set_gpio_msg->K1;
        set_gpio_value(116, value1);
    }

    if(direction2 == "out")
    {
        value2 = set_gpio_msg->K2;
        set_gpio_value(125, value2);    
    }
    
    if(direction3 == "out")
    {
        value3 = set_gpio_msg->K3;
        set_gpio_value(135, value3);
    }

    if(direction4 == "out")
    {
        value4 = set_gpio_msg->K4;
        set_gpio_value(27, value4);
    }
    
        
}

void release_gpio(int gpio_num)
{
    std::ofstream gpio_unexport("/sys/class/gpio/unexport");
    if (gpio_unexport.is_open()) {
        gpio_unexport << gpio_num;
        gpio_unexport.close();
        ROS_INFO("GPIO %d has been released.", gpio_num);
    } else {
        ROS_ERROR("Failed to release GPIO %d", gpio_num);
    }
}

// 检查 GPIO 是否已被导出
bool is_gpio_exported(int gpio_num) {
    std::ifstream gpio_dir("/sys/class/gpio/gpio" + std::to_string(gpio_num));
    return gpio_dir.good();
}

// 导出 GPIO
void export_gpio(int gpio_num) {
    // 如果 GPIO 已被导出，则先取消导出
    if (is_gpio_exported(gpio_num)) {
        release_gpio(gpio_num);  // 先释放 GPIO
    }

    // 导出 GPIO
    int export_fd = open(SYSFS_GPIO_EXPORT, O_WRONLY);
    if (export_fd == -1) {
        perror("Error opening export file");
        exit(EXIT_FAILURE);
    }
    char gpio_str[3];
    sprintf(gpio_str, "%d", gpio_num);
    write(export_fd, gpio_str, strlen(gpio_str));
    close(export_fd);
    ROS_INFO("GPIO %d has been exported.", gpio_num);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "publish_gpio_msg", ros::init_options::NoSigintHandler); // 解析参数
    ros::NodeHandle nh;
    ros::NodeHandle n("~");
    
    
    n.param<int>("gpio_pin1", gpio_pin1, 116);
    n.param<std::string>("direction1", direction1, "in");
    n.param<int>("gpio_pin2", gpio_pin2, 116);
    n.param<std::string>("direction2", direction2, "out");
    n.param<int>("gpio_pin3", gpio_pin3, 116);
    n.param<std::string>("direction3", direction3, "out");
    n.param<int>("gpio_pin4", gpio_pin4, 116);
    n.param<std::string>("direction4", direction4, "out");
    
    

    // signal(SIGINT, mySigIntHandler);                                                                 // 把原来ctrl+c中断函数覆盖掉，把信号槽连接到mySigIntHandler保证关闭节点

    ros_gpio_msg::gpio gpio_msg;
    ros_gpio_msg::set_gpio set_gpio_msg;
    ros::Publisher pub = nh.advertise<ros_gpio_msg::gpio>("gpio_msg", 10); // 创建 publisher 对象
    ros::Subscriber set_gpio_sub = nh.subscribe("set_gpio", 100, set_gpio_callback);

    
    int leds[LED_NUM] = {116,125,135,27};  // 假设使用的 GPIO 引脚编号
    int i;

    // 导出所有 GPIO
    for (i = 0; i < LED_NUM; i++) {
        export_gpio(leds[i]);  // 导出 GPIO
    }

    // 设置 GPIO 方向
    set_gpio_direction(gpio_pin1, direction1.c_str());
    set_gpio_direction(gpio_pin2, direction2.c_str());
    set_gpio_direction(gpio_pin3, direction3.c_str());
    set_gpio_direction(gpio_pin4, direction4.c_str());

    ros::Rate loop_rate(10);
    while (ros::ok())
    {
        // 遍历每个 GPIO 引脚并读取值
        for (int i = 0; i < LED_NUM; ++i)
        {
            int gpio = leds[i];
            std::string gpio_value = read_gpio(gpio);

            if (!gpio_value.empty())
            {
                if(i == 0)
                {
                    gpio_msg.K1 = std::stoi(gpio_value);
                }
                if(i == 1)
                {
                    gpio_msg.K2 = std::stoi(gpio_value);
                }
                if(i == 2)
                {
                    gpio_msg.K3 = std::stoi(gpio_value);
                }
                if(i == 3)
                {
                    gpio_msg.K4 = std::stoi(gpio_value);
                }

                // 发布 GPIO 值
                pub.publish(gpio_msg);
            }
        }

        ros::spinOnce();
        loop_rate.sleep();
    }
    
    return 0;
}
