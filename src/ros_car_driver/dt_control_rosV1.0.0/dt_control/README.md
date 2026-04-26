# DT系列底盘 相关 ROS 包

ROS包 话题参数说明

## ROS包信息

包名: dt_control <br>
程序名: dt_control_node <br>
节点名: dt_control_node

## 话题

发布话题名称: /dt/buff_info <br>
发布话题数据: dt_control_msgs::dt_buff <br>
发布话题说明: 原始数据

发布话题名称: /dt/charge_info <br>
发布话题数据: dt_control_msgs::dt_charge <br>
发布话题说明: 回充状态

发布话题名称: /dt/current_info <br>
发布话题数据: dt_control_msgs::dt_current <br>
发布话题说明: 电机电流

发布话题名称: /dt/date_info <br>
发布话题数据: dt_control_msgs::dt_date <br>
发布话题说明: 出货日期

发布话题名称: /dt/drive_error_info <br>
发布话题数据: dt_control_msgs::dt_drive_error <br>
发布话题说明: 驱动器错误

发布话题名称: /dt/hardware_version_info <br>
发布话题数据: dt_control_msgs::dt_hardware_version <br>
发布话题说明: 硬件版本

发布话题名称: /dt/parameter_info <br>
发布话题数据: dt_control_msgs::dt_parameter <br>
发布话题说明: 底盘参数

发布话题名称: /dt/remote_ctrl_info <br>
发布话题数据: dt_control_msgs::dt_remote_ctrl <br>
发布话题说明: 遥控数据

发布话题名称: /dt/software_version_info <br>
发布话题数据: dt_control_msgs::dt_software_version <br>
发布话题说明: 软件版本

发布话题名称: /dt/state_info <br>
发布话题数据: dt_control_msgs::dt_state <br>
发布话题说明: 底盘状态

发布话题名称: /dt/odom_info <br>
发布话题数据: nav_msgs::Odometry <br>
发布话题说明: 里程信息

---

订阅话题名称: /dt/velocity_ctrl <br>
订阅话题数据: geometry_msgs::Twist <br>
订阅话题说明: 线速角速控制

订阅话题名称: /dt/stop_ctrl <br>
订阅话题数据: std_msgs::Bool <br>
订阅话题说明: 底盘急停控制, [true: 打开] [false: 关闭]

订阅话题名称: /dt/collision_clean <br>
订阅话题数据: std_msgs::Bool <br>
订阅话题说明: 碰撞状态清除, [true: 清除]

订阅话题名称: /dt/fault_clean <br>
订阅话题数据: std_msgs::Bool <br>
订阅话题说明: 故障尝试清除, [true: 清除]

订阅话题名称: /dt/charge_ctrl <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 回充开关, [0: 关闭回充] [1: 开启红外回充] [2: 开启激光回充]

订阅话题名称: /dt/odom_clean <br>
订阅话题数据: std_msgs::Bool <br>
订阅话题说明: 里程计清零, [true: 清除]

## 参数

参数名: dt_port <br>
数据类型: string <br>
默认值: /dev/ttyUSB0 <br>
参数说明: 通讯接口

参数名: dt_baudrate <br>
数据类型: int <br>
默认值: 115200 <br>
参数说明: 通讯波特率

参数名: dt_odom_enable <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 里程计开关

参数名: dt_log_display <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 日志信息显示开关

参数名: dt_original_display <br>
数据类型: bool <br>
默认值: false <br>
参数说明: 原始数据输出开关

参数名: dt_info_display <br>
数据类型: bool <br>
默认值: false <br>
参数说明: 运动信息输出开关

参数名: dt_info_display_time <br>
数据类型: int <br>
默认值: 2 <br>
参数说明: 运动信息输出时间间隔, 单位s

## 版本信息

当前: V 1.0.1 <br>

V 1.0.1 <br>
++更改底盘状态为状态标志位和错误标志位

V 1.0.0 <br>
++首次创建

## 其他
