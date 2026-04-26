# 四轮四驱底盘 相关 ROS 包

ROS包 话题参数说明

## ROS包信息

包名: four_wheel <br>
程序名: four_wheel_node <br>
节点名: four_wheel_node

## 话题

发布话题名称: four_wheel_info <br>
发布话题数据: four_wheel_msgs::four_wheel <br>
发布话题说明: 四轮四驱数据

发布话题名称: four_wheel_error_info <br>
发布话题数据: four_wheel_msgs::four_wheel_error <br>
发布话题说明: 四轮四驱异常

发布话题名称: four_wheel_param_info <br>
发布话题数据: four_wheel_msgs::four_wheel_param <br>
发布话题说明: 四轮四驱参数

发布话题名称: four_wheel_drive_info <br>
发布话题数据: four_wheel_msgs::four_wheel_drive <br>
发布话题说明: 四轮四驱驱动器数据

发布话题名称: four_wheel_pt_info <br>
发布话题数据: four_wheel_msgs::four_wheel_pt <br>
发布话题说明: 四轮四驱原始数据

发布话题名称: four_wheel_odom_info <br>
发布话题数据: nav_msgs::Odometry <br>
发布话题说明: 四轮四驱里程消息

---

订阅话题名称: /four_wheel_velocity <br>
订阅话题数据: geometry_msgs::Twist <br>
订阅话题说明: 底盘控制

订阅话题名称: /four_wheel_odom_clean <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 里程计清零, [1: 清除]

订阅话题名称: /four_wheel_fault_clean <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 故障清除, [1: 驱动器] [2: 前碰撞] [3: 后碰撞] [255: 全错误]

订阅话题名称: /four_wheel_go_charge <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 回充开关, [1: 打开] [0: 关闭]

订阅话题名称: /four_wheel_stop_ctrl <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 急停开关, [1: 打开] [0: 关闭]

## 参数

参数名: four_wheel_port <br>
数据类型: string <br>
默认值: /dev/ttyUSB0 <br>
参数说明: 底盘通讯接口

参数名: four_wheel_baudrate <br>
数据类型: int <br>
默认值: 115200 <br>
参数说明: 底盘通讯波特率

参数名: four_wheel_state_display <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 底盘状态输出开关

参数名: four_wheel_original_display <br>
数据类型: bool <br>
默认值: false <br>
参数说明: 底盘原始数据输出开关

参数名: four_wheel_odom_enable <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 底盘里程计开关

参数名: four_wheel_info_display <br>
数据类型: bool <br>
默认值: false <br>
参数说明: 底盘信息输出开关

参数名: four_wheel_info_display_time <br>
数据类型: int <br>
默认值: 2 <br>
参数说明: 底盘信息输出时间间隔, 单位s

## 版本信息

当前: V 2.1.5 <br>

V 2.1.5 <br>
++修复了遥控在线状态的显示bug

V 2.1.4 <br>
++增加了打印原始数据的功能

V 2.1.3 <br>
++增加了故障全部清除的命令码

V 2.1.2 <br>
++修复了发送延迟的bug

V 2.1.0 <br>
++修改时间间隔函数取值方式 linux -> ros <br>
++修改了ros话题发布的位置 outside -> inside

V 2.0.2 <br>
++修复了底盘静止时，发布数据异常跳动问题。 <br>
++修复了发布话题频率异常问题。

V 2.0.1 <br>
++修复了故障清除订阅未能清除故障的bug

## 其他
