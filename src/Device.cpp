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
        m_com_post("\r"),
        m_timeout_ms(50)
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
#ifdef DEBUG
    std::cout << "sending command [" << util::escape(msg.c_str()) << "]" << std::endl;
#endif
    size_t written_bytes = m_serial.write(msg);
  #ifdef DEBUG
    std::cout << "Device::write_cmd : Wrote "<< written_bytes << " bytes" << std::endl;
  #endif
    // NFB : -- this is not true
    // -- delay of 50 ms between commands is required
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  void Device::read_cmd(std::string &answer)
  {
    size_t nbytes = m_serial.readline(answer,0xFFFF,m_com_post);
    // one should remove the chars
#ifdef DEBUG
    std::cout << "Received " << nbytes << " bytes answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif

    // trim the suffix chars
    answer.erase(answer.size()-m_com_post.size());
  }


} /* namespace device */
