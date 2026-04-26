# DT系列底盘 相关 ROS 包

ROS包 消息文件说明

## dt_buff.msg

原始数据

| 类型        | 名称                         | 单位       | 备注    |
|-----------|----------------------------|----------|-------|
| uint8\[\] | cmd\_80\_state             | \-\-\-\- | 底盘状态  |
| uint8\[\] | cmd\_81\_current           | \-\-\-\- | 电机电流  |
| uint8\[\] | cmd\_82\_charge            | \-\-\-\- | 回充状态  |
| uint8\[\] | cmd\_83\_remote\_ctrl      | \-\-\-\- | 遥控数据  |
| uint8\[\] | cmd\_A0\_drvie\_error      | \-\-\-\- | 驱动器错误 |
| uint8\[\] | cmd\_A1\_parameter         | \-\-\-\- | 底盘参数  |
| uint8\[\] | cmd\_A2\_software\_version | \-\-\-\- | 软件版本  |
| uint8\[\] | cmd\_A3\_hardware\_version | \-\-\-\- | 硬件版本  |
| uint8\[\] | cmd\_A4\_date              | \-\-\-\- | 出货日期  |

## dt_charge.msg

电机电流

| 类型    | 名称       | 单位       | 备注     |
|-------|----------|----------|--------|
| uint8 | state    | \-\-\-\- | 回充当前状态 |
| bool  | relays   | \-\-\-\- | 回充头继电器 |
| bool  | button   | \-\-\-\- | 回充桩按钮  |
| uint8 | infrared | \-\-\-\- | 红外回充状态 |
| uint8 | laser    | \-\-\-\- | 激光回充状态 |

## dt_current.msg

回充状态

| 类型      | 名称    | 单位 | 备注    |
|---------|-------|----|-------|
| float32 | left  | A  | 左电机电流 |
| float32 | right | A  | 右电机电流 |

## dt_date.msg

出货日期

| 类型    | 名称    | 单位       | 备注 |
|-------|-------|----------|----|
| uint8 | year  | \-\-\-\- | 年  |
| uint8 | month | \-\-\-\- | 月  |
| uint8 | day   | \-\-\-\- | 日  |

## dt_drive_error.msg

驱动器错误

| 类型     | 名称    | 单位       | 备注   |
|--------|-------|----------|------|
| struct | left  | \-\-\-\- | 左路错误 |
| struct | right | \-\-\-\- | 右路错误 |

## dt_drive_hollysys_error.msg

驱动器错误信息

| 类型   | 名称                                                         | 单位       | 备注            |
|------|------------------------------------------------------------|----------|---------------|
| bool | encoder\_abz\_fault\_alarm                                 | \-\-\-\- | 编码器故障ABZ报警    |
| bool | encoder\_uvw\_fault\_alarm                                 | \-\-\-\- | 编码器故障UVW报警    |
| bool | position\_error                                            | \-\-\-\- | 位置超差          |
| bool | stall                                                      | \-\-\-\- | 失速            |
| bool | current\_sampling\_midpoint\_fault                         | \-\-\-\- | 电流采样（中点）故障    |
| bool | overload                                                   | \-\-\-\- | 过载            |
| bool | undervoltage                                               | \-\-\-\- | 欠压            |
| bool | overvoltage                                                | \-\-\-\- | 过压            |
| bool | overcurrent                                                | \-\-\-\- | 过流            |
| bool | discharge\_alarm\_instant\_power\_high                     | \-\-\-\- | 放电报警瞬时功率大     |
| bool | discharge\_circuit\_frequent\_action\_average\_power\_high | \-\-\-\- | 放电回路频繁动作平均功率大 |
| bool | parameter\_read\_write\_error                              | \-\-\-\- | 参数读写异常        |
| bool | input\_function\_duplicate\_definition                     | \-\-\-\- | 输入口功能定义重复     |
| bool | communication\_watchdog\_triggered                         | \-\-\-\- | 通讯看门狗触发       |
| bool | motor\_overtemperature\_alarm                              | \-\-\-\- | 电机过温报警        |

## dt_hardware_version.msg

硬件版本

| 类型         | 名称      | 单位       | 备注   |
|------------|---------|----------|------|
| uint8\[8\] | version | \-\-\-\- | 硬件版本 |

## dt_parameter.msg

底盘参数

| 类型     | 名称             | 单位   | 备注  |
|--------|----------------|------|-----|
| uint16 | track_width    | m    | 轮距  |
| uint16 | wheel_base     | m    | 轴距  |
| uint16 | wheel_diameter | m    | 直径  |
| uint16 | gear_ratio     | ---- | 减速比 |
| uint16 | encoder_line   | ---- | 线数  |

## dt_remote_ctrl.msg

遥控数据

| 类型    | 名称                | 单位       | 备注     |
|-------|-------------------|----------|--------|
| int16 | right\_rocker\_x  | \-\-\-\- | 右摇杆左右  |
| int16 | right\_rocker\_y  | \-\-\-\- | 右摇杆上下  |
| int16 | left\_rocker\_y   | \-\-\-\- | 左摇杆上下  |
| int16 | left\_rocker\_x   | \-\-\-\- | 左摇杆左右  |
| int16 | left\_round\_vra  | \-\-\-\- | 左拨轮前后  |
| int16 | right\_round\_vrb | \-\-\-\- | 右拨轮前后  |
| uint8 | key\_swa          | \-\-\-\- | 拨杆swa  |
| uint8 | key\_swb          | \-\-\-\- | 拨杆swb  |
| uint8 | key\_swc          | \-\-\-\- | 拨杆swc  |
| uint8 | key\_swd          | \-\-\-\- | 拨杆swd  |
| bool  | is\_offline       | \-\-\-\- | 遥控在线状态 |

## dt_software_version.msg

软件版本

| 类型         | 名称                | 单位       | 备注     |
|------------|-------------------|----------|--------|
| uint8      | major\_version    | \-\-\-\- | 主版本号   |
| uint8      | minor\_version    | \-\-\-\- | 次版本号   |
| uint8      | revision\_version | \-\-\-\- | 修订版本号  |
| uint8      | other\_version    | \-\-\-\- | 其他版本号  |
| uint8      | year\_version     | \-\-\-\- | 年      |
| uint8      | month\_version    | \-\-\-\- | 月      |
| uint8      | day\_version      | \-\-\-\- | 日      |
| uint8      | seq\_version      | \-\-\-\- | 序列号    |
| uint8\[8\] | version           | \-\-\-\- | 软件版本信息 |

## dt_state.msg

底盘状态

| 类型      | 名称                    | 单位       | 备注     |
|---------|-----------------------|----------|--------|
| uint8   | ctrl\_mode            | \-\-\-\- | 控制模式   |
| uint8   | battery\_percent      | %        | 电量百分比  |
| float32 | battery\_voltage      | V        | 电池电压   |
| bool    | stop\_button          | \-\-\-\- | 急停按钮   |
| bool    | remote\_ctrl\_stop    | \-\-\-\- | 遥控急停   |
| bool    | software\_stop        | \-\-\-\- | 软件急停   |
| bool    | remote\_ctrl\_offline | \-\-\-\- | 遥控掉线   |
| bool    | front\_collision      | \-\-\-\- | 前防撞杆触发 |
| bool    | back\_collision       | \-\-\-\- | 后防撞杆触发 |
| bool    | charge\_enable        | \-\-\-\- | 回充开启   |
| bool    | drive\_offline        | \-\-\-\- | 驱动器掉线  |
| bool    | drive\_error          | \-\-\-\- | 驱动器报警  |
| float64 | linear\_velocity      | m/s      | 线速度    |
| float64 | angular\_velocity     | rad/s    | 角速度    |
| int16   | left\_speed           | mm/s     | 左轮速    |
| int16   | right\_speed          | mm/s     | 右轮速    |



## 版本信息

当前: V 1.0.0 <br>

V 1.0.0 <br>
++首次创建