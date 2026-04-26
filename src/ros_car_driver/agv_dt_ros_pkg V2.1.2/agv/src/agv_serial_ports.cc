
#include <stdint.h>
#include <string.h>
#include <ros/ros.h>
#include <serial/serial.h>

#include "agv_fsm.h"
#include "agv_serial_ports.h"
#include "agv.h"

using TimeInterval = time_interval::TimeInterval;
using TimeIntervalType = time_interval::TimeType;

using FsmPort = fsm::Port::Fsm;
using FsmPortEvent = fsm::Port::Event;
using FsmPortState = fsm::Port::State;

using SerialPort = serial_ports::SerialPort;

SerialPort::SerialPort(std::string port, uint32_t timeout, uint32_t baudrate,
                       ByteSize byte_size, Parity parity, StopBit stop_bit, FlowCtrl flow_ctrl)
{
    param.port = port;
    param.baudrate = baudrate;
    param.byte_size = (serial::bytesize_t)byte_size;
    param.parity = (serial::parity_t)parity;
    param.stop_bits = (serial::stopbits_t)stop_bit;
    param.flow_control = (serial::flowcontrol_t)flow_ctrl;
    param.timeout = timeout;

    param.auto_restart = true;

    fsm_port_ptr_ = new FsmPort();
    fsm_port_ptr_->start();
}

SerialPort::~SerialPort(void)
{
    delete fsm_port_ptr_;
    delete serial_port_ptr_;
}

std::deque<uint8_t> &SerialPort::getReadBuffer(void)
{
    return ctrl.rw.read;
}

std::deque<uint8_t> &SerialPort::getWriteBuffer(void)
{
    return ctrl.rw.write;
}

std::string SerialPort::getFsmState(void)
{
    return fsm_port_ptr_->getNowState();
}

void SerialPort::scan(void)
{
  if(ctrl.exit == true) {return;}

  if(ctrl.skip.close == false) {close();}

  if(ctrl.skip.init == false) {init();}

  if(ctrl.skip.open == false) {open();}

  if(ctrl.skip.idle == false) {idle();}

  #if USE_SYSTEM_ACCESS_CHECK
  if(ctrl.skip.link == false) {link();}
  #endif

  if(ctrl.skip.read == false) {read();}

  if(ctrl.skip.write == false) {write();}

  if(ctrl.skip.restart == false) {restart();}

  /* logs_info_stream(fsm_port_ptr_->getNowState()); */
}

void SerialPort::close(void)
{
  if(fsm_port_ptr_->getNowState() == "close")
  {
    fsm_port_ptr_->dispatch(FsmPortEvent::Init());

    return;
  }
}

void SerialPort::init(void)
{
  if(fsm_port_ptr_->getNowState() == "init")
  {
    try
    {
      logs_info("serial port try init.");

      serial_port_ptr_ = new serial::Serial(param.port, param.baudrate, serial::Timeout::simpleTimeout(param.timeout), \
                                            param.byte_size, param.parity, param.stop_bits, param.flow_control);
    }
    catch(const std::exception& e)
    {
      logs_error_stream(e.what());

      fsm_port_ptr_->dispatch(FsmPortEvent::InitFail());

      return;
    }

    try
    {
      serial_port_ptr_->close();
    }
    catch(const std::exception& e)
    {
      logs_error_stream(e.what());
    }

    fsm_port_ptr_->dispatch(FsmPortEvent::InitSuccess());

    return;
  }

  if(fsm_port_ptr_->getNowState() == "init fail")
  {
    logs_error("serial port init fail.");

    if(param.auto_restart == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Restart());
    }

    if(param.auto_restart == false)
    {
      logs_error("serial port stop working.");

      ctrl.exit = true;
    }

    return;
  }

  if(fsm_port_ptr_->getNowState() == "init success")
  {
    logs_info("serial port init succeed.");

    fsm_port_ptr_->dispatch(FsmPortEvent::Open());

    ctrl.skip.init = true;

    return;
  }
}

void SerialPort::open(void)
{
  if(fsm_port_ptr_->getNowState() == "open")
  {
    try
    {
      logs_info("serial port try open.");

      serial_port_ptr_->open();
    }
    catch(const std::exception& e)
    {
      logs_error_stream(e.what());

      fsm_port_ptr_->dispatch(FsmPortEvent::OpenFail());

      return;
    }

    fsm_port_ptr_->dispatch(FsmPortEvent::OpenSuccess());

    return;
  }

  if(fsm_port_ptr_->getNowState() == "open fail")
  {
    logs_error("serial port open fail.");

    if(param.auto_restart == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Restart());
    }

    if(param.auto_restart == false)
    {
      logs_error("serial port stop working.");

      ctrl.exit = true;
    }

    return;
  }

  if(fsm_port_ptr_->getNowState() == "open success")
  {
    logs_info("serial port open succeed.");

    fsm_port_ptr_->dispatch(FsmPortEvent::Idle());

    ctrl.skip.open = true;

    return;
  }
}

void SerialPort::idle(void)
{
  if(fsm_port_ptr_->getNowState() == "idle")
  {
    #if USE_SYSTEM_ACCESS_CHECK

    fsm_port_ptr_->dispatch(FsmPortEvent::Link());

    #else

    if(ctrl.rw.sw == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Read());

      ctrl.rw.sw = false;

      return;
    }

    if(ctrl.rw.sw == false)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Write());

      ctrl.rw.sw = true;

      return;
    }

    #endif

    return;
  }
}

#if USE_SYSTEM_ACCESS_CHECK

void SerialPort::link(void)
{
  if(fsm_port_ptr_->getNowState() == "link")
  {
    int link_state = 0;

    link_state = access(param.port.c_str(), F_OK);

    if(link_state == 0)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::LinkSuccess());
    }

    if(link_state == -1)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::LinkFail());
    }

    return;
  }

  if(fsm_port_ptr_->getNowState() == "link fail")
  {
    logs_error("serial port link abnormal.");

    if(param.auto_restart == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Restart());
    }

    if(param.auto_restart == false)
    {
      logs_error("serial port stop working.");

      ctrl.exit = true;
    }

    return;
  }

  if(fsm_port_ptr_->getNowState() == "link success")
  {
    /* logs_info("serial port link normal."); */

    if(ctrl.rw.sw == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Read());

      ctrl.rw.sw = false;

      return;
    }

    if(ctrl.rw.sw == false)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Write());

      ctrl.rw.sw = true;

      return;
    }

    return;
  }
}

#endif

void SerialPort::read(void)
{
  if(fsm_port_ptr_->getNowState() == "read")
  {
    size_t available_num = 0;

    try
    {
      available_num = serial_port_ptr_->available();
    }
    catch(const std::exception& e)
    {
      logs_error_stream(e.what());

      fsm_port_ptr_->dispatch(FsmPortEvent::ReadFail());

      return;
    }

    if(available_num == 0)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::ReadSuccess());

      return;
    }

    size_t read_byte = 0;
    uint8_t array[available_num] = {0};

    try
    {
      /* logs_info("serial port try read."); */

      read_byte = serial_port_ptr_->read(array, available_num);
    }
    catch(const std::exception& e)
    {
      logs_error_stream(e.what());

      fsm_port_ptr_->dispatch(FsmPortEvent::ReadFail());

      return;
    }

    for(size_t loop = 0; loop < read_byte; loop++)
    {
      ctrl.rw.read.push_back(array[loop]);
    }

    fsm_port_ptr_->dispatch(FsmPortEvent::ReadSuccess());

    return;
  }

  if(fsm_port_ptr_->getNowState() == "read fail")
  {
    logs_error("serial port read fail.");

    if(param.auto_restart == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Restart());
    }

    if(param.auto_restart == false)
    {
      logs_error("serial port stop working.");

      ctrl.skip.init = false;
      ctrl.skip.open = false;

      ctrl.rw.sw = true;
      ctrl.rw.read.clear();
      ctrl.rw.write.clear();

      ctrl.exit = true;
    }

    return;
  }

  if(fsm_port_ptr_->getNowState() == "read success")
  {
    /* logs_info("serial port read succeed."); */

    fsm_port_ptr_->dispatch(FsmPortEvent::Idle());

    return;
  }
}

void SerialPort::write(void)
{
  if(fsm_port_ptr_->getNowState() == "write")
  {
    size_t tx_size = 0;

    tx_size = ctrl.rw.write.size();

    if(tx_size == 0)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::WriteSuccess());

      return;
    }

    size_t write_byte = 0;
    uint8_t array[tx_size] = {0};

    for(size_t loop = 0; loop < tx_size; loop++)
    {
      try
      {
        array[loop] = ctrl.rw.write.at(loop);
      }
      catch(const std::exception& e)
      {
        logs_error("serial port tx buffer access fail.");
        logs_error_stream(e.what());

        fsm_port_ptr_->dispatch(FsmPortEvent::WriteFail());

        return;
      }
    }

    try
    {
      /* logs_info("serial port try write."); */

      write_byte = serial_port_ptr_->write(array, tx_size);
    }
    catch(const std::exception& e)
    {
      logs_error_stream(e.what());

      fsm_port_ptr_->dispatch(FsmPortEvent::WriteFail());

      return;
    }

    for(size_t clean = 0; clean < write_byte; clean++)
    {
      ctrl.rw.write.pop_front();
    }

    fsm_port_ptr_->dispatch(FsmPortEvent::WriteSuccess());

    return;
  }

  if(fsm_port_ptr_->getNowState() == "write fail")
  {
    logs_error("serial port write fail.");

    if(param.auto_restart == true)
    {
      fsm_port_ptr_->dispatch(FsmPortEvent::Restart());
    }

    if(param.auto_restart == false)
    {
      logs_error("serial port stop working.");

      ctrl.skip.init = false;
      ctrl.skip.open = false;

      ctrl.rw.sw = true;
      ctrl.rw.read.clear();
      ctrl.rw.write.clear();

      ctrl.exit = true;
    }

    return;
  }

  if(fsm_port_ptr_->getNowState() == "write success")
  {
    /* logs_info("serial port write succeed."); */

    fsm_port_ptr_->dispatch(FsmPortEvent::Idle());

    return;
  }
}

void SerialPort::restart(void)
{
  if(fsm_port_ptr_->getNowState() == "restart")
  {
    if(ctrl.restart.begin == false)
    {
      logs_info("serial port restart.");

      ctrl.restart.begin = true;
      ctrl.restart.interval.clear();
      ctrl.restart.interval.start();

      return;
    }

    if(ctrl.restart.begin == true)
    {
      if(ctrl.restart.interval.timeout(TimeIntervalType::seconds, 2) == true)
      {
        ctrl.restart.begin = false;
        ctrl.restart.interval.clear();

        ctrl.skip.init = false;
        ctrl.skip.open = false;

        ctrl.rw.sw = true;
        ctrl.rw.read.clear();
        ctrl.rw.write.clear();

        fsm_port_ptr_->dispatch(FsmPortEvent::Close());
      }

      return;
    }

    return;
  }
}
