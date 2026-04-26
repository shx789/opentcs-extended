
#include "xadz_lifting_ros/lifting_ros_pkg.h"

void serialInit(void)
{
  if(ctrl.step.finish() == true) {return;}

  ctrl.step.create = serialCreate();
  if(ctrl.step.create == false) {serialReset(); return;}

  ctrl.step.close = serialClose();
  if(ctrl.step.close == false) {serialReset(); return;}

  ctrl.step.open = serialOpen();
  if(ctrl.step.open == false) {serialReset(); return;}
}

void serialReset(void)
{
    delete ctrl.port.ptr;
    ctrl.port.ptr = nullptr;
    ctrl.step.clean();
    ctrl.port.rx.clear();
    for(auto &item : ctrl.port.tx) {item.clear();}
    ctrl.port.tx.clear();
    pkgLoopReset();
}

bool serialCreate(void)
{
  try
  {
    logs_info("serial port try create.");

    ctrl.port.ptr = new serial::Serial(ctrl.param.port, ctrl.param.baudrate, serial::Timeout::simpleTimeout(1000), \
                                        serial::bytesize_t::eightbits, serial::parity_t::parity_none, \
                                        serial::stopbits_t::stopbits_one, serial::flowcontrol_t::flowcontrol_none);
  }
  catch(const std::exception& e)
  {
    logs_error("serial port create fail.");

    logs_error_stream(e.what());

    return false;
  }

  logs_info("serial port create succeed.");

  return true;
}

bool serialClose(void)
{
  try
  {
    logs_info("serial port try close.");

    ctrl.port.ptr->close();
  }
  catch(const std::exception& e)
  {
    logs_error("serial port close fail.");

    logs_error_stream(e.what());

    return false;
  }

  logs_info("serial port close succeed.");

  return true;
}

bool serialOpen(void)
{
  try
  {
    logs_info("serial port try open.");

    ctrl.port.ptr->open();
  }
  catch(const std::exception& e)
  {
    logs_error("serial port open fail.");

    logs_error_stream(e.what());

    return false;
  }

  logs_info("serial port open succeed.");

  return true;
}

bool serialLink(void)
{
  int link_state = 0;

  link_state = access(ctrl.param.port.c_str(), F_OK);

  if(link_state == 0) {return true;}

  if(link_state == -1) {return false;}

  return false;
}

void serialRead(void)
{
  if(ctrl.step.finish() != true) {return;}

  size_t available_num = 0;

  try
  {
    /* logs_info("serial port try get available."); */

    available_num = ctrl.port.ptr->available();
  }
  catch(const std::exception& e)
  {
    logs_error("serial port get available fail.");

    logs_error_stream(e.what());

    serialReset();

    return;
  }

  size_t read_byte = 0;
  uint8_t array[available_num] = {0};

  try
  {
    /* logs_info("serial port try read."); */

    read_byte = ctrl.port.ptr->read(array, available_num);
  }
  catch(const std::exception& e)
  {
    logs_error("serial port read fail.");

    logs_error_stream(e.what());

    serialReset();

    return;
  }

  /* logs_info("serial port read succeed."); */

  for(size_t loop = 0; loop < read_byte; loop++)
  {
    ctrl.port.rx.push_back(array[loop]);
  }

  return;
}

void serialWrite(void)
{
  /* write cycle use 1ms */

  if(ctrl.step.finish() != true) {return;}

  if(ctrl.port.tx.size() == 0) {return;}

  size_t tx_size = 0;

  tx_size = ctrl.port.tx.at(0).size();

  if(tx_size == 0) {return;}

  size_t write_byte = 0;
  uint8_t array[tx_size] = {0};

  for(size_t loop = 0; loop < tx_size; loop++)
  {
    array[loop] = ctrl.port.tx.at(0).at(loop);
  }

  try
  {
    /* logs_info("serial port try write."); */

    write_byte = ctrl.port.ptr->write(array, tx_size);
  }
  catch(const std::exception& e)
  {
    logs_error("serial port write fail.");

    logs_error_stream(e.what());

    serialReset();

    return;
  }

  if(write_byte == tx_size)
  {
    /* logs_info("serial port write succeed."); */

    ctrl.port.tx.at(0).clear();
    ctrl.port.tx.pop_front();
  }
}

void pkgReceive(void)
{
  if(ctrl.step.finish() != true) {return;}

  std::deque<uint8_t> &rx_buffer = ctrl.port.rx;

  while(rx_buffer.size() > 6)
  {
    if(rx_buffer.at(0) != 0xDE) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(1) != 0xED) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(2) < 6) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.size() < rx_buffer.at(2)) {return;}

    uint16_t sum = 0;

    for(size_t loop = 0; loop < (rx_buffer.at(2) - 2); loop++) {sum = sum + rx_buffer.at(loop);}

    if(rx_buffer.at(rx_buffer.at(2) - 2) != ((sum >> 0) & 0xFF)) {rx_buffer.pop_front(); continue;}

    if(rx_buffer.at(rx_buffer.at(2) - 1) != ((sum >> 8) & 0xFF)) {rx_buffer.pop_front(); continue;}

    if((ctrl.param.logs == true) && (ctrl.param.original == true))
    {
      printf("rx frame: ");
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      {
        printf("%02X ", rx_buffer.at(loop));
      }
      printf("\n");
    }

    pkgProcess();

    for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
    {
      rx_buffer.pop_front();
    }

    /* std::cout << "rx buffe size: " << rx_buffer.size() << std::endl; */
  }
}

void pkgProcess(void)
{
  std::deque<uint8_t> &rx_buffer = ctrl.port.rx;

  switch((ReceiveCmd)rx_buffer.at(3))
  {
    case ReceiveCmd::upload:
    {
      std_msgs::UInt8MultiArray frame;
      frame.data.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      frame.data.push_back(rx_buffer.at(loop));
      ctrl.pub.frame.publish(frame);

      ctrl.pkg.upload.setResponse();

      pkgInfoDisplay(upload, 1, "set upload success.");

      break;
    }
    case ReceiveCmd::state:
    {
      std_msgs::UInt8MultiArray frame;
      frame.data.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      frame.data.push_back(rx_buffer.at(loop));
      ctrl.pub.frame.publish(frame);

      std_msgs::UInt8 mode;
      std_msgs::Bool limit_up;
      std_msgs::Bool limit_down;
      std_msgs::Bool drive_error;
      std_msgs::Bool encoder_online;
      std_msgs::UInt8 position_action;
      std_msgs::Int16 position_target;
      std_msgs::Int16 position_real;
      std_msgs::Int8 velocity_target;
      std_msgs::Int8 velocity_real;

      mode.data = static_cast<uint8_t>((rx_buffer.at(4) >> 0) & 0x01);
      limit_up.data = static_cast<bool>((rx_buffer.at(4) >> 1) & 0x01);
      limit_down.data = static_cast<bool>((rx_buffer.at(4) >> 2) & 0x01);
      drive_error.data = static_cast<bool>((rx_buffer.at(4) >> 3) & 0x01);
      encoder_online.data = static_cast<bool>((rx_buffer.at(4) >> 4) & 0x01);

      position_action.data = static_cast<uint8_t>(rx_buffer.at(5));
      position_target.data = static_cast<int16_t>((rx_buffer.at(7) << 8) | (rx_buffer.at(6) << 0));
      position_real.data = static_cast<int16_t>((rx_buffer.at(9) << 8) | (rx_buffer.at(8) << 0));
      velocity_target.data = static_cast<int8_t>(rx_buffer.at(10));
      velocity_real.data = static_cast<int8_t>(rx_buffer.at(11));

      ctrl.pub.mode.publish(mode);
      ctrl.pub.limit_up.publish(limit_up);
      ctrl.pub.limit_down.publish(limit_down);
      ctrl.pub.drive_error.publish(drive_error);
      ctrl.pub.encoder_online.publish(encoder_online);
      ctrl.pub.position_action.publish(position_action);
      ctrl.pub.position_target.publish(position_target);
      ctrl.pub.position_real.publish(position_real);
      ctrl.pub.velocity_target.publish(velocity_target);
      ctrl.pub.velocity_real.publish(velocity_real);

      ctrl.pkg.get_state.setResponse();

      pkgInfoDisplay(state, 100, "get state success.");

      break;
    }
    case ReceiveCmd::ctrl:
    {
      std_msgs::UInt8MultiArray frame;
      frame.data.clear();
      for(size_t loop = 0; loop < rx_buffer.at(2); loop++)
      frame.data.push_back(rx_buffer.at(loop));
      ctrl.pub.frame.publish(frame);

      pkgInfoDisplay(ctrl, 100, "set ctrl success.");

      break;
    }
    default:
    {
      break;
    }
  }
}

void pkgFormatFrame(TransmitCmd cmd, size_t data_len, uint8_t *data_buffe, std::vector<uint8_t> &tx_buffer)
{
  size_t loop = 0;
  uint16_t sum_value = 0;

  tx_buffer.clear();

  tx_buffer.push_back(0xDE);
  tx_buffer.push_back(0xED);
  tx_buffer.push_back(6 + data_len);
  tx_buffer.push_back((uint8_t)cmd);

  for(loop = 0; loop < data_len; loop++)
  {
    tx_buffer.push_back(data_buffe[loop]);
  }

  for(loop = 0; loop < tx_buffer.size(); loop++)
  {
    sum_value = sum_value + tx_buffer.at(loop);
  }

  tx_buffer.push_back((sum_value >> 0) & 0xFF);
  tx_buffer.push_back((sum_value >> 8) & 0xFF);

  #if 0
  printf("tx frame: ");
  for(loop = 0; loop < tx_buffer.size(); loop++)
  {printf("%02X ", tx_buffer.at(loop));}
  printf("\n");
  #endif
}

void pkgSetUpload(bool enable)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint8_t *enable_ptr = nullptr;

  enable_ptr = (uint8_t *)&data_buffer[0];

  *enable_ptr = enable;

  pkgFormatFrame(TransmitCmd::upload, 8, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetVelocity(int8_t percent)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint8_t *mode_ptr = nullptr;
  int8_t *percent_ptr = nullptr;

  mode_ptr = (uint8_t *)&data_buffer[0];
  percent_ptr = (int8_t *)&data_buffer[1];

  *mode_ptr = 0x00;
  *percent_ptr = percent;

  pkgFormatFrame(TransmitCmd::ctrl, 8, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetPosition(uint8_t percent, int16_t position)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint8_t *mode_ptr = nullptr;
  uint8_t *percent_ptr = nullptr;
  int16_t *position_ptr = nullptr;

  mode_ptr = (uint8_t *)&data_buffer[0];
  percent_ptr = (uint8_t *)&data_buffer[1];
  position_ptr = (int16_t *)&data_buffer[2];

  *mode_ptr = 0x01;
  *percent_ptr = percent;
  *position_ptr = position;

  pkgFormatFrame(TransmitCmd::ctrl, 8, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgSetZero(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  uint8_t *mode_ptr = nullptr;

  mode_ptr = (uint8_t *)&data_buffer[0];

  *mode_ptr = 0xFF;

  pkgFormatFrame(TransmitCmd::ctrl, 8, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgGetState(void)
{
  std::vector<uint8_t> tx_buffer;
  uint8_t data_buffer[8] = {0};

  pkgFormatFrame(TransmitCmd::state, 8, data_buffer, tx_buffer);
  ctrl.port.tx.push_back(tx_buffer);
}

void pkgLoopInit(void)
{
  ctrl.pkg.step = 0;

  /* get once */

  //

  /* upload */

  ctrl.pkg.upload.setOnce(true);
  ctrl.pkg.upload.setTimeout(1000);
  ctrl.pkg.upload.flagClean();

  /* loop get */

  //

  /* not use */

  ctrl.pkg.get_state.setOnce(true);
  ctrl.pkg.get_state.setTimeout(1000);
  ctrl.pkg.get_state.flagClean();

}

void pkgLoopReset(void)
{
  ctrl.pkg.step = 0;

  //

  ctrl.pkg.upload.flagClean();

  //

  ctrl.pkg.get_state.flagClean();
}

void pkgGetLoop(void)
{
  if(ctrl.step.finish() != true) {return;}

  PkgGetCode code = PkgGetCode::wait;

  for(size_t step_loop = 0; step_loop < ctrl.pkg.max_num; step_loop++)
  {
    /* get once */

    //

    /* upload */

    if(ctrl.pkg.step == 0)
    {
      code = pkgGetCheck(ctrl.pkg.upload, ctrl.pkg.step);
      if(code == PkgGetCode::send)    {pkgSetUpload(true);}
      if(code == PkgGetCode::send)    {logs_info("try open state upload."); break;}
      if(code == PkgGetCode::get)     {logs_info("open state upload success."); break;}
      if(code == PkgGetCode::timeout) {logs_error("open state upload timeout."); break;}
    }

    /* loop get */

    //

    /* not use */

    else if(ctrl.pkg.step == ((size_t)-1))
    {
      code = pkgGetCheck(ctrl.pkg.get_state, ctrl.pkg.step);
      if(code == PkgGetCode::send)    {pkgGetState();}
      if(code == PkgGetCode::send)    {logs_info("try get state."); break;}
      if(code == PkgGetCode::get)     {logs_info("get state success."); break;}
      if(code == PkgGetCode::timeout) {logs_error("get state timeout."); break;}
    }

    if(code == PkgGetCode::get) {break;}
    if(code == PkgGetCode::wait) {break;}
    if(code == PkgGetCode::skip) {continue;}
  }

  if(ctrl.pkg.step == ctrl.pkg.max_num)
  {
    ctrl.pkg.step = 0;
  }
}

PkgGetCode pkgGetCheck(PkgGet &pkg, uint32_t &step)
{
  if((pkg.do_once == true) && (pkg.flag.once == true))
  {
    step++;
    return PkgGetCode::skip;
  }

  if(pkg.flag.request == false)
  {
    pkg.flag.request = true;
    pkg.flag.response = false;
    pkg.flag.timeout_cnt = 0;
    return PkgGetCode::send;
  }

  if(pkg.flag.request == true)
  {
    pkg.flag.timeout_cnt++;

    if(pkg.flag.response == true)
    {
      step++;
      pkg.flag.once = true;
      pkg.flag.request = false;
      pkg.flag.response = false;
      pkg.flag.timeout_cnt = 0;
      return PkgGetCode::get;
    }

    if(pkg.flag.timeout_cnt > ((pkg.timeout_ms < ctrl.pkg.cycle) ? 1 : (pkg.timeout_ms / ctrl.pkg.cycle)))
    {
      step++;
      pkg.flag.request = false;
      pkg.flag.response = false;
      pkg.flag.timeout_cnt = 0;
      return PkgGetCode::timeout;
    }

    if(pkg.flag.response == false)
    {
      return PkgGetCode::wait;
    }
  }

  return PkgGetCode::wait;
}
