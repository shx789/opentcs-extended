#include <laser_udp/laser_common.h>

#include <cstdio>
#include <cstring>

namespace laser_udp
{
ClaserCommon::ClaserCommon(CParserBase *parser, ros::NodeHandle &nh, ros::NodeHandle &pnh) :
    mDiagPublisher(NULL),
    dExpectedFreq(15.0), /* Default frequency */
    mParser(parser),
    mNodeHandler(nh),
    mPrivNodeHandler(pnh)
{
    /*Initialize receive buffer*/
    memset(mRecvBuffer, 0, CMD_FRAME_MAX_LEN);

    /*Set reconfigure callback*/
    dynamic_reconfigure::Server<laser_udp::laserConfig>::CallbackType f;
    f = boost::bind(&laser_udp::ClaserCommon::UpdateConfig, this, _1, _2);
    mDynaReconfigServer.setCallback(f);

    /*Set data publisher (used for debug)*/
    mPrivNodeHandler.param<bool>("publish_datagram", mPublishData, false);
    if(mPublishData)
    {
        /*datagram publish is enabled*/
        mDataPublisher = mNodeHandler.advertise<std_msgs::String>("datagram", 1000);
    }

    /*Set scan publisher*/
    mScanPublisher = mNodeHandler.advertise<sensor_msgs::LaserScan>("scan", 1000);

    mDiagUpdater.setHardwareID("none");
    mDiagPublisher = new diagnostic_updater::DiagnosedPublisher<sensor_msgs::LaserScan>(mScanPublisher,
            mDiagUpdater,
            /* frequency should be target +- 10% */
            diagnostic_updater::FrequencyStatusParam(&dExpectedFreq, &dExpectedFreq, 0.1, 10),
            /*timestamp delta can be from 1.1 to 1.3x what it ideally is*/
            diagnostic_updater::TimeStampStatusParam(-1, 1.3 * 1.0 / dExpectedFreq - mConfig.time_offset));

    ROS_ASSERT(mDiagPublisher);
}

int ClaserCommon::StopScanner()
{
    int result = 0;
    result = SendDeviceReq(CMD_STOP_STREAM_DATA, NULL);
    return result;
}

bool ClaserCommon::RebootDevice()
{
    return true;
}

ClaserCommon::~ClaserCommon()
{
    delete mDiagPublisher;
    ROS_INFO("laser_udp drvier exiting.\n");
}

int ClaserCommon::Init()
{
    int result = InitDevice();
    if(0 != result)
    {
        ROS_FATAL("Failed to init device: %d", result);
        return result;
    }

    result = InitScanner();
    if(0 != result)
    {
        ROS_FATAL("Failed to init scanner: %d", result);
    }

    return result;
}

int ClaserCommon::InitScanner()
{
    SendDeviceReq(CMD_START_STREAM_DATA, NULL);
    return ExitSuccess;
}

std::string ClaserCommon::StringResp(const std::vector<unsigned char> &resp)
{
    std::string strResp;
    for(std::vector<unsigned char>::const_iterator it = resp.begin();
            it != resp.end();
            it++)
    {
        if(*it > 13)
        {
            strResp.push_back(*it);
        }
    }

    return strResp;
}

bool ClaserCommon::IsCompatibleDevice(const std::string strIdentify) const
{
    // TODO: Always return true
    return true;
}

int ClaserCommon::LoopOnce()
{
    unsigned char header[4] = {0xFE, 0x5A, 0xA5, 0x55};
    static unsigned int lastFrameTotalIndex = 0xFFFFFFFF;
    unsigned int n, totalDataLen, addInfoLen;
    unsigned char *p_data, *p_info;


    mDiagUpdater.update();

    int dataLength = 0;
    static unsigned int iteration_count = 0;

    int result = GetDataGram(mRecvBuffer, CMD_FRAME_MAX_LEN, &dataLength);
    if(0 != result)
    {
        ROS_ERROR("laser - Read Error when getting datagram: %d", result);
        mDiagUpdater.broadcast(diagnostic_msgs::DiagnosticStatus::ERROR,
                               "laser - Read Error when getting datagram.");

        return ExitError;
    }
    else
    {
        ROS_DEBUG("laser - Received data gram. Data Length %d", dataLength);

        //ROS_INFO_ONCE("laser_LS - Successfully connected !!");
        if(!mConnectFlag)
        {
            mConnectFlag = 1;
            ROS_INFO("laser - Successfully connected !!");
        }

        if(memcmp(mRecvBuffer, header, sizeof(header)) == 0)    //compare the header                     //compare the header
        {

            int rawDatalen = ((*(mRecvBuffer + CMD_FRAME_HEADER_LENGTH_H) << 8) | (*(mRecvBuffer + CMD_FRAME_HEADER_LENGTH_L)))
                             - (CMD_FRAME_DATA_START - CMD_FRAME_HEADER_CHECK);                            //raw data length

            unsigned int frameTotalIndex = (*(mRecvBuffer + CMD_FRAME_HEADER_TOTAL_INDEX_H) << 8)
                                           | (*(mRecvBuffer + CMD_FRAME_HEADER_TOTAL_INDEX_L));          //current totalIndex

            unsigned char subPkgNum = *(mRecvBuffer + CMD_FRAME_HEADER_SUB_PKG_NUM);
            unsigned char subPkgIndex = *(mRecvBuffer + CMD_FRAME_HEADER_SUB_INDEX);  //current subPkgIndex
            unsigned char subPkgType = *(mRecvBuffer + CMD_FRAME_HEADER_SUB_TYPE);    //current subPkgType

            unsigned char checkSum = 0;    //checkSunm
            for(int i = 0; i < rawDatalen + (CMD_FRAME_DATA_START - CMD_FRAME_HEADER_TYPE); i++)  //add sum
            {
                checkSum ^= *(mRecvBuffer + CMD_FRAME_HEADER_TYPE + i);
            }
            if(checkSum != *(mRecvBuffer + CMD_FRAME_HEADER_CHECK))    //check sum
            {
                memset(mRecvBuffer, 0, CMD_FRAME_MAX_LEN);
                ROS_WARN("checkSum error");
                return ExitSuccess;
            }

            if(dataLength != (rawDatalen + CMD_FRAME_DATA_START) || dataLength > CMD_FRAME_MAX_LEN)     //the datalength received is not the same as the package length.
            {
                memset(mRecvBuffer, 0, CMD_FRAME_MAX_LEN);
                ROS_WARN("dataLength is error");
                return ExitSuccess;
            }

            if(subPkgNum > CMD_FRAME_MAX_SUB_PKG_NUM || subPkgNum < CMD_FRAME_MIN_SUB_PKG_NUM || subPkgIndex > CMD_FRAME_MAX_SUB_PKG_NUM - 1)
            {
                memset(mRecvBuffer, 0, CMD_FRAME_MAX_LEN);
                ROS_WARN("dataLength is error");
                return ExitSuccess;
            }

            ROS_DEBUG("%d-%d%d%d%d", frameTotalIndex, subPkgNum, subPkgIndex, subPkgType, rawDatalen);

            mDataSaveSt[subPkgIndex].totaIndexlCount = frameTotalIndex;
            mDataSaveSt[subPkgIndex].subPkgNum = subPkgNum;
            mDataSaveSt[subPkgIndex].subPkgIndex = subPkgIndex;
            mDataSaveSt[subPkgIndex].subPkgType = subPkgType;
            mDataSaveSt[subPkgIndex].rawDataLen = rawDatalen;
            memcpy(mDataSaveSt[subPkgIndex].sens_data, mRecvBuffer + CMD_FRAME_DATA_START, rawDatalen);

            bool checkResult = false;
            for(n = 0; n < subPkgNum - 1; n++)
            {
                if(mDataSaveSt[n].totaIndexlCount != mDataSaveSt[n + 1].totaIndexlCount || 
                        mDataSaveSt[n].subPkgIndex != mDataSaveSt[n + 1].subPkgIndex - 1)
                {
                    checkResult = true;
                    break;
                }
            }

            if(checkResult == true)
            {
                //ROS_WARN("data rev not complete !!");
                return ExitSuccess;
            }

            totalDataLen = 0;
            addInfoLen = 0;
            p_data = (unsigned char *)mStoreBuffer;
            p_info = (unsigned char *)mStoreBuffer2;
            for(n = 0; n < subPkgNum; n++)
            {
                if((0x01 == mDataSaveSt[n].subPkgType) || (0x02 == mDataSaveSt[n].subPkgType))
                {
                    memcpy((unsigned char *)(p_data + totalDataLen), mDataSaveSt[n].sens_data, mDataSaveSt[n].rawDataLen);
                    totalDataLen += mDataSaveSt[n].rawDataLen;
                }
                else if(0x03 == mDataSaveSt[n].subPkgType)
                {
                    memcpy((unsigned char *)(p_info + addInfoLen), mDataSaveSt[n].sens_data, mDataSaveSt[n].rawDataLen);
                    addInfoLen += mDataSaveSt[n].rawDataLen;
                }
            }

            ROS_DEBUG("totalDataLen = %d- addInfoLen = %d", totalDataLen, addInfoLen);

            if(frameTotalIndex != lastFrameTotalIndex + 1 && frameTotalIndex != 0)
            {
                ROS_WARN("frameTotalIndex:%d is out-of-order, last is:%d", frameTotalIndex, lastFrameTotalIndex);
            }

            lastFrameTotalIndex = frameTotalIndex;
            memset(mRecvBuffer, 0, CMD_FRAME_MAX_LEN);
        }
        else    //header error
        {
            memset(mRecvBuffer, 0, CMD_FRAME_MAX_LEN);
            ROS_WARN("command header is error!!");
            return ExitSuccess;
        }
    }

    /*Data requested, skip frames*/
    if(iteration_count++ % (mConfig.skip + 1) != 0)
    {
        ROS_DEBUG("laser - Skip frame");
        return ExitSuccess;
    }

    sensor_msgs::LaserScan msg;
    int success = mParser->Parse(mStoreBuffer, (totalDataLen >> 2), mStoreBuffer2, addInfoLen, mConfig, msg);
    if(ExitSuccess == success)
    {
        if(mConfig.debug_mode)
        {
            DumpLaserMessage(msg);
        }
        mDiagPublisher->publish(msg);
    }

    memset(mStoreBuffer, 0, RECV_BUFFER_SIZE);

    return ExitSuccess; // return success to continue
}

void ClaserCommon::CheckAngleRange(laser_udp::laserConfig &config)
{
    if(config.min_ang > config.max_ang)
    {
        ROS_WARN("Minimum angle must be greater than maxmum angle. Adjusting min_ang");
        config.min_ang = config.max_ang;
    }
}

void ClaserCommon::UpdateConfig(laser_udp::laserConfig &newConfig, uint32_t level)
{
    CheckAngleRange(newConfig);
    mConfig = newConfig;
}

void ClaserCommon::DumpLaserMessage(sensor_msgs::LaserScan &msg)
{
    ROS_DEBUG("Laser Message to send:");
    ROS_DEBUG("Header  frame_id: %s", msg.header.frame_id.c_str());
    //ROS_DEBUG("Header timestamp: %ld", msg.header.stamp);
    ROS_DEBUG("angle_min: %f", msg.angle_min);
    ROS_DEBUG("angle_max: %f", msg.angle_max);
    ROS_DEBUG("angle_increment: %f", msg.angle_increment);
    ROS_DEBUG("time_increment: %f", msg.time_increment);
    ROS_DEBUG("scan_time: %f", msg.scan_time);
    ROS_DEBUG("range_min: %f", msg.range_min);
    ROS_DEBUG("range_max: %f", msg.range_max);
}

void ClaserCommon::ClearConnectFlag(void)
{
    mConnectFlag = 0;
}

} // laser_udp
