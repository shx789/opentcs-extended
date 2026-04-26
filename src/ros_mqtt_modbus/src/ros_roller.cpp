#include "ros_roller.h"  

serial::Serial ser; //声明串口对象 
queue<int> send_roller_que;  //创建队列对象

/***************************************************************
 * @author     zxp
 * @brief     set_modbus_data
 * @version    v1
 * @return     null
 * @date       2024/01/08
 * @example     从modbus的0x06命令中获得地址
 **************************************************************/
static void set_modbus_data(struct Roller_Data &roller)
{
    modSer.setTab_input_registers(INPUT_ROLE_ADDR - INPUT_BASE_ADDR,roller.Moto);
    modSer.setTab_input_registers(INPUT_LIGHT_IN_ADDR - INPUT_BASE_ADDR,roller.Ligt_In);
    modSer.setTab_input_registers(INPUT_LIGHT_OUT_ADDR - INPUT_BASE_ADDR,roller.Ligt_Out);
};

/***************************************************************
 * @author     zxp
 * @brief     set_serial_data
 * @version    v1
 * @return     null
 * @date       2024/01/08
 * @example     设置串口数据
 **************************************************************/
void set_serial_data(unsigned char cmd)
{
    static Set_Roller_Data set_data;
    unsigned int chek = 0;
    set_data.prot.Header = HEADER;
    set_data.prot.Len = 0x08;
    set_data.prot.Type = 0x0A;
    set_data.prot.Cmd = cmd;
    set_data.prot.Num = 0x00;
    for(int i=0;i< sizeof(set_data.data)-2;i++)
    {
        chek += set_data.data[i];
        printf("%x  ",set_data.data[i]);
    }
    set_data.prot.Check = chek;
    printf("hed:%X  check:%X\n",set_data.prot.Cmd,set_data.prot.Check);
    ser.write(set_data.data,sizeof(set_data.data));
}

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
    struct Roller_Data   roller_data;
    unsigned char temp_data[11]={},data[200];
    unsigned int len = 0,len_time = 0;
    int argc;
    char **argv;
    try  
    { 
         //设置串口属性，并打开串口 
        ser.setPort("/dev/roller_usb"); 
        ser.setBaudrate(115200);
        serial::Timeout to = serial::Timeout::simpleTimeout(2000); 
        ser.setTimeout(to); 
        ser.open(); 
    } 
    catch (serial::IOException& e) 
    { 
        printf("Roller  Unable to open port \n"); 
        return; 
    } 

    //检测串口是否已经打开，并给出提示信息 
    if(ser.isOpen()) 
    { 
        printf("Roller Serial Port initialized\n"); 
    } 
    else 
    { 
        printf("Roller  Serial Port init failse\n"); 
        return; 
    } 
    while (1)
    {
        len = ser.available();
        if(len >= 11)
        {    
            ser.read(temp_data,sizeof(temp_data));
            unsigned int check = 0;
            for(int i=0;i<sizeof(temp_data)-2;i++)
            {
                check += temp_data[i];
            }
            roller_data.Header = ((temp_data[1]<<8)&0xFF00) + (temp_data[0]&0xFF);
            roller_data.Len = temp_data[2];
            roller_data.Type = temp_data[3];
            roller_data.Cmd = temp_data[4];
            roller_data.Num = temp_data[5];
            roller_data.Moto = temp_data[6];
            roller_data.Ligt_In = temp_data[7];
            roller_data.Ligt_Out = temp_data[8];
            roller_data.Check = ((temp_data[10] << 8)&0xFF00) + (temp_data[9]&0xFF);

            //数据正确
            if(roller_data.Header == HEADER && roller_data.Check == check)
            {
                //printf("get dat\n");
                set_modbus_data(roller_data);    
            }
            else
            {
                printf("get data error Header:%X Check:%X   NCheck%X",roller_data.Header,roller_data.Check,check);
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
        else
        {
            len_time++;
            if(len_time > 40)
            {
                len_time = 0;
                printf("the roller serial have no data\n");
            }
        }
        deal_modbus_cmd();
        usleep(25000);
    }
    return;
}

/***************************************************************
 * @author     zxp
 * @brief     roller_get_modbus_cmd
 * @version    v1
 * @return     null
 * @date       2024/01/08
 * @example     从modbus的0x06命令中获得地址
 **************************************************************/
void roller_get_modbus_cmd(int addr)
{
    send_roller_que.push(addr);
    
}

/***************************************************************
 * @author     zxp
 * @brief      deal_modbus_cmd
 * @version    v1
 * @return     null
 * @date       2024/01/08
 * @example     处理modbus的指令
 **************************************************************/
void deal_modbus_cmd(void)
{
    //printf("get addr:%x\n",send_roller_que.front());
    if(!send_roller_que.empty())
    {
        //printf("get data:%d\n",send_roller_que.size());
        switch(send_roller_que.front())
        {
            case ROLLER_CONTROL_ADDR:   //滚筒控制指令
            {
                int addr = ROLLER_CONTROL_ADDR - BASE_ADDR;
                uint16_t cmd = modSer.getRegisterValue(addr -0);
                //printf("get data:%d cmd:%d\n",send_roller_que.front(),cmd);
                set_serial_data(cmd);
                break;
            }
            default:
            {
                break;
            }
        }  
        send_roller_que.pop();
    }
}