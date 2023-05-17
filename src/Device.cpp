/*
 * Device.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <Device.hh>
#ifdef DEBUG
#include <iostream>
#endif
namespace device
{

  Device::Device (const char* port, const uint32_t baud_rate)
      : m_comport(port),
        m_baud(baud_rate),
        m_com_pre(""),
        m_com_post("\r"),
        m_timeout_ms(1000)
  {
    // initialize the serial connection
    m_serial.setPort(m_comport);
    m_serial.setBaudrate(m_baud);
    m_serial.setBytesize(serial::eightbits);
    m_serial.setParity(serial::parity_none);
    serial::Timeout t = serial::Timeout::simpleTimeout(m_timeout_ms);
    m_serial.setTimeout(t);
    m_serial.setStopbits(serial::stopbits_one);


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
    // first flush any pending buffers
    m_serial.flush();
    std::string msg = m_com_pre + cmd + m_com_post;
    size_t written_bytes = m_serial.write(msg);
  #ifdef DEBUG
    std::cout << "Device::write_cmd : Wrote "<< written_bytes << " bytes" << std::endl;
  #endif
  }

  void Device::read_cmd(std::string &answer)
  {
    answer = m_serial.readline(0xFFFF,m_com_post);
  }


} /* namespace device */
