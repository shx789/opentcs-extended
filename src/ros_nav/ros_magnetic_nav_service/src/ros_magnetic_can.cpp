#include "ros_magnetic_can.h"

/***************************************************************
 * @author     zxp
 * @brief      mqtt_poll
 * @version    v1
 * @return     null
 * @date       2023/10/09
 * @example     mqtt poll处理
 **************************************************************/
VCI_BOARD_INFO pInfo;//用来获取设备信息。
void run_magnetic_can()
{
    ros::NodeHandle private_nh("~");
    ros::Publisher magnetic_front_pub = private_nh.advertise<ros_magnetic_nav_service::magnetic>("/magnetic_can_front",100);
    ros::Publisher magnetic_back_pub = private_nh.advertise<ros_magnetic_nav_service::magnetic>("/magnetic_can_back",100);

    ros_magnetic_nav_service::magnetic front_value,back_value;
    
    int re_size = 0;
    int num = VCI_OpenDevice(VCI_USBCAN2,0,0);
    if(num == 0 || num == 1)//打开设备
    {
        printf(">>open deivce success!\n");//打开设备成功
    }else
    {
        printf(">>open deivce error %d!\n",num);
        exit(1);
    }
    if(VCI_ReadBoardInfo(VCI_USBCAN2,0,&pInfo)==1)//读取设备序列号、版本等信息。
    {
        printf(">>Get VCI_ReadBoardInfo success!\n");
    }else
    {
        printf(">>Get VCI_ReadBoardInfo error!\n");
        exit(1);
    }

    //初始化参数，严格参数二次开发函数库说明书。
    VCI_INIT_CONFIG config;
    config.AccCode=0;
    config.AccMask=0xFFFFFFFF;//FFFFFFFF全部接收
    config.Filter=2;//接收所有帧  2-只接受标准帧  3-只接受扩展帧
    config.Timing0=0x00;/*波特率500 Kbps  0x00  0x1C*/
    config.Timing1=0x1C;
    config.Mode=0;//正常模式

    if(VCI_InitCAN(VCI_USBCAN2,0,0,&config)!=1)
    {
        printf(">>Init CAN1 error\n");
        VCI_CloseDevice(VCI_USBCAN2,0);
    }

    if(VCI_StartCAN(VCI_USBCAN2,0,0)!=1)
    {
        printf(">>Start CAN1 error\n");
        VCI_CloseDevice(VCI_USBCAN2,0);

    }
   
    //需要读取的帧，结构体设置
    VCI_CAN_OBJ rev[2500];
    rev[0].SendType=0;
    rev[0].RemoteFlag=0;
    rev[0].ExternFlag=0;
    rev[0].DataLen=8;

    ros::Rate loop_rate(100);    //设置发送数据的频率为100Hz
    while (ros::ok())
    {
        //读取数据
        re_size = VCI_Receive(VCI_USBCAN2, 0, 0,rev, 2500, 0);
        if(re_size > 0)
        {	
            for(int i=0;i< re_size;i++)
            {
                // fprintf(fp, "%03d ", i);
                // fprintf(fp, "%02d:%02d:%02d ",t->tm_hour, t->tm_min, t->tm_sec);
                // fprintf(fp, "%04x %02X %02X %02X %02X %02X %02X %02X %02X\n",
                //         rev[i].ID,rev[i].Data[0],rev[i].Data[1],rev[i].Data[2],rev[i].Data[3],rev[i].Data[4],rev[i].Data[5],rev[i].Data[6],rev[i].Data[7]);
                // printf("%04x %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                //     rev[i].ID,rev[i].Data[0],rev[i].Data[1],rev[i].Data[2],rev[i].Data[3],rev[i].Data[4],rev[i].Data[5],rev[i].Data[6],rev[i].Data[7]);

                if(rev[i].ID == 0x0161 && rev[i].Data[0] == 0x01 && rev[i].Data[1] == 0xAB && rev[i].Data[2] == 0x00 && rev[i].Data[3] == 0x28)
                {
                    int L_sum = 0,H_sum = 0,Sum = 0,num = 0;
                    for(int j=0;j<8;j++)
                    {
                        bool bit = ((rev[i].Data[5] >> j) & 0x01);
                        if(bit)
                        {
                            num++;
                        }
                        L_sum -= bit * (7-j);
                    }

                    for(int k=0;k<8;k++)
                    {
                        bool bit =  ((rev[i].Data[4] >> k) & 0x01);
                        if(bit)
                        {
                            num++;
                        }
                        H_sum += bit * k;
                    }

                    Sum = L_sum + H_sum;
                    //发布消息
                    front_value.value = Sum;
                    front_value.num = num;
                    front_value.l8 = rev[i].Data[5];
                    front_value.h8 = rev[i].Data[4];
                    magnetic_front_pub.publish(front_value);
                    // printf("front Sum:%d  L_sum:%d  H_sum:%d  num:%d   %02X  %02X\n",Sum,L_sum,H_sum,num,rev[i].Data[4],rev[i].Data[5]);
                }

                else  if(rev[i].ID == 0x0161 && rev[i].Data[0] == 0x02 && rev[i].Data[1] == 0xAB && rev[i].Data[2] == 0x00 && rev[i].Data[3] == 0x28)
                {
                    int L_sum = 0,H_sum = 0,Sum = 0,num = 0;
                    for(int j=0;j<8;j++)
                    {
                        bool bit = ((rev[i].Data[5] >> j) & 0x01);
                        if(bit)
                        {
                            num++;
                        }
                        L_sum -= bit * (7-j);
                    }

                    for(int k=0;k<8;k++)
                    {
                        bool bit =  ((rev[i].Data[4] >> k) & 0x01);
                        if(bit)
                        {
                            num++;
                        }
                        H_sum += bit * k;
                    }

                    Sum = L_sum + H_sum;
                    //发布消息
                    back_value.value = Sum;
                    back_value.num = num;
                    back_value.l8 = rev[i].Data[5];
                    back_value.h8 = rev[i].Data[4];
                    magnetic_back_pub.publish(back_value);
                    // printf("front Sum:%d  L_sum:%d  H_sum:%d  num:%d   %02X  %02X\n",Sum,L_sum,H_sum,num,rev[i].Data[4],rev[i].Data[5]);
                }
            }
     
        }
        ros::spinOnce();
        loop_rate.sleep();  //按前面设置的10Hz频率将程序挂起
    }
    VCI_CloseDevice(VCI_USBCAN2,0);//关闭设备。
}
