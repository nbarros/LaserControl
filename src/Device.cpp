/*
 * Device.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <Device.hh>
#include <thread>
#include <chrono>
#include <utilities.hh>

#ifdef DEBUG
#include <iostream>
#endif

namespace device
{

  Device::Device (const char* port, const uint32_t baud_rate)
      : m_comport(port),
        m_baud(baud_rate),
        m_com_pre(""),
        m_com_sfx("\r"),
        m_timeout_ms(500)
  {

//    // initialize the serial connection
//    m_serial.setPort(m_comport);
//    m_serial.setBaudrate(m_baud);
//    m_serial.setBytesize(serial::eightbits);
//    m_serial.setParity(serial::parity_none);
//    serial::Timeout t = serial::Timeout::simpleTimeout(m_timeout_ms);
//    m_serial.setTimeout(t);
//    m_serial.setStopbits(serial::stopbits_one);
  }

  Device::~Device ()
  {
    if (m_serial.isOpen())
    {
      m_serial.close();
    }
  }

  void Device::write_cmd(const std::string cmd)
  {
    // drop any input that may be pending
    m_serial.flushInput();
    m_serial.flushOutput();
    // first flush any pending buffers
    //m_serial.flush();

    std::string msg = m_com_pre + cmd + m_com_sfx;
#ifdef DEBUG
    std::cout << "Device::write_cmd : Sending command [" << util::escape(msg.c_str()) << "]" << std::endl;
#endif
    size_t written_bytes = m_serial.write(msg);
  #ifdef DEBUG
    std::cout << "Device::write_cmd : Wrote "<< written_bytes << " bytes" << std::endl;
  #endif
  }

  void Device::read_cmd(std::string &answer)
  {
    //m_serial.waitReadable()
    size_t nbytes = m_serial.readline(answer,0xFFFF,m_read_sfx);
    // one should remove the chars
#ifdef DEBUG
    std::cout << "Device::read_cmd : Received " << nbytes << " bytes answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif

    // careful with the trim
    // if for some reason the answer is not valid, we can hit an exception here
    if (answer.size() > m_read_sfx.size())
    {
      std::cout << "Answer has less characters than expected [ (got) " << answer.size() << " vs " << m_read_sfx.size() << "]" << std::endl;
    }
    // trim the suffix chars
    answer.erase(answer.size()-m_read_sfx.size());
  }


  void Device::set_timeout_ms(uint32_t t)
  {
    serial::Timeout to = serial::Timeout::simpleTimeout(t);
  m_serial.setTimeout(to);

  #ifdef DEBUG
    std::cout << "Device::set_timeout_ms : Setting timeout to [" << t << "] ms" << std::endl;
  #endif

  }

  void Device::read_lines(std::vector<std::string> &lines)
  {
    // wait for the port to be ready
    //size_t nbytes = 0;
    lines = m_serial.readlines(0xFFFF,m_read_sfx);
  #ifdef DEBUG
    std::cout << "Device::read_lines : Received " << lines.size() << " strings" << std::endl;
  #endif

  }

} /* namespace device */
