#include "ros_modbus.h"
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      modbus close_sigint
 * @param     dummy
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
void RDSModbusSlave::close_sigint(int dummy)
{
    if (server_socket != -1) 
    {
        close(server_socket);
    }
    modbus_free(ctx);
    modbus_mapping_free(mapping);

    exit(dummy);
}

/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author    zxp
 * @brief      modbus initialization
 * @param      IP/PORT/debugflag
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::initModbus(std::string Host_Ip = "0.0.0.0", int port = 1502, bool debugging = true)
{
   ctx = modbus_new_tcp(Host_Ip.c_str(), port);

    //mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0, MODBUS_MAX_READ_REGISTERS, 0);
    //mapping = modbus_mapping_new_start_address(0,0,0,0,40001,m_numRegisters,30001,m_numInputRegisters);
    mapping = modbus_mapping_new_start_address(0,0,0,0,BASE_ADDR,m_numRegisters,INPUT_BASE_ADDR,m_numInputRegisters);
    if (mapping == NULL) 
    {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return false;
    }

    server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
    if (server_socket == -1) 
    {
        fprintf(stderr, "Unable to listen TCP connection\n");
        modbus_free(ctx);
        return false;
    }

    //signal(SIGINT, &RDSModbusSlave::close_sigint);
    return true;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author    zxp
 * @brief      Constructor
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
RDSModbusSlave::RDSModbusSlave()
{
    bool res = initModbus("0.0.0.0", 1502, false);
    if(res)
    {
        printf("initModbus is sucess\n");
    }
    else
    {
        printf("initModbus is failse\n");
    }
    //TODO：
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      Destructor
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
RDSModbusSlave::~RDSModbusSlave()
{
    modbus_mapping_free(mapping);
    modbus_close(ctx);
    modbus_free(ctx);
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      loadFromConfigFile
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
void RDSModbusSlave::loadFromConfigFile()
{
    return;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      run
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
void RDSModbusSlave::run()
{
    std::thread loop([this]()
    {
        while (true)
        {
             recieveMessages();
        }
        close_sigint(1);
    });
    loop.detach();
    return;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      modbus_set_slave_id
 * @param      id
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::modbus_set_slave_id(int id)
{
    int rc = modbus_set_slave(ctx, id);
    if (rc == -1)
    {
        fprintf(stderr, "Invalid slave id\n");
        printf("Invalid slave id\n");
        modbus_free(ctx);
        return false;
    }
    return true;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      setRegisterValue(设置保存寄存器的值，类型为uint16_t)
 * @param      registerStartaddress(保存寄存器的起始地址)
 * @param      Value(写入到保存寄存器里的值)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::setRegisterValue(int registerStartaddress, uint16_t Value)
{
    if (registerStartaddress > (m_numRegisters - 1))
    {
        return false;
    }
    slavemutex.lock();
    mapping->tab_registers[registerStartaddress] = Value;
    slavemutex.unlock();
    return true;
}

/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      setRegisterValue(设置保存寄存器的值，类型为uint16_t)
 * @param      registerStartaddress(保存寄存器的起始地址)
 * @param      Value(写入到输入寄存器里的值)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::setTab_input_registers(int registerStartaddress, uint16_t Value)
{
    if (registerStartaddress > (m_numInputRegisters - 1))
    {
        return false;
    }
    slavemutex.lock();
    mapping->tab_input_registers[registerStartaddress] = Value;
    slavemutex.unlock();
    return true;
}

/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      setRegisterValue(设置保存寄存器的值，类型为float)
 * @param      registerStartaddress(保存寄存器的起始地址)
 * @param      Value(写入到输入寄存器里的值)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::setTab_input_Floatregisters(int registerStartaddress, float Value)
{
    if (registerStartaddress > (m_numInputRegisters - 2))
    {
        return false;
    }
    /*大端模式*/
    slavemutex.lock();
    modbus_set_float_badc(Value, &mapping->tab_input_registers[registerStartaddress]);
    slavemutex.unlock();
    return true;
}

/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author    zxp
 * @brief      getRegisterValue(获取保存寄存器的值)
 * @param      registerStartaddress(保存寄存器的起始地址)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
uint16_t RDSModbusSlave::getRegisterValue(int registerStartaddress)
{
    return mapping->tab_registers[registerStartaddress];
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      setTab_Input_Bits(设置输入寄存器某一位的值)
 * @param      NumBit(输入寄存器的起始地址)
 * @param      Value(输入寄存器的值)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::setTab_Input_Bits(int NumBit, uint8_t Value)
{
    if (NumBit > (m_numInputBits - 1))
    {
        return false;
    }
    slavemutex.lock();
    mapping->tab_input_bits[NumBit] = Value;
    slavemutex.unlock();
    return true;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      getTab_Input_Bits(获取输入寄存器某一位的值)
 * @param      NumBit(输入寄存器相应的bit位)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
uint8_t RDSModbusSlave::getTab_Input_Bits(int NumBit)
{
    return mapping->tab_input_bits[NumBit];
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      setRegisterFloatValue(设置浮点值)
 * @param      (Value：浮点值，registerStartaddress寄存器起始地址)
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
bool RDSModbusSlave::setRegisterFloatValue(float Value, int registerStartaddress)
{
    if (registerStartaddress > (m_numRegisters - 2))
    {
        return false;
    }
    /*小端模式*/
    slavemutex.lock();
    modbus_set_float_badc(Value, &mapping->tab_registers[registerStartaddress]);
    slavemutex.unlock();
    return true;
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author    zxp
 * @brief      获取寄存器里的浮点数 
 * @param      registerStartaddress寄存器起始地址
 * @version    v1
 * @return     两个uint16_t拼接而成的浮点值
 * @date       2023/10/09
 **************************************************************/
float RDSModbusSlave::getRegisterFloatValue(int registerStartaddress)
{
    return modbus_get_float_badc(&mapping->tab_registers[registerStartaddress]);
}
/***************************************************************
 * @file       RDSModbusSlave.cpp
 * @author     zxp
 * @brief      支持多个master同时连接
 * @version    v1
 * @return     null
 * @date       2023/10/09
 **************************************************************/
void RDSModbusSlave::recieveMessages()
{
    static uint16_t read_cnt = 0;
   uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    /* Maximum file descriptor number */
    int fdmax;
    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket;

    int header_length = modbus_get_header_length(ctx);
    while (true)
    {
        rdset = refset;
        if (select(fdmax + 1, &rdset, NULL, NULL, NULL) == -1) 
        {
            perror("Server select() failure.");
            close_sigint(1);
        }

        /* Run through the existing connections looking for data to be
         * read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) 
        {

            if (!FD_ISSET(master_socket, &rdset)) 
            {
                continue;
            }

            if (master_socket == server_socket) 
            {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket, (struct sockaddr *) &clientaddr, &addrlen);
                if (newfd == -1) 
                {
                    perror("Server accept() error");
                } 
                else 
                {
                    FD_SET(newfd, &refset);
                    if (newfd > fdmax) 
                    {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    printf("New connection from %s:%d on socket %d\n",inet_ntoa(clientaddr.sin_addr),clientaddr.sin_port,newfd);
                }
            } 
            else 
            {
                modbus_set_socket(ctx, master_socket);
                rc = modbus_receive(ctx, query);     
                if (rc > 0)   //接收到的报文长度
                {
                     modbus_reply(ctx, query, rc, mapping);
                     /* Special server behavior to test client */
                     int addr = MODBUS_GET_INT16_FROM_INT8(query, header_length + 1);
                     //printf("the cmd is %x  addr:%d\n",query[header_length],addr);

                    //输入寄存器读取计数
                     if(query[header_length] == 0x03)   //03读取输入寄存器
                     {
                        read_cnt++;
                        setTab_input_registers(0,read_cnt);
                     }

                     if(query[header_length] == 0x06)   //06写保持寄存器指令
                     {
                       
                        get_modbus_cmd(addr);
                        roller_get_modbus_cmd(addr);
                         printf("get data:%d\n",addr);
                     }
                     
                } 
                else if (rc == -1) 
                {
                    /* This example server in ended on connection closing or * any errors. */
                    printf("Connection closed on socket %d\n", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) 
                    {
                        fdmax--;
                    }
                }
            }
        }
    }
}
