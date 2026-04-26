#include "ros_rfid_serial.h"

serial::Serial ser; //声明串口对象 
/***************************************************************
 * @author     zxp
 * @brief     set_serial_data
 * @version    v1
 * @return     null
 * @date       2024/01/08
 * @example     处理串口数据
 **************************************************************/
void get_serial_data()
{
    std::string usart_port;
    int baud_data;
    
    ros::NodeHandle n;
    ros::NodeHandle private_nh("~");
    ros::Publisher control_pub = n.advertise<std_msgs::UInt16>("/rfid",100);

    private_nh.param<std::string>("usart_port", usart_port, "/dev/ttyACM0"); 
    private_nh.param<int>("baud_data", baud_data, 115200); 

    printf("usart_port:%s  baud_data:%d\n",usart_port.c_str(),baud_data);

    unsigned int len = 0,len_time = 0;
    u8  data[200];
    try  
    { 
         //设置串口属性，并打开串口 
        ser.setPort(usart_port); 
        ser.setBaudrate(baud_data);
        serial::Timeout to = serial::Timeout::simpleTimeout(2000); 
        ser.setTimeout(to); 
        //解决刚开机打不开问题
        sleep(1);
        ser.open(); 
    } 
    catch (serial::IOException& e) 
    { 
        ROS_ERROR("RFID Unable to open port \n"); 
        return; 
    } 

    //检测串口是否已经打开，并给出提示信息 
    if(ser.isOpen()) 
    { 
        printf("RFID Serial Port initialized\n"); 
    } 
    else 
    { 
        return; 
    } 
     ros::Rate loop_rate(100);    //设置发送数据的频率为100Hz
    while (ros::ok())
    {
        len = ser.available();
        if(len > sizeof(RxRfidData.data))
        {
            ser.read(RxRfidData.data,sizeof(RxRfidData.data));
            u8 sum = RxRfidData.prot.lid + RxRfidData.prot.hid;
            if(RxRfidData.prot.Header1 == 0xFF && RxRfidData.prot.Header2 == 0x5A && RxRfidData.prot.end == 0xA5 && sum == RxRfidData.prot.sum)
            {
                std_msgs::UInt16 rfid;
                rfid.data = ((RxRfidData.prot.hid << 8) & 0xFF00) + RxRfidData.prot.lid;
                control_pub.publish(rfid);
            }
            else
            {
                printf("get data error hed1:%X hed2:%X   sum:%X    Nsum:%X  end:%X",RxRfidData.prot.Header1,RxRfidData.prot.Header2,RxRfidData.prot.sum,sum,RxRfidData.prot.end);
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
            }
            len_time = 0;
        }
        ros::spinOnce();
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
    }
}
