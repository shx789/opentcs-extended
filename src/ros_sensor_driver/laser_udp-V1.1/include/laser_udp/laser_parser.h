#ifndef LASER_PARSER__
#define LASER_PARSER__

#include <laser_udp/parser_base.h>

namespace laser_udp
{
class ClaserParser : public CParserBase
{
public:
    ClaserParser();
    virtual ~ClaserParser();

    virtual int Parse(unsigned short *data, size_t data_length, unsigned char *info, size_t info_length, laserConfig &config, sensor_msgs::LaserScan &msg);

    void SetRangeMin(float minRange);
    void SetRangeMax(float maxRange);
    void SetTimeIncrement(float time);
    void SetFrameId(std::string str);

private:
    float fRangeMin;
    float fRangeMax;
    float fTimeIncrement;
    std::string fFrame_id;
};
} /*namespace laser_udp*/

#endif /*laser_PARSER__*/
