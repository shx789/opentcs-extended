发布话题
uav_msg
内容
flag: 0       	//错误标志
voltage: 239	//电压
stop: 0		//急停状态
state: 170	//限位状态
lspeed: -5	//左舱门目标速度
rspeed: 0	//右舱门目标速度
fspeed: -60	//上夹速度
bspeed: -60	//下夹速度
lerror: 0	//左舱门电机故障码
rerror: 0	//右舱门电机故障码

订阅话题
/uav_room_vel	//控制舱门速度控制

开门
rostopic pub /uav_room_vel ros_uav_room/roopeed "lspeed: 100
rspeed: 120" 

关门
rostopic pub -r 10  /uav_room_vel ros_uav_room/roomspeed "lspeed: -110 
rspeed: -100" 

注意：由于机械结构问题，开门时先把右边的打开，关门时先把左边关上。

/uav_room_stop	//控制舱门急停控制

急停
rostopic pub /uav_room_stop ros_uav_room/roomstop "stop: 1" 

取消急停
rostopic pub /uav_room_stop ros_uav_room/roomstop "stop: 0" 

/uav_clamp_vel	//控制夹子速度控制

打开
rostopic pub -r 10 /uav_clamp_vel ros_uav_rm/clampspeed "fspeed: 1500
bspeed: 1500" 

关闭
rostopic pub -r 10 /uav_clamp_vel ros_uav_rm/clampspeed "fspeed: -1500
bspeed: -1500" 


/uav_clamp_stop	//控制夹子急停控制

急停


取消急停
rostopic pub /uav_clamp_stop ros_uav_room/cmpstop "stop: 0" 




