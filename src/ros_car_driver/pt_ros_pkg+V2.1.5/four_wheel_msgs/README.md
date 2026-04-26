# 四轮四驱底盘 相关 ROS 包

ROS包 消息文件说明

## four_wheel.msg

四驱底盘 通用数据包

| 类型      | 名称                  | 取值         | 单位      | 备注    |
|---------|-----------------------|--------------|----------|---------|
| float64 | linear\_velocity      | \-\-\-\-     | m/s      | 线速度   |
| float64 | angular\_velocity     | \-\-\-\-     | rad/s    | 角速度   |
| int16   | front\_left\_speed    | \-\-\-\-     | mm/s     | 前左轮速 |
| int16   | front\_right\_speed   | \-\-\-\-     | mm/s     | 前右轮速 |
| int16   | back\_left\_speed     | \-\-\-\-     | mm/s     | 后左轮速 |
| int16   | back\_right\_speed    | \-\-\-\-     | mm/s     | 后右轮度 |
| float32 | front\_left\_current  | \-\-\-\-     | A        | 前左电流 |
| float32 | front\_right\_current | \-\-\-\-     | A        | 前右电流 |
| float32 | back\_left\_current   | \-\-\-\-     | A        | 后左电流 |
| float32 | back\_right\_current  | \-\-\-\-     | A        | 后右电流 |
| float32 | front\_voltage        | \-\-\-\-     | V        | 前电压   |
| float32 | back\_voltage         | \-\-\-\-     | V        | 后电压   |
| float32 | voltage               | \-\-\-\-     | V        | 主板电压 |
| bool    | stop\_button          | true / false | \-\-\-\- | 急停按钮 |
| bool    | software\_stop        | true / false | \-\-\-\- | 软件急停 |
| bool    | remote\_ctrl\_stop    | true / false | \-\-\-\- | 遥控急停 |
| bool    | front\_collision      | true / false | \-\-\-\- | 前碰撞   |
| bool    | back\_collision       | true / false | \-\-\-\- | 后碰撞   |
| bool    | charge\_enable        | true / false | \-\-\-\- | 回充使能 |
| uint8   | charge\_state         | 0 \-> 7      | \-\-\-\- | 回充状态 |
| bool    | remote\_ctrl\_error   | true / false | \-\-\-\- | 遥控异常 |
| bool    | voltage\_error        | true / false | \-\-\-\- | 电压异常 |
| bool    | drive\_error          | true / false | \-\-\-\- | 驱动异常 |

## four_wheel_pt.msg

四轮四驱底盘 原始数据

| 类型       | 名称                  | 取值      | 单位     | 备注         |
|-----------|----------------------|----------|----------|--------------|
| uint8\[\] | cmd\_81\_state       | \-\-\-\- | \-\-\-\- | 状态数据      |
| uint8\[\] | cmd\_87\_software    | \-\-\-\- | \-\-\-\- | 软件版本      |
| uint8\[\] | cmd\_88\_hardware    | \-\-\-\- | \-\-\-\- | 硬件版本      |
| uint8\[\] | cmd\_89\_param       | \-\-\-\- | \-\-\-\- | 底盘参数      |
| uint8\[\] | cmd\_8A\_date        | \-\-\-\- | \-\-\-\- | 出货日期      |
| uint8\[\] | cmd\_B0\_drive\_full | \-\-\-\- | \-\-\-\- | 驱动器完整数据 |
| uint8\[\] | cmd\_B1\_drive\_base | \-\-\-\- | \-\-\-\- | 驱动器基础数据 |

## four_wheel_param.msg

四驱底盘 通用参数数据包

| 类型     | 名称           | 取值      | 单位      | 备注   |
|--------|-----------------|----------|----------|-------|
| uint16 | wheel\_base     | \-\-\-\- | mm       | 轴距   |
| uint16 | track\_width    | \-\-\-\- | mm       | 轮距   |
| uint16 | wheel\_diameter | \-\-\-\- | mm       | 轮径   |
| uint16 | gear\_ratio     | \-\-\-\- | \-\-\-\- | 减速比  |
| uint16 | encoder\_line   | \-\-\-\- | \-\-\-\- | 光编线数 |

## four_wheel_error.msg

四驱底盘 通用错误数据包

| 类型  | 名称                       | 取值          | 单位      | 备注       |
|------|----------------------------|--------------|----------|------------|
| bool | remote\_ctrl\_lost         | true / false | \-\-\-\- | 遥控掉线    |
| bool | front\_drive\_lost         | true / false | \-\-\-\- | 前驱动器掉线 |
| bool | back\_drive\_lost          | true / false | \-\-\-\- | 后驱动器掉线 |
| bool | front\_left\_drive\_error  | true / false | \-\-\-\- | 前左驱动异常 |
| bool | front\_right\_drive\_error | true / false | \-\-\-\- | 前右驱动异常 |
| bool | back\_left\_drive\_error   | true / false | \-\-\-\- | 后左驱动异常 |
| bool | back\_right\_drive\_error  | true / false | \-\-\-\- | 后右驱动异常 |

## four_wheel_drive.msg

四驱底盘 驱动器 数据包

| 类型                               | 名称          | 取值     | 单位      | 备注 |
|-----------------------------------|--------------|----------|----------|------|
| struct four\_wheel\_drive\_fb\_lr | front\_left  | \-\-\-\- | \-\-\-\- | 前左 |
| struct four\_wheel\_drive\_fb\_lr | front\_right | \-\-\-\- | \-\-\-\- | 前右 |
| struct four\_wheel\_drive\_fb\_lr | back\_left   | \-\-\-\- | \-\-\-\- | 后左 |
| struct four\_wheel\_drive\_fb\_lr | back\_right  | \-\-\-\- | \-\-\-\- | 后右 |
| struct four\_wheel\_drive\_fb     | front        | \-\-\-\- | \-\-\-\- | 前  |
| struct four\_wheel\_drive\_fb     | back         | \-\-\-\- | \-\-\-\- | 后  |

## four_wheel_drive_fb_lr.msg

四驱底盘 驱动器 前左 前右 后左 后右 数据包

| 类型                              | 名称          | 取值     | 单位      | 备注   |
|----------------------------------|--------------|----------|----------|--------|
| struct four\_wheel\_drive\_error | error        | \-\-\-\- | \-\-\-\- | 错误   |
| struct four\_wheel\_drive\_state | state        | \-\-\-\- | \-\-\-\- | 状态   |
| float32                          | current      | \-\-\-\- | A        | 电流   |
| float32                          | max\_current | \-\-\-\- | A        | 最大电流 |

## four_wheel_drive_fb.msg

四驱底盘 驱动器 前 后 数据包

| 类型     | 名称        | 取值      | 单位 | 备注 |
|---------|-------------|----------|-----|-----|
| float32 | temperature | \-\-\-\- | ℃  | 温度 |
| float32 | voltage     | \-\-\-\- | V   | 电压 |

## four_wheel_drive_error.msg

四驱底盘 驱动器 错误 数据包

| 类型    | 名称                  | 取值          | 单位     | 备注     |
|--------|-----------------------|--------------|----------|---------|
| bool   | under\_voltage        | true / false | \-\-\-\- | 低压故障 |
| bool   | position\_fault       | true / false | \-\-\-\- | 位置故障 |
| bool   | hall\_fault           | true / false | \-\-\-\- | 霍尔故障 |
| bool   | overcurrent           | true / false | \-\-\-\- | 过流故障 |
| bool   | overload              | true / false | \-\-\-\- | 过载故障 |
| bool   | overheating           | true / false | \-\-\-\- | 过热故障 |
| bool   | speed\_deviation      | true / false | \-\-\-\- | 速度超差 |
| bool   | free\_wheeling\_fault | true / false | \-\-\-\- | 飞车故障 |
| uint16 | error\_code           | \-\-\-\-     | \-\-\-\- | 错误码   |

## four_wheel_drive_state.msg

四驱底盘 驱动器 状态 数据包

| 类型    | 名称               | 取值          | 单位      | 备注    |
|--------|--------------------|--------------|----------|---------|
| bool   | start              | true / false | \-\-\-\- | 伺服启动 |
| bool   | running            | true / false | \-\-\-\- | 伺服运行 |
| bool   | speed\_reach       | true / false | \-\-\-\- | 速度到达 |
| bool   | position\_reach    | true / false | \-\-\-\- | 位置到达 |
| bool   | braking\_output    | true / false | \-\-\-\- | 制动输出 |
| bool   | exceeded\_overload | true / false | \-\-\-\- | 过载门槛 |
| bool   | error\_warning     | true / false | \-\-\-\- | 错误警告 |
| bool   | reverse\_stall     | true / false | \-\-\-\- | 反向堵转 |
| bool   | forward\_stall     | true / false | \-\-\-\- | 正向堵转 |
| uint16 | state\_code        | \-\-\-\-     | \-\-\-\- | 状态码   |



## 消息结构

```

four_wheel
├── [float64] linear_velocity
├── [float64] angular_velocity
├── [int16] front_left_speed
├── [int16] front_right_speed
├── [int16] back_left_speed
├── [int16] back_right_speed
├── [float32] front_left_current
├── [float32] front_right_current
├── [float32] back_left_current
├── [float32] back_right_current
├── [float32] front_voltage
├── [float32] back_voltage
├── [float32] voltage
├── [bool] stop_button
├── [bool] software_stop
├── [bool] remote_ctrl_stop
├── [bool] front_collision
├── [bool] back_collision
├── [bool] charge_enable
├── [uint8] charge_state
├── [bool] remote_ctrl_error
├── [bool] voltage_error
└── [bool] drive_error

four_wheel_pt
├── [uint8[]] cmd_81_state
├── [uint8[]] cmd_87_software
├── [uint8[]] cmd_88_hardware
├── [uint8[]] cmd_89_param
├── [uint8[]] cmd_8A_date
├── [uint8[]] cmd_B0_drive_full
└── [uint8[]] cmd_B1_drive_base

four_wheel_param
├── [uint16] wheel_base
├── [uint16] track_width
├── [uint16] wheel_diameter
├── [uint16] gear_ratio
└── [uint16] encoder_line

four_wheel_error
├── [bool] remote_ctrl_lost
├── [bool] front_drive_lost
├── [bool] back_drive_lost
├── [bool] front_left_drive_error
├── [bool] front_right_drive_error
├── [bool] back_left_drive_error
└── [bool] back_right_drive_error

four_wheel_drive
│
├── [struct four_wheel_drive_fb_lr] front_left
│   │
│   ├── [struct four_wheel_drive_error] error
│   │   │
│   │   ├── [bool] under_voltage
│   │   ├── [bool] position_fault
│   │   ├── [bool] hall_fault
│   │   ├── [bool] overcurrent
│   │   ├── [bool] overload
│   │   ├── [bool] overheating
│   │   ├── [bool] speed_deviation
│   │   └── [bool] free_wheeling_fault
│   │
│   ├── [struct four_wheel_drive_state] state
│   │   │
│   │   ├── [bool] start
│   │   ├── [bool] running
│   │   ├── [bool] speed_reach
│   │   ├── [bool] position_reach
│   │   ├── [bool] braking_output
│   │   ├── [bool] exceeded_overload
│   │   ├── [bool] error_warning
│   │   ├── [bool] reverse_stall
│   │   └── [bool] forward_stall
│   │
│   ├── [float32] current
│   └── [float32] max_current
│
├── [struct four_wheel_drive_fb_lr] front_right
│   │
│   ├── [struct four_wheel_drive_error] error
│   │   │
│   │   ├── [bool] under_voltage
│   │   ├── [bool] position_fault
│   │   ├── [bool] hall_fault
│   │   ├── [bool] overcurrent
│   │   ├── [bool] overload
│   │   ├── [bool] overheating
│   │   ├── [bool] speed_deviation
│   │   └── [bool] free_wheeling_fault
│   │
│   ├── [struct four_wheel_drive_state] state
│   │   │
│   │   ├── [bool] start
│   │   ├── [bool] running
│   │   ├── [bool] speed_reach
│   │   ├── [bool] position_reach
│   │   ├── [bool] braking_output
│   │   ├── [bool] exceeded_overload
│   │   ├── [bool] error_warning
│   │   ├── [bool] reverse_stall
│   │   └── [bool] forward_stall
│   │
│   ├── [float32] current
│   └── [float32] max_current
│
├── [struct four_wheel_drive_fb_lr] back_left
│   │
│   ├── [struct four_wheel_drive_error] error
│   │   │
│   │   ├── [bool] under_voltage
│   │   ├── [bool] position_fault
│   │   ├── [bool] hall_fault
│   │   ├── [bool] overcurrent
│   │   ├── [bool] overload
│   │   ├── [bool] overheating
│   │   ├── [bool] speed_deviation
│   │   └── [bool] free_wheeling_fault
│   │
│   ├── [struct four_wheel_drive_state] state
│   │   │
│   │   ├── [bool] start
│   │   ├── [bool] running
│   │   ├── [bool] speed_reach
│   │   ├── [bool] position_reach
│   │   ├── [bool] braking_output
│   │   ├── [bool] exceeded_overload
│   │   ├── [bool] error_warning
│   │   ├── [bool] reverse_stall
│   │   └── [bool] forward_stall
│   │
│   ├── [float32] current
│   └── [float32] max_current
│
├── [struct four_wheel_drive_fb_lr] back_right
│   │
│   ├── [struct four_wheel_drive_error] error
│   │   │
│   │   ├── [bool] under_voltage
│   │   ├── [bool] position_fault
│   │   ├── [bool] hall_fault
│   │   ├── [bool] overcurrent
│   │   ├── [bool] overload
│   │   ├── [bool] overheating
│   │   ├── [bool] speed_deviation
│   │   └── [bool] free_wheeling_fault
│   │
│   ├── [struct four_wheel_drive_state] state
│   │   │
│   │   ├── [bool] start
│   │   ├── [bool] running
│   │   ├── [bool] speed_reach
│   │   ├── [bool] position_reach
│   │   ├── [bool] braking_output
│   │   ├── [bool] exceeded_overload
│   │   ├── [bool] error_warning
│   │   ├── [bool] reverse_stall
│   │   └── [bool] forward_stall
│   │
│   ├── [float32] current
│   └── [float32] max_current
│
├── [struct four_wheel_drive_fb_msg] front
│   │
│   ├── [float32] temperature
│   └── [float32] voltage
│
└── [struct four_wheel_drive_fb_msg] back
    │
    ├── [float32] temperature
    └── [float32] voltage

```

## 版本信息

当前: V 2.1.4 <br>

V 2.1.4 <br>
++无改动

V 2.1.3 <br>
++无改动

V 2.1.2 <br>
++无改动

V 2.1.0 <br>
++修复了充电状态变量取值范围的表述错误

V 2.0.2 <br>
++无改动

V 2.0.1 <br>
++无改动