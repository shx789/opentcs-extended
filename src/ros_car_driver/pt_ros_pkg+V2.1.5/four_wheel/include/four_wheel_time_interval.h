
#ifndef TIME_INTERVAL_H
#define TIME_INTERVAL_H

#include <ros/ros.h>

namespace time_interval
{
  enum class TimeType
  {
    nanoseconds,
    microseconds,
    milliseconds,
    seconds,
    minutes,
    hours,
  };

  class TimeInterval
  {
    public:

      void start(void)
      {
        begin_ = true;
        start_time_ = ros::Time::now();
      }

      void stop(void)
      {
        end_ = true;
        stop_time_ = ros::Time::now();
      }

      void clear(void)
      {
        begin_ = false;
        end_ = false;
      }

      bool timeout(TimeType type, int64_t time)
      {
        if(begin_ == false) {return false;}

        bool timeout = false;

        switch(type)
        {
          case TimeType::nanoseconds:  {(((ros::Time::now() - start_time_).toSec() * 1000000000) >= time) ? (timeout = true) : (timeout = false); break;}
          case TimeType::microseconds: {(((ros::Time::now() - start_time_).toSec() * 1000000   ) >= time) ? (timeout = true) : (timeout = false); break;}
          case TimeType::milliseconds: {(((ros::Time::now() - start_time_).toSec() * 1000      ) >= time) ? (timeout = true) : (timeout = false); break;}
          case TimeType::seconds:      {(((ros::Time::now() - start_time_).toSec() * 1         ) >= time) ? (timeout = true) : (timeout = false); break;}
          case TimeType::minutes:      {(((ros::Time::now() - start_time_).toSec() / 60        ) >= time) ? (timeout = true) : (timeout = false); break;}
          case TimeType::hours:        {(((ros::Time::now() - start_time_).toSec() / 3600      ) >= time) ? (timeout = true) : (timeout = false); break;}
        }

        return timeout;
      }

      double interval(TimeType type)
      {
        if((begin_ == false) || (end_ == false)) {return 0;}

        switch(type)
        {
          case TimeType::nanoseconds:  {return ((ros::Time::now() - start_time_).toSec() * 1000000000);}
          case TimeType::microseconds: {return ((ros::Time::now() - start_time_).toSec() * 1000000   );}
          case TimeType::milliseconds: {return ((ros::Time::now() - start_time_).toSec() * 1000      );}
          case TimeType::seconds:      {return ((ros::Time::now() - start_time_).toSec() * 1         );}
          case TimeType::minutes:      {return ((ros::Time::now() - start_time_).toSec() / 60        );}
          case TimeType::hours:        {return ((ros::Time::now() - start_time_).toSec() / 3600      );}
        }
      }

    private:

      bool begin_ = false;
      bool end_ = false;

      ros::Time start_time_;
      ros::Time stop_time_;
  };
}

#endif
