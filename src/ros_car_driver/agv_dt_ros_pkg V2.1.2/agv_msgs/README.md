# AGV底盘 相关 ROS 包

ROS包 消息文件说明

## agv.msg

AGV底盘 通用数据包

| 类型      | 名称                  | 取值         | 单位      | 备注    |
|---------|-----------------------|--------------|----------|---------|
| float64 | linear\_velocity      | \-\-\-\-     | m/s      | 线速度   |
| float64 | angular\_velocity     | \-\-\-\-     | rad/s    | 角速度   |
| int16   | left\_speed           | \-\-\-\-     | mm/s     | 左轮速   |
| int16   | right\_speed          | \-\-\-\-     | mm/s     | 右轮速   |
| float32 | left\_current         | \-\-\-\-     | A        | 左电流   |
| float32 | right\_current        | \-\-\-\-     | A        | 右电流   |
| float32 | drive\_voltage        | \-\-\-\-     | V        | 驱动电压 |
| float32 | voltage               | \-\-\-\-     | V        | 主板电压 |
| bool    | stop\_button          | true / false | \-\-\-\- | 急停按钮 |
| bool    | software\_stop        | true / false | \-\-\-\- | 软件急停 |
| bool    | remote\_ctrl\_stop    | true / false | \-\-\-\- | 遥控急停 |
| bool    | front\_collision      | true / false | \-\-\-\- | 前碰撞   |
| bool    | back\_collision       | true / false | \-\-\-\- | 后碰撞   |
| bool    | charge\_enable        | true / false | \-\-\-\- | 回充使能 |
| bool    | charge\_io            | true / false | \-\-\-\- | 激光回充 |
| uint8   | charge\_state         | 0 \-> 7      | \-\-\-\- | 红外回充 |
| bool    | remote\_ctrl\_error   | true / false | \-\-\-\- | 遥控异常 |
| bool    | voltage\_error        | true / false | \-\-\-\- | 电压异常 |
| bool    | drive\_error          | true / false | \-\-\-\- | 驱动异常 |

## agv_dt.msg

AGV底盘 原始数据

| 类型        | 名称                 | 取值     | 单位      | 备注         |
|-----------|----------------------|----------|----------|--------------|
| uint8\[\] | cmd\_81\_state       | \-\-\-\- | \-\-\-\- | 状态数据      |
| uint8\[\] | cmd\_87\_software    | \-\-\-\- | \-\-\-\- | 软件版本      |
| uint8\[\] | cmd\_88\_hardware    | \-\-\-\- | \-\-\-\- | 硬件版本      |
| uint8\[\] | cmd\_89\_param       | \-\-\-\- | \-\-\-\- | 底盘参数      |
| uint8\[\] | cmd\_8A\_date        | \-\-\-\- | \-\-\-\- | 出货日期      |
| uint8\[\] | cmd\_B0\_drive\_full | \-\-\-\- | \-\-\-\- | 驱动器完整信息 |
| uint8\[\] | cmd\_C0\_drive\_full | \-\-\-\- | \-\-\-\- | 轮毂完整信息   |
| uint8\[\] | cmd\_B1\_drive\_base | \-\-\-\- | \-\-\-\- | 驱动器基础信息 |
| uint8\[\] | cmd\_C1\_drive\_base | \-\-\-\- | \-\-\-\- | 轮毂基础信息  |

## agv_param.msg

AGV底盘 通用参数数据包

| 类型     | 名称           | 取值      | 单位      | 备注   |
|--------|-----------------|----------|----------|-------|
| uint16 | wheel\_base     | \-\-\-\- | mm       | 轴距   |
| uint16 | track\_width    | \-\-\-\- | mm       | 轮距   |
| uint16 | wheel\_diameter | \-\-\-\- | mm       | 轮径   |
| uint16 | gear\_ratio     | \-\-\-\- | \-\-\-\- | 减速比  |
| uint16 | encoder\_line   | \-\-\-\- | \-\-\-\- | 光编线数 |

## agv_error.msg

AGV底盘 通用错误数据包

| 类型  | 名称                | 取值          | 单位      | 备注       |
|------|---------------------|--------------|----------|------------|
| bool | remote\_ctrl\_lost  | true / false | \-\-\-\- | 遥控掉线    |
| bool | drive\_lost         | true / false | \-\-\-\- | 驱动器掉线  |
| bool | left\_drive\_error  | true / false | \-\-\-\- | 左驱动异常  |
| bool | right\_drive\_error | true / false | \-\-\-\- | 右驱动异常  |

## agv_drive.msg

AGV底盘 驱动器 数据包

| 类型                   | 名称        | 取值            | 单位      | 备注     |
|-----------------------|-------------|----------------|----------|----------|
| struct agv\_drive\_lr | left        | \-\-\-\-       | \-\-\-\- | 左       |
| struct agv\_drive\_lr | right       | \-\-\-\-       | \-\-\-\- | 右       |
| float32               | temperature | \-\-\-\-       | ℃       | 温度      |
| float32               | voltage     | \-\-\-\-       | V        | 电压      |
| string                | drive_type  | "FDK" / "SDFZ" | \-\-\-\- | 驱动器类型 |

## agv_drive_lr.msg

AGV底盘 驱动器 左 右 数据包

| 类型                      | 名称          | 取值     | 单位      | 备注   |
|--------------------------|--------------|----------|----------|--------|
| struct agv\_drive\_error | error        | \-\-\-\- | \-\-\-\- | 错误   |
| struct agv\_drive\_state | state        | \-\-\-\- | \-\-\-\- | 状态   |
| float32                  | current      | \-\-\-\- | A        | 电流   |
| float32                  | max\_current | \-\-\-\- | A        | 最大电流 |

## agv_drive_error.msg

AGV底盘 驱动器 错误 数据包

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
| bool   | encoder\_fault        | true / false | \-\-\-\- | 编码故障 |
| bool   | overvoltage           | true / false | \-\-\-\- | 过压故障 |
| bool   | short\_circuit        | true / false | \-\-\-\- | 短路故障 |
| uint16 | error\_code           | \-\-\-\-     | \-\-\-\- | 错误码   |

## agv_drive_state.msg

AGV底盘 驱动器 状态 数据包

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

agv
├── [float64] linear_velocity
├── [float64] angular_velocity
├── [int16] left_speed
├── [int16] right_speed
├── [float32] left_current
├── [float32] right_current
├── [float32] drive_voltage
├── [float32] voltage
├── [bool] stop_button
├── [bool] software_stop
├── [bool] remote_ctrl_stop
├── [bool] front_collision
├── [bool] back_collision
├── [bool] charge_enable
├── [bool] charge_io
├── [uint8] charge_state
├── [bool] remote_ctrl_error
├── [bool] voltage_error
└── [bool] drive_error

agv_dt
├── [uint8[]] cmd_81_state
├── [uint8[]] cmd_87_software
├── [uint8[]] cmd_88_hardware
├── [uint8[]] cmd_89_param
├── [uint8[]] cmd_8A_date
├── [uint8[]] cmd_B0_drive_full
├── [uint8[]] cmd_C0_drive_full
├── [uint8[]] cmd_B1_drive_base
└── [uint8[]] cmd_C1_drive_base

agv_param
├── [uint16] wheel_base
├── [uint16] track_width
├── [uint16] wheel_diameter
├── [uint16] gear_ratio
└── [uint16] encoder_line

agv_error
├── [bool] remote_ctrl_lost
├── [bool] drive_lost
├── [bool] left_drive_error
└── [bool] right_drive_error

agv_drive
│
├── [struct agv_drive_lr] left
│   │
│   ├── [struct agv_drive_error] error
│   │   │
│   │   ├── [bool] under_voltage
│   │   ├── [bool] position_fault
│   │   ├── [bool] hall_fault
│   │   ├── [bool] overcurrent
│   │   ├── [bool] overload
│   │   ├── [bool] overheating
│   │   ├── [bool] speed_deviation
│   │   ├── [bool] free_wheeling_fault
│   │   ├── [bool] encoder_fault
│   │   ├── [bool] overvoltage
│   │   ├── [bool] short_circuit
│   │   └── [uint16] error_code
│   │
│   ├── [struct agv_drive_state] state
│   │   │
│   │   ├── [bool] start
│   │   ├── [bool] running
│   │   ├── [bool] speed_reach
│   │   ├── [bool] position_reach
│   │   ├── [bool] braking_output
│   │   ├── [bool] exceeded_overload
│   │   ├── [bool] error_warning
│   │   ├── [bool] reverse_stall
│   │   ├── [bool] forward_stall
│   │   └── [uint16] state_code
│   │
│   ├── [float32] current
│   └── [float32] max_current
│
├── [struct agv_drive_lr] right
│   │
│   ├── [struct agv_drive_error] error
│   │   │
│   │   ├── [bool] under_voltage
│   │   ├── [bool] position_fault
│   │   ├── [bool] hall_fault
│   │   ├── [bool] overcurrent
│   │   ├── [bool] overload
│   │   ├── [bool] overheating
│   │   ├── [bool] speed_deviation
│   │   ├── [bool] free_wheeling_fault
│   │   ├── [bool] encoder_fault
│   │   ├── [bool] overvoltage
│   │   ├── [bool] short_circuit
│   │   └── [uint16] error_code
│   │
│   ├── [struct agv_drive_state] state
│   │   │
│   │   ├── [bool] start
│   │   ├── [bool] running
│   │   ├── [bool] speed_reach
│   │   ├── [bool] position_reach
│   │   ├── [bool] braking_output
│   │   ├── [bool] exceeded_overload
│   │   ├── [bool] error_warning
│   │   ├── [bool] reverse_stall
│   │   ├── [bool] forward_stall
│   │   └── [uint16] state_code
│   │
│   ├── [float32] current
│   └── [float32] max_current
│
├── [float32] temperature
├── [float32] voltage
└── [string] drive_type

```

## 版本信息

当前: V 2.1.0 <br>

V 2.1.0 <br>
++修复了充电状态变量取值范围的表述错误

V 2.0.1 <br>
++无改动