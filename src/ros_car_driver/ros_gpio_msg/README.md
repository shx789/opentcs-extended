gpio_pin1对应K1, gpio_pin2对应K2, gpio_pin3对应K3, gpio_pin4对应K4
direction是方向，in为输入，out为输出
输入默认为1

修改好launch文件，直接终端运行命令 roslaunch ros_gpio_msg ros_gpio_msg.launch 
订阅话题/gpio_msg ，0为低电平；1为高电平;终端运行命令 :rostopic echo /gpio_msg 


修改Kx的高低电平，发布/set_gpio，Kx为out才行，0为低电平；1为高电平.终端运行命令:
rostopic pub /set_gpio ros_gpio_msg/set_gp "K1: 0
K2: 0
K3: 0
K4: 1" 

