#include <laser_udp/laser_sensor_frame.h>
#include <laser_udp/laser_constants.h>

namespace laser_udp
{
ClaserSensFrame::ClaserSensFrame()
{
    mSensDataLength = 0;

    m_pSensData  = NULL;
    m_pSensData2  = NULL;

    m_angleRes = 0;
    m_sampleNum = 0;
    m_addIntensity = 0;
    m_timeSample = 0;
}

ClaserSensFrame::~ClaserSensFrame()
{
    if(m_pSensData != NULL)
    {
        delete m_pSensData;
    }

    if(m_pSensData2 != NULL)
    {
        delete m_pSensData2;
    }
}

int  ClaserSensFrame::GetSensDataCount()
{
    return mSensDataLength;
}

int  ClaserSensFrame::GetSensInfoAngleRes()
{
    return m_angleRes;
}

int  ClaserSensFrame::GetSensInfoSampleNum()
{
    return m_sampleNum;
}

int  ClaserSensFrame::GetSensInfoAddIntensity()
{
    return m_addIntensity;
}

int  ClaserSensFrame::GetSensInfoTimeSample()
{
    return m_timeSample;
}

int  ClaserSensFrame::GetSensInfoScanTime()
{
    return m_scanTime;
}

uint16_t ClaserSensFrame::GetSensDataOfIndex(int index)
{
    if(index < 0 || index > mSensDataLength)
    {
        ROS_ERROR("Fail to get of index %d.", index);
        return 0;
    }

    return m_pSensData[index];
}

uint16_t ClaserSensFrame::GetSensIntensityOfIndex(int index)
{
    if(index < 0 || index > mSensDataLength)
    {
        ROS_ERROR("Fail to get of index %d.", index);
        return 0;
    }

    return m_pSensData2[index];
}

bool ClaserSensFrame::InitFromSensBuff(unsigned short *buff, int length,
        unsigned char *info, int info_length)
{
    if(buff == NULL)
    {
        ROS_ERROR("Invalide input buffer!");
        return false;
    }

    m_pSensData = new unsigned short[length];
    if(m_pSensData == NULL)
    {
        ROS_ERROR("Insufficiant memory!");
        return NULL;
    }

    m_pSensData2 = new unsigned short[length];
    if(m_pSensData2 == NULL)
    {
        ROS_ERROR("Insufficiant memory!");
        return NULL;
    }

    mSensDataLength = length;
    unsigned short *p_data = m_pSensData;
    for(int i = 0; i < length; i++)
    {
        *p_data++ = *buff++;
    }

    p_data = m_pSensData2;
    for(int i = 0; i < length; i++)
    {
        *p_data++ = *buff++;
    }

    info_length = info_length;  
    m_angleRes = info[72] << 24 | info[73] << 16 | info[74] << 8 | info[75];
    m_sampleNum = info[76] << 8 | info[77];
    m_addIntensity = 0X01;
    m_timeSample = info[100] | info[99] << 8 | info[98] << 16 | info[97] << 24;
    m_scanTime = info[94] | info[93] << 8 | info[92] << 16 | info[91] << 24;

    ROS_DEBUG("m_angleRes = %d - %d -%d - %d -%d", m_angleRes, m_sampleNum, m_addIntensity, m_timeSample, m_scanTime);

    return true;
}
} /*namespace laser_ls_udp*/
