#include <ros_agv3_msg.h>
#include <ros/ros.h>                      //类似 C 语言\E7\9A?stdio.h 
#include <serial/serial.h>                //ROS已经内置了的串口\E5\8C?
#include <std_msgs/String.h> 
#include <std_msgs/Empty.h> 
#include <std_msgs/UInt8.h> 

#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Vector3.h>
#include <geometry_msgs/Twist.h>
#include <signal.h>
#include <string.h>
#include <string> 
#include <ros_agv3_msg/agv1.h>            //要用\E5\88?msg 中定义的数据类型 
#include <ros_agv3_msg/agv2.h>            //要用\E5\88?msg 中定义的数据类型 

#define CONTROL_MODE 1  //传输模式选择\EF\BC?：线速模式，2：轮速模\E5\BC?
#define Base_Width 348  //轴距

serial::Serial ser; //声明串口对象 
/*************************************************************************/
static void open20MsData(u8 data)
{
  Open20MsData.prot.Header = HEADER;
  Open20MsData.prot.Len    = 10;
  Open20MsData.prot.Type   = 4;
  Open20MsData.prot.Cmd    = 0x01;
  Open20MsData.prot.Num    = 1;
  Open20MsData.prot.Data   = data;
  Open20MsData.prot.Check  = 0;
  for(int i=0;i < Open20MsData.prot.Len - 2;i++)
  {
    Open20MsData.prot.Check += Open20MsData.data[i];
  }
  ser.write(Open20MsData.data,sizeof(Open20MsData.data));
}
/*************************************************************************/
static void openGoCharge(u8 data)
{
  OpenGoCharge.prot.Header = HEADER;
  OpenGoCharge.prot.Len    = 10;
  OpenGoCharge.prot.Type   = 4;
  OpenGoCharge.prot.Cmd    = 0x04;
  OpenGoCharge.prot.Num    = 1;
  OpenGoCharge.prot.Data   = data;
  OpenGoCharge.prot.Check  = 0;
  for(int i=0;i < OpenGoCharge.prot.Len - 2;i++)
  {
    OpenGoCharge.prot.Check += OpenGoCharge.data[i];
  }
  ser.write(OpenGoCharge.data,sizeof(OpenGoCharge.data));
}
/****************************灯颜色和急停控制*********************************************/
static void SetLedStop(u8 led1,u8 led2,u8 led3,u8 led4,u16 stop)
{
  LedStopSet.prot.Header = HEADER;
  LedStopSet.prot.Len    = 14;
  LedStopSet.prot.Type   = 4;
  LedStopSet.prot.Cmd    = 0x03;
  LedStopSet.prot.Num    = 3;
  LedStopSet.prot.Led12  = led1 + (led2<<8);
  LedStopSet.prot.Led34  = led3 + (led4<<8);
  LedStopSet.prot.Stop   = stop;
  LedStopSet.prot.Check  = 0;
  for(int i=0;i < LedStopSet.prot.Len - 2;i++)
  {
    LedStopSet.prot.Check += LedStopSet.data[i];
  }
  ser.write(LedStopSet.data,sizeof(LedStopSet.data));
}
/*************************************************************************/
void setSpeed(float Vx,float Vz)
{
  s16 TempLSpeed=0,TempRSpeed=0;

  TempLSpeed = Vx*1000 - Vz*Base_Width/2.0;
  TempRSpeed = Vx*1000 + Vz*Base_Width/2.0;
 

#if   CONTROL_MODE == 1

  TXRobotData1.prot.Header  = HEADER;
  TXRobotData1.prot.Len     = 16;
  TXRobotData1.prot.Type    = 4;
  TXRobotData1.prot.Cmd     = 0x02;
  TXRobotData1.prot.Num     = 4;
  TXRobotData1.prot.Mode    = 0;
  TXRobotData1.prot.Vx      = Vx*1000;
  TXRobotData1.prot.Vz      = Vz;
  TXRobotData1.prot.Check   = 0;

  for(int i=0;i < sizeof(TXRobotData1.data) - 2;i++)
  {
    TXRobotData1.prot.Check += TXRobotData1.data[i];
  }
  ser.write(TXRobotData1.data,sizeof(TXRobotData1.data));

#elif CONTROL_MODE == 2

  TXRobotData2.prot.Header  = HEADER;
  TXRobotData2.prot.Len     = 16;
  TXRobotData2.prot.Type    = 4;
  TXRobotData2.prot.Cmd     = 0x02;
  TXRobotData2.prot.Num     = 4;
  TXRobotData2.prot.Mode    = 1;
  TXRobotData2.prot.LSpeed  = TempLSpeed;
  TXRobotData2.prot.RSpeed  = TempRSpeed;
  TXRobotData2.prot.NC      = 0;
  TXRobotData2.prot.Check   = 0;

  for(int i=0;i < sizeof(TXRobotData2.data) - 2;i++)
  {
    TXRobotData2.prot.Check += TXRobotData2.data[i];
  }
  ser.write(TXRobotData2.data,sizeof(TXRobotData2.data));

#endif
  
}

/*************************************************************************/
void go_charge_Callback(const std_msgs::UInt8 status)
{
   openGoCharge(status.data);
}

/*************************************************************************/
void software_stop_Callback(const std_msgs::UInt8 stop)
{
   software_stop_flag = stop.data;
}
/*************************************************************************/
void cmd_velCallback(const geometry_msgs::Twist &twist_aux)
{
  
    setSpeed(twist_aux.linear.x,twist_aux.angular.z);
}
/*************************************************************************/
/*************************************************************************/
//当关闭包时调用，关闭20ms上传
void mySigIntHandler(int sig)
{
   ROS_INFO("close the serial!\n");
   open20MsData(0);
   sleep(1);
   SetLedStop(0x77,0x77,0,0,0);
   //ser.close();
   ros::shutdown();
}
/*************************************************************************/
int main(int argc,char **argv)
{    
   u16 len=0,TempCheck=0,len_time = 0;
   u8  data[200];
   u16 TimeCnt=0;
   string smoother_cmd_vel, usart_port;
   int baud_data;

   ros::init(argc,argv,"ros_agv3_talker",ros::init_options::NoSigintHandler);            //解析参数，命名节点为 talker
   signal(SIGINT, mySigIntHandler);  										//把信号槽连接到mySigIntHandler保证关闭节点时能够关\E9\97?0ms数据上传

   ros::NodeHandle nh;
   ros::NodeHandle n("~");
#if   CONTROL_MODE == 1
   ros_agv3_msg::agv1 agv_msg;
   ros::Publisher pub = nh.advertise<ros_agv3_msg::agv1>("agv_info",1);//创建 publisher 对象

#elif CONTROL_MODE == 2
   ros_agv3_msg::agv2 agv_msg;
   ros::Publisher pub = nh.advertise<ros_agv3_msg::agv2>("agv_info",1);//创建 publisher 对象
#endif
   n.param<std::string>("usart_port", usart_port, "/dev/robot_usb"); 
   n.param<int>("baud_data", baud_data, 115200); 
   n.param<std::string>("smoother_cmd_vel", smoother_cmd_vel, "/cmd_vel"); 

   ros::Subscriber sub = nh.subscribe(smoother_cmd_vel,100,cmd_velCallback);  //
   ros::Subscriber go_charde_sub = nh.subscribe("go_charge",100,go_charge_Callback);  //
   ros::Subscriber software_stop_sub = nh.subscribe("software_stop",100,software_stop_Callback);  //

   try  
    { 
         //设置串口属性，并打开串口 
        ser.setPort(usart_port); 
        ser.setBaudrate(baud_data);
        serial::Timeout to = serial::Timeout::simpleTimeout(2000); 
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
        ROS_INFO_STREAM("Serial Port initialized"); 
    } 
    else 
    { 
        return -1; 
    } 

   open20MsData(CONTROL_MODE);
   ros::Rate loop_rate(100);    //设置发送数据的频率为10Hz
   while(ros::ok())
   { 
      len = ser.available();
      if(len >= sizeof(RXRobotData20MS.data))
      {                                                                
         //wait have date to spinince ,the callback
         ser.read(RXRobotData20MS.data,sizeof(RXRobotData20MS.data));
         //get check
         TempCheck = 0;
         for(u8 i=0;i<sizeof(RXRobotData20MS.data)-2;i++)
         {
            TempCheck += RXRobotData20MS.data[i];
         }

         //头和校验正确
         if(RXRobotData20MS.prot.Header == HEADER && RXRobotData20MS.prot.Check == TempCheck)
         {

   #if   CONTROL_MODE == 1

            for(int i=0;i<sizeof(RXMode1.data);i++)
            {
               RXMode1.data[i] = RXRobotData20MS.prot.data[i];
            }
            //消息赋\E5\80?         
            agv_msg.Vx      = RXMode1.prot.Vx;
            agv_msg.Vz      = RXMode1.prot.Vz ; //补偿
            agv_msg.AccX    = RXMode1.prot.AccX;
            agv_msg.AccY    = RXMode1.prot.AccY;
            agv_msg.AccZ    = RXMode1.prot.AccZ;
            agv_msg.GyrX    = RXMode1.prot.GyrX;
            agv_msg.GyrY    = RXMode1.prot.GyrY;
            agv_msg.GyrZ    = RXMode1.prot.GyrZ;
            agv_msg.Voltage = RXMode1.prot.Voltage; 
            agv_msg.State   = RXMode1.prot.State; 
            agv_msg.Light12 = RXMode1.prot.Light12; 
            agv_msg.Light34 = RXMode1.prot.Light34; 
      
            //解决陀螺仪匀速旋转角速度为0的问题。 匀速旋转，车子角速度不为0，陀螺仪为0，则车子的角速度。或者陀螺仪的角速度大于车子的太多，则用车子的。(长时间匀速旋转会有这个问题)
            if((fabs(agv_msg.Vz) > 0.1 && fabs(agv_msg.GyrZ) < 0.05) || (fabs(agv_msg.GyrZ) - fabs(agv_msg.Vz) > 0.1))
            {
               agv_msg.GyrZ = agv_msg.Vz;
            }
            else if(fabs(agv_msg.Vz) < 0.01 && fabs(agv_msg.GyrZ) > 0.05)
            {
               agv_msg.GyrZ = 0.0;
            }

            pub.publish(agv_msg);                      //发布消息
            
            /*
            printf("Vx:%d Vz:%.2f Ax:%.2f Ay:%.2f Az:%.2f Gx:%.2f Gy:%.2f Gz:%.2f Vo:%d St:%d L12:%d L34:%d\n",
               RXMode1.prot.Vx,     RXMode1.prot.Vz,
               RXMode1.prot.AccX,   RXMode1.prot.AccY, RXMode1.prot.AccZ,
               RXMode1.prot.GyrX,   RXMode1.prot.GyrY, RXMode1.prot.GyrZ,
               RXMode1.prot.Voltage,RXMode1.prot.State,RXMode1.prot.Light12,
               RXMode1.prot.Light34);
            
            ROS_INFO("---------------------------------------------------\n");
            */
            memset(RXMode1.data,0,sizeof(RXMode1.data));

   #elif CONTROL_MODE == 2

            for(int i=0;i<sizeof(RXMode2.data);i++)
            {
               RXMode2.data[i] = RXRobotData20MS.prot.data[i];
            }
            //消息赋\E5\80?         
            agv_msg.LSpeed  = RXMode2.prot.LSpeed;
            agv_msg.RSpeed  = RXMode2.prot.RSpeed;
            agv_msg.LAddEN  = RXMode2.prot.LAddEN;
            agv_msg.RAddEN  = RXMode2.prot.RAddEN;
            agv_msg.AccX    = RXMode2.prot.AccX;
            agv_msg.AccY    = RXMode2.prot.AccY;
            agv_msg.AccZ    = RXMode2.prot.AccZ;
            agv_msg.GyrX    = RXMode2.prot.GyrX;
            agv_msg.GyrY    = RXMode2.prot.GyrY;
            agv_msg.GyrZ    = RXMode2.prot.GyrZ;
            agv_msg.Voltage = RXMode2.prot.Voltage; 
            agv_msg.State   = RXMode2.prot.State; 
            agv_msg.Light12 = RXMode2.prot.Light12; 
            agv_msg.Light34 = RXMode2.prot.Light34; 
            pub.publish(agv_msg);                      //发布消息

            printf("LS:%d RS:%d LA:%d RA:%d Ax:%.2f Ay:%.2f Az:%.2f Gx:%.2f Gy:%.2f Gz:%.2f Vo:%d St:%d L12:%d L34:%d\n",
               RXMode2.prot.LSpeed, RXMode2.prot.RSpeed,RXMode2.prot.LAddEN, RXMode2.prot.RAddEN,
               RXMode2.prot.AccX,   RXMode2.prot.AccY,  RXMode2.prot.AccZ,
               RXMode2.prot.GyrX,   RXMode2.prot.GyrY,  RXMode2.prot.GyrZ,
               RXMode2.prot.Voltage,RXMode2.prot.State, RXMode2.prot.Light12,
               RXMode2.prot.Light34);
            
            ROS_INFO("---------------------------------------------------\n");
            memset(RXMode2.data,0,sizeof(RXMode2.data));
   #endif
         
         }
         else     //
         {
            ROS_INFO("Send The ON 20ms up date CMD %x %x %x\n",RXRobotData20MS.prot.Header,RXRobotData20MS.prot.Check,TempCheck);

            len = ser.available();
            //清空数据残余
            if(len > 0 && len < 200)
            {
               ser.read(data,len);
            }
            else
            {
               ser.read(data,200);
            }
            open20MsData(CONTROL_MODE);
         
         }
         len_time = 0;
      }
      else 
      {
        len_time++;
        if(len_time > 100)
        {
           len_time = 0;
           open20MsData(CONTROL_MODE);
           printf("ros agv3 open 20cm\n");
        }
       
      }

      //Led Test
      TimeCnt++;
      if(software_stop_flag == false)
      {
         
         if(TimeCnt == 250)
         {
            SetLedStop(0x01,0x01,0,0,0);
         }
         else if(TimeCnt == 500)
         {
            SetLedStop(0x02,0x02,0,0,0);
         }
         else if(TimeCnt == 750)
         {
            SetLedStop(0x04,0x04,0,0,0);
         }
         else if(TimeCnt > 1000)
         {
            SetLedStop(0x07,0x07,0,0,0);
            TimeCnt = 0;
         }
      }
      else
      {
         if(TimeCnt > 250)
         {
            SetLedStop(0x22,0x22,0,0,1);
            TimeCnt = 0;
         }
      }
      

      ros::spinOnce();
      loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
   }
   return 0;
}
/*************************************************************************/
