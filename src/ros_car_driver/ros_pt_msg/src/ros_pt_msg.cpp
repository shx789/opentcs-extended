#include <ros/ros.h>                      //类似 C 语言的 stdio.h
#include <serial/serial.h>                //ROS已经内置了的串口包
#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <std_msgs/UInt8.h> 
#include <signal.h>
#include <sensor_msgs/Imu.h>
#include <ros/duration.h>
#include <string.h>
#include <string>
#include "ros_pt_msg.h"
#include "ros_pt_control.h"
#include "ros_pt_msg/control1.h"
#include "ros_pt_msg/control2.h"
#include "ros_pt_msg/pt1.h"      //要用到 msg 中定义的数据类型
#include "ros_pt_msg/pt2.h"      //要用到 msg 中定义的数据类型
#include "ros_pt_msg/error.h"
#include <geometry_msgs/Twist.h>

#define CONTROL_MODE 1  //传输模式选择1：轮速模式，2：线速模式

serial::Serial ser; //声明串口对象

void open20ms( u8 data)
{
    four_OpenRobot20ms.prot.Header = HEADER;
    four_OpenRobot20ms.prot.Len = 0x0A;
    four_OpenRobot20ms.prot.Type = 0x04;
    four_OpenRobot20ms.prot.Cmd = 0x01;
    four_OpenRobot20ms.prot.Num = 0x01;
    four_OpenRobot20ms.prot.Data = data;

    switch (four_OpenRobot20ms.prot.Data )
    {
    case 0:
        printf("close20ms\n");
        break;
    case 1:
        printf("open20ms1\n");
        break;
    case 2:
        printf("open20ms2\n");
        break;
    default:
        break;
    }

    for(int i = 0;i < sizeof(four_OpenRobot20ms) - 2;i++)
    {
        four_OpenRobot20ms.prot.Check += four_OpenRobot20ms.data[i];
    }
    ser.write(four_OpenRobot20ms.data,sizeof(four_OpenRobot20ms));
}

/*************************************************************************/
//当关闭包时调用，关闭
void mySigIntHandler(int sig)
{
   ROS_INFO("close the com serial!\n");
   open20ms(0);
   sleep(1);
   ser.close();
   ros::shutdown();
}

/*************************************************************************/
#define Base_Width 1100  //轴距
void cmd_velCallback(const geometry_msgs::Twist &twist_aux)
{
    s16 TempLSpeed=0,TempRSpeed=0;
    TempLSpeed = twist_aux.linear.x*1000 -  twist_aux.angular.z*Base_Width/2.0;
    TempRSpeed = twist_aux.linear.x*1000 + twist_aux.angular.z*Base_Width/2.0;

    pt_control1(TempLSpeed,TempRSpeed,TempLSpeed,TempRSpeed,0);
}
/*************************************************************************/
/*************************************************************************/

void pt_control1(s16 flspeed,s16 frspeed,s16 blspeed,s16 brspeed,s16 stop)
{
    memset(four_TXRobotData1.data, 0, sizeof(four_TXRobotData1.data));

    four_TXRobotData1.prot.Header  = HEADER;
    four_TXRobotData1.prot.Len     = 0x12;
    four_TXRobotData1.prot.Type    = 0x04;
    four_TXRobotData1.prot.Cmd     = 0x02;
    four_TXRobotData1.prot.Num     = 0x05;
    four_TXRobotData1.prot.FLSpeed = flspeed;
    four_TXRobotData1.prot.FRSpeed = frspeed;
    four_TXRobotData1.prot.BLSpeed = blspeed;
    four_TXRobotData1.prot.BRSpeed = brspeed;
    four_TXRobotData1.prot.StopCon = stop;
    four_TXRobotData1.prot.Check   = 0;
    for(u8 i=0;i<sizeof(four_TXRobotData1.data)- 2;i++)
    {
        four_TXRobotData1.prot.Check += four_TXRobotData1.data[i];
    }
    ser.write(four_TXRobotData1.data,sizeof(four_TXRobotData1.data));

}

void pt_control2(s16 Vx,float Vz,s16 stop)
{
    memset(four_TXRobotData2.data, 0, sizeof(four_TXRobotData2.data));

    four_TXRobotData2.prot.Header  = HEADER;
    four_TXRobotData2.prot.Len     = 0x12;
    four_TXRobotData2.prot.Type    = 0x04;
    four_TXRobotData2.prot.Cmd     = 0x02;
    four_TXRobotData2.prot.Num     = 0x03;
    four_TXRobotData2.prot.Vx = Vx;
    four_TXRobotData2.prot.Vz = Vz * 100;
    four_TXRobotData2.prot.temp1 = 0x00;
    four_TXRobotData2.prot.temp2 = 0x00;
    four_TXRobotData2.prot.StopCon = stop;
    four_TXRobotData2.prot.Check   = 0;
    for(u8 i=0;i<sizeof(four_TXRobotData2.data)- 2;i++)
    {
        four_TXRobotData2.prot.Check += four_TXRobotData2.data[i];
    }

    ser.write(four_TXRobotData2.data,sizeof(four_TXRobotData2.data));
}

void pt_control_callback1(const ros_pt_msg::control1::ConstPtr& control_msg)
{
    ROS_INFO("---write---\nflspeed:%d,frspeed%d,blspeed:%d,brspeed:%d,stop:%d",control_msg->flspeed,control_msg->frspeed,control_msg->blspeed,control_msg->brspeed,control_msg->stop);
    pt_control1(control_msg->flspeed,control_msg->frspeed,control_msg->blspeed,control_msg->brspeed,control_msg->stop);

}

void pt_control_callback2(const ros_pt_msg::control2::ConstPtr& control_msg)
{    
    ROS_INFO("---write---\nvx:%d,vz:%f,stop:%d",control_msg->vx,control_msg->vz,control_msg->stop);
    pt_control2(control_msg->vx,control_msg->vz,control_msg->stop);
  
}

void pt_error_clear()
{
    u8 data[10] = {0xED,0xDE,0x0A,0x04,0x06 ,0x00,0x00,0x00,0xDF,0x01};
    ser.write(data,10);
}

void pt_error_clear_callback(const ros_pt_msg::error::ConstPtr& error_msg)
{
    ROS_INFO("error_clear");
    if(error_msg->error == 1)
    {
        pt_error_clear();
    }
}

void openGoCharge(u8 data)
{
    four_OpenGoCharge.prot.Header = HEADER;
    four_OpenGoCharge.prot.Len = 0x0A;
    four_OpenGoCharge.prot.Type = 0x04;
    four_OpenGoCharge.prot.Cmd = 0x03;
    four_OpenGoCharge.prot.Num = 0x01;
    four_OpenGoCharge.prot.Data = data;
    four_OpenGoCharge.prot.Check = 0;

    for(int i = 0;i < sizeof(four_OpenGoCharge) - 2;i++)
    {
        four_OpenGoCharge.prot.Check += four_OpenGoCharge.data[i];
    }
    ser.write(four_OpenGoCharge.data,sizeof(four_OpenGoCharge));

    switch (data)
    {
    case 0:
        printf("close go_charge\n");
        break;
    case 1:
        printf("open go_charge\n");
        break;
    default:
        break;
    }
}

void go_charge_callback(const std_msgs::UInt8 status)
{
    openGoCharge(status.data);
}

int main(int argc,char **argv)
{
   u16 len = 0;
   u8  data[200];
   std::string usart_port;
   int baud_data;

   ros::init(argc,argv,"publish_pt_msg",ros::init_options::NoSigintHandler);            //解析参数 
   
   ros_pt_msg::error error_msg;
   ros::NodeHandle nh;
   ros::NodeHandle n("~");
   #if CONTROL_MODE ==1
    ros_pt_msg::pt1 pt_msg;
    ros::Publisher pub = nh.advertise<ros_pt_msg::pt1>("PT_Robot_info",1);          //创建一个publisher对象，发布名为PT_Robot_info，队列长度为1，消息类型为ros_pt_msg::pt。
    ros_pt_msg::control1 control_msg;
    ros::Subscriber  pt_control_sub = nh.subscribe("/PT_Control",100,pt_control_callback1);
   #elif CONTROL_MODE ==2
    ros_pt_msg::pt2 pt_msg;
    ros_pt_msg::control2 control_msg;
    ros::Publisher pub = nh.advertise<ros_pt_msg::pt2>("PT_Robot_info",1);          //创建一个publisher对象，发布名为PT_Robot_info，队列长度为1，消息类型为ros_pt_msg::pt。
    ros::Subscriber  pt_control_sub = nh.subscribe("/PT_Control",100,pt_control_callback2);
   #endif
    n.param<std::string>("usart_port", usart_port, "/dev/ttyS0"); 
    n.param<int>("baud_data", baud_data, 115200); 
   
   ros::Subscriber  pt_error_sub      = nh.subscribe("/PT_Error",100,pt_error_clear_callback);
   ros::Subscriber  go_charge_sub = nh.subscribe("go_charge",100,go_charge_callback);
   ros::Subscriber cmd_vel_sub      = nh.subscribe("cmd_vel",1,cmd_velCallback);  

   signal(SIGINT, mySigIntHandler);  											//把原来ctrl+c中断函数覆盖掉，把信号槽连接到mySigIntHandler保证关闭节点

   try
    {
      //设置串口属性，并打开串口
      ser.setPort(usart_port);
      ser.setBaudrate(baud_data);
      serial::Timeout to = serial::Timeout::simpleTimeout(500);
      ser.setTimeout(to);
      ser.open();
    }
    catch (serial::IOException& e)
    {
        ROS_ERROR_STREAM("Unable to open port ");
        return -1;
    }
    //检测串口是否已经打开，并给出提示信息
    if(ser.isOpen())
    {
        ser.flushInput();      //清空输入缓存,把多余的无用数据删除
        ROS_INFO_STREAM("Serial Port initialized");
	    open20ms(CONTROL_MODE);
    }
    else
    {
        return -1;
    }
    ros::Rate loop_rate(100);   
   while(ros::ok())
   {
        size_t n = ser.available();
	    int returndata;
        u16 sum =0;
        if(n>=28 && n<=200)
        {
             
	        u8 buffer[200] ={0};
	        returndata =  ser.read(buffer,n);
	        int num =0;
          
		
	        //第一个即为头(20ms)
	        if(buffer[0] == 0xED && buffer[1] == 0xDE && buffer[2] == 0x1C && buffer[4] == 0x81)
	        {
	            for(int i =0;i<sizeof(return_robot_data.rx_buffer);i++)
		        {
			       return_robot_data.rx_buffer[i] = buffer[i];
			        //printf("buffer1:%02X\n",return_robot_data.rx_buffer[i]);
		        }	     
	        }
	        else
	        {
		        //找头
		        for(int i = 0;i<returndata;i++)
		        {
	   		        if(buffer[i] == 0xED && buffer[i+1] == 0xDE && buffer[i+2] == 0x1C && buffer[i+4] == 0x81)
	   		        {	
   	      			    num = i;
	     			    // printf("num:%d\n",num);
	   		        }
		        }
            
                if(num<returndata && num != 0)
                {
                    for(int j = 0;j<sizeof(return_robot_data.rx_buffer);j++)
		            {
	  		            return_robot_data.rx_buffer[j] = buffer[num];
	  		            num++;
	  		            // printf("buffer:%02X\n",return_robot_data.rx_buffer[j]);
		            }
                }
	        }

            for(u8 i=0;i<sizeof(return_robot_data.rx_buffer)-2;i++)
            {
                sum += return_robot_data.rx_buffer[i];
            }

            ushort *sum_check = (ushort *)&return_robot_data.rx_buffer[26];
        
            //20ms
            if(return_robot_data.rx_buffer[0] == 0xED && return_robot_data.rx_buffer[1] == 0xDE && sum == *sum_check )
            {
                
                #if CONTROL_MODE ==1
                pt_msg.flwspeed = (return_robot_data.rx_buffer[7]<<8) | return_robot_data.rx_buffer[6];
	            pt_msg.frwspeed = (return_robot_data.rx_buffer[9]<<8) | return_robot_data.rx_buffer[8];
	            pt_msg.blwspeed = (return_robot_data.rx_buffer[11]<<8) | return_robot_data.rx_buffer[10];
	            pt_msg.brwspeed = (return_robot_data.rx_buffer[13]<<8) | return_robot_data.rx_buffer[12];
	       
                #elif CONTROL_MODE == 2
                pt_msg.Vx =  (return_robot_data.rx_buffer[7]<<8) | return_robot_data.rx_buffer[6];
                int16_t Vz = (return_robot_data.rx_buffer[9]<<8) | return_robot_data.rx_buffer[8];
                pt_msg.Vz = Vz *0.01;
                #endif   
              
                // printf("--------------------------------------------------------------------------------\n");
                pt_msg.flwadd = (return_robot_data.rx_buffer[15]<<8) | return_robot_data.rx_buffer[14];
	            pt_msg.frwadd = (return_robot_data.rx_buffer[17]<<8) | return_robot_data.rx_buffer[16];
	            pt_msg.blwadd = (return_robot_data.rx_buffer[19]<<8) | return_robot_data.rx_buffer[18];
	            pt_msg.brwadd = (return_robot_data.rx_buffer[21]<<8) | return_robot_data.rx_buffer[20];
	            double voltage = (return_robot_data.rx_buffer[23]<<8) | return_robot_data.rx_buffer[22];
	            pt_msg.voltage = voltage*0.1 ;
	            pt_msg.status = (return_robot_data.rx_buffer[25]<<8) | return_robot_data.rx_buffer[24];
                pub.publish(pt_msg); 
                memset(return_robot_data.rx_buffer,0,sizeof(return_robot_data.rx_buffer));
                
            }
            else
            {
                len = ser.available();
                printf("not read accuracy:%d\n",len);	 
                //清空数据残余
                if(len > 0 && len <= 200)
                {
                    ser.read(data,len);
                }
                else if(len > 200)
                {
                    ser.read(data,200);
                }
            }
           
        }
        else if(n > 200)
        {
            ser.read(data,200);
        }
        ros::spinOnce();      //集中处理本节点回调函数
        loop_rate.sleep();   //按前面设置的20Hz频率将程序挂起
   }
    return 0;
}
