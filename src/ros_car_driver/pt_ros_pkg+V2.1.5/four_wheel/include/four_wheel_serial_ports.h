
#ifndef SERIAL_PORTS_H
#define SERIAL_PORTS_H

#include <deque>
#include <serial/serial.h>

#include "four_wheel_fsm.h"
#include "four_wheel_time_interval.h"

namespace serial_ports
{

#define USE_SYSTEM_ACCESS_CHECK    (false)

enum class ByteSize
{
  five_bits = 5,
  six_bits = 6,
  seven_bits = 7,
  eight_bits = 8,
};

enum class Parity
{
  parity_none = 0,
  parity_odd = 1,
  parity_even = 2,
  parity_mark = 3,
  parity_space = 4
};

enum class StopBit
{
  stop_bit_one = 1,
  stop_bit_two = 2,
  stop_bit_one_point_five,
};

enum class FlowCtrl
{
  flow_ctrl_none = 0,
  flow_ctrl_software,
  flow_ctrl_hardware
};

struct Parameter
{
  std::string port;
  uint32_t baudrate;
  serial::bytesize_t byte_size;
  serial::parity_t parity;
  serial::stopbits_t stop_bits;
  serial::flowcontrol_t flow_control;
  uint32_t timeout;

  bool auto_restart;
};

struct Skip
{
  bool close = false;
  bool init = false;
  bool open = false;
  bool idle = false;

  #if USE_SYSTEM_ACCESS_CHECK
  bool link = false;
  #endif

  bool read = false;
  bool write = false;

  bool restart = false;
};

struct ReadWrite
{
  bool sw = true;

  std::deque<uint8_t> read;
  std::deque<uint8_t> write;
};

struct Restart
{
  bool begin = false;
  time_interval::TimeInterval interval;
};

struct Ctrl
{
  bool exit = false;

  Skip skip;
  ReadWrite rw;
  Restart restart;
};

class SerialPort
{
  public:

    SerialPort(std::string port, uint32_t timeout, uint32_t baudrate,
               ByteSize byte_size, Parity parity, StopBit stop_bit, FlowCtrl flow_ctrl);

    ~SerialPort();

    std::deque<uint8_t> &getReadBuffer(void);
    std::deque<uint8_t> &getWriteBuffer(void);

    std::string getFsmState(void);

    void scan(void);

  private:

    fsm::Port::Fsm *fsm_port_ptr_ = nullptr;
    serial::Serial *serial_port_ptr_ = nullptr;

    Parameter param;
    Ctrl ctrl;

    void close(void);
    void init(void);
    void open(void);
    void idle(void);

    #if USE_SYSTEM_ACCESS_CHECK
    void link(void);
    #endif

    void read(void);
    void write(void);

    void restart(void);
};

}

#endif
