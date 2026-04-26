#ifndef PARSER_BASE__
#define PARSER_BASE__

#include "laser_udp/laserConfig.h"
#include "sensor_msgs/LaserScan.h"

namespace laser_udp
{
enum ExitCode
{
    ExitSuccess = 0,
    ExitError   = 1,
    ExitFatal   = 2
};

class CParserBase
{
public:
    CParserBase();
    virtual ~CParserBase();

    virtual int Parse(unsigned short *data,
                      size_t data_length,
                      unsigned char *info,
                      size_t info_length,
                      laserConfig &config,
                      sensor_msgs::LaserScan &msg) = 0;
};
} /*namespace laser_udp*/

#endif /*PARSER_BASE__*/
