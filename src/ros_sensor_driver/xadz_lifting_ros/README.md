# 升降控制 ROS 包

ROS包 话题参数说明

## ROS包信息

包名: xadz_lifting_ros <br>
程序名: xadz_lifting_ros_node <br>
节点名: xadz_lifting_ros_node

## 话题

发布话题名称: /lifting/mode_info <br>
发布话题数据: std_msgs::UInt8 <br>
发布话题说明: 当前升降模组的工作模式，0:速度模式  1:位置模式。

发布话题名称: /lifting/limit_up_info <br>
发布话题数据: std_msgs::Bool <br>
发布话题说明: 升降模组上限位触发状态，false:未触发  true:触发。

发布话题名称: /lifting/limit_down_info <br>
发布话题数据: std_msgs::Bool <br>
发布话题说明: 升降模组下限位触发状态，false:未触发  true:触发。

发布话题名称: /lifting/motor_drive_error_info <br>
发布话题数据: std_msgs::Bool <br>
发布话题说明: 升降模组驱动器错误触发状态，false:无错误  true:有错误。

发布话题名称: /lifting/abs_encoder_online_info <br>
发布话题数据: std_msgs::Bool <br>
发布话题说明: 升降模组绝对值编码器在线状态，false:离线  true:在线。

发布话题名称: /lifting/position_action_info <br>
发布话题数据: std_msgs::Bool <br>
发布话题说明: 升降模组位置模式当前动作，0:空闲  1:下降完成  2:上升完成  3:正在下降  4:正在上升。

发布话题名称: /lifting/position_target_info <br>
发布话题数据: std_msgs::Int16 <br>
发布话题说明: 升降模组位置模式期望到达位置，单位mm。

发布话题名称: /lifting/position_real_info <br>
发布话题数据: std_msgs::Int16 <br>
发布话题说明: 升降模组位置模式实际到达位置，单位mm。

发布话题名称: /lifting/velocity_target_info <br>
发布话题数据: std_msgs::Int8 <br>
发布话题说明: 升降模组速度模式期望速度百分比，正数上升，负数下降，零停止，单位%。

发布话题名称: /lifting/velocity_real_info <br>
发布话题数据: std_msgs::Int8 <br>
发布话题说明: 升降模组速度模式实际速度百分比，正数上升，负数下降，零停止，单位%。

发布话题名称: /lifting/frame <br>
发布话题数据: std_msgs::UInt8MultiArray <br>
发布话题说明: 解析后得到的帧数组

---

订阅话题名称: /lifting/velocity_mode_ctrl <br>
订阅话题数据: std_msgs::Int8 <br>
订阅话题说明: 速度模式期望速度百分比。值范围，-100到100。正数上升，负数下降，零停止。

订阅话题名称: /lifting/position_mode_ctrl <br>
订阅话题数据: std_msgs::Int16MultiArray <br>
订阅话题说明: 位置模式速度位置控制。data.at(0)，前往位置时的期望速度百分比，值范围0到100。data.at(1)，期望前往的位置，单位mm。

订阅话题名称: /lifting/position_zero_set <br>
订阅话题数据: std_msgs::Empty <br>
订阅话题说明: 把当前位置设为零值。

## 参数

参数名: lifting_port <br>
数据类型: string <br>
默认值: /dev/ttyUSB0 <br>
参数说明: 通讯接口

参数名: lifting_baudrate <br>
数据类型: int <br>
默认值: 115200 <br>
参数说明: 通讯波特率

参数名: lifting_log_display <br>
数据类型: bool <br>
默认值: true <br>
参数说明: 日志信息显示开关

参数名: lifting_original_display <br>
数据类型: bool <br>
默认值: false <br>
参数说明: 原始数据输出开关

## 版本信息

当前: V 1.0.0 <br>

V 1.0.0 <br>
++首次创建

## 其他
