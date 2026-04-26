# AGV底盘 相关 ROS 包

ROS包 话题参数说明

## ROS包信息

包名: agv <br>
程序名: agv_node <br>
节点名: agv_node

## 话题

发布话题名称: agv_info <br>
发布话题数据: agv_msgs::agv <br>
发布话题说明: AGV数据

发布话题名称: agv_error_info <br>
发布话题数据: agv_msgs::agv_error <br>
发布话题说明: AGV异常

发布话题名称: agv_param_info <br>
发布话题数据: agv_msgs::agv_param <br>
发布话题说明: AGV参数

发布话题名称: agv_drive_info <br>
发布话题数据: agv_msgs::agv_drive <br>
发布话题说明: AGV驱动器数据

发布话题名称: agv_dt_info <br>
发布话题数据: agv_msgs::agv_dt <br>
发布话题说明: AGV原始数据

发布话题名称: agv_odom_info <br>
发布话题数据: nav_msgs::Odometry <br>
发布话题说明: AGV里程消息

---

订阅话题名称: /agv_velocity <br>
订阅话题数据: geometry_msgs::Twist <br>
订阅话题说明: 底盘控制

订阅话题名称: /agv_odom_clean <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 里程计清零, [1: 清除]

订阅话题名称: /agv_fault_clean <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 故障清除, [1: 驱动器] [2: 前碰撞] [3: 后碰撞]

订阅话题名称: /agv_go_charge <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 回充开关, [1: 红外回充] [2: 激光回充] [0: 关闭回充]

订阅话题名称: /agv_stop_ctrl <br>
订阅话题数据: std_msgs::UInt8 <br>
订阅话题说明: 急停开关, [1: 打开] [0: 关闭]

## 参数

参数名: agv_port <br>
数据类型: string <br>
默认值: /dev/ttyUSB0 <br>
参数说明: 底盘通讯接口

参数名: agv_baudrate <br>
数据类型: int <br>
默认值: 115200 <br>
参数说明: 底盘通讯波特率

参数名: agv_state_display <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 底盘状态输出开关

参数名: agv_odom_enable <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 底盘里程计开关

参数名: agv_info_display <br>
数据类型: bool <br>
默认值: false <br>
参数说明: 底盘信息输出开关

参数名: agv_info_display_time <br>
数据类型: int <br>
默认值: 2 <br>
参数说明: 底盘信息输出时间间隔, 单位s

## 版本信息

当前: V 2.1.2 <br>

V 2.1.2 <br>
++修复了发送延迟的bug

V 2.1.1 <br>
++修复了设置回充命令未下发bug

V 2.1.0 <br>
++修改时间间隔函数取值方式 linux -> ros <br>
++修改了ros话题发布的位置 outside -> inside

V 2.0.1 <br>
++修复了底盘静止时，发布数据异常跳动问题。 <br>
++修复了发布话题频率异常问题。 <br>
++修复了故障清除订阅未能清除故障的bug

## 其他
