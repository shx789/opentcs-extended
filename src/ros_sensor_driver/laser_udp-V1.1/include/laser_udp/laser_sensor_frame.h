#ifndef LASER_SENSOR_FRAME__
#define LASER_SENSOR_FRAME__

#include <stdio.h>
#include <stdlib.h>

#include <ros/ros.h>

namespace laser_udp
{
class ClaserSensFrame
{

public:
    ClaserSensFrame();
    ~ClaserSensFrame();

    bool     InitFromSensBuff(unsigned short *buff, int length, unsigned char *info, int info_length);

    /*Get sensor data count*/
    int      GetSensDataCount();

    int      GetSensInfoAngleRes();

    int     GetSensInfoSampleNum();

    int     GetSensInfoAddIntensity();

    int     GetSensInfoTimeSample();

    int     GetSensInfoScanTime();

    /*Get sensor data of index*/
    uint16_t GetSensDataOfIndex(int index);

    /*Get sensor intensity of index*/
    uint16_t GetSensIntensityOfIndex(int index);

private:
    unsigned short *m_pSensData;
    unsigned short *m_pSensData2;
    int            m_angleRes;
    int            m_sampleNum;
    int            m_addIntensity;
    int            m_timeSample;
    int            m_scanTime;

    int            mSensDataLength;
};
}

#endif /*laser_SENSOR_FRAME__*/
