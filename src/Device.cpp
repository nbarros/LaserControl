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
//#define DEBUG 1

#ifdef DEBUG
#include <iostream>
#endif

namespace device
{

  bool Device::is_open()
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    return m_serial.is_open();
  }

  void Device::close()
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (m_serial.is_open()) {
      m_serial.close();
    }
  }

  Device::Device (const char* port, const uint32_t baud_rate)
      : m_comport(port),
        m_baud(baud_rate),
        m_com_pre(""),
        m_request_suffix("\r"),
        m_timeout_ms(500)
  {

  }

  Device::~Device ()
  {
    close();
  }

  bool Device::write_cmd(const std::string cmd)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (!m_serial.is_open())
    {
      m_serial.open();
    }
    // drop any input that may be pending
    m_serial.flush_input();
    m_serial.flush_output();
    // first flush any pending buffers
    // m_serial.flush();

      std::string msg = m_com_pre + cmd + m_request_suffix;
#ifdef DEBUG
    std::cout << "Device::write_cmd : Sending command [" << util::escape(msg.c_str()) << "]" << std::endl;
#endif
    size_t written_bytes = m_serial.write(msg);
    if (written_bytes != msg.size())
    {
      return false;
    }
  #ifdef DEBUG
    std::cout << "Device::write_cmd : Wrote "<< written_bytes << " bytes" << std::endl;
  #endif
    return true;
    }



  void Device::set_timeout_ms(uint32_t t)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    serial::Timeout to = serial::Timeout::simpleTimeout(t);
  m_serial.set_timeout(to);

  #ifdef DEBUG
    std::cout << "Device::set_timeout_ms : Setting timeout to [" << t << "] ms" << std::endl;
  #endif

  }

  bool Device::read_cmd(std::string &answer)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);

    // m_serial.waitReadable()
    size_t nbytes = m_serial.readline(answer, 0xFFFF, m_response_suffix);
    // one should remove the chars
#ifdef DEBUG
    std::cout << "Device::read_cmd : Received " << nbytes << " bytes answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif

  // if we got no answer
  if (nbytes == 0) 
  {
    return false;
  }
  else if (nbytes < m_response_suffix.size())
  {
    return false;
  }
  // careful with the trim
  // if for some reason the answer is not valid, we can hit an exception here
  if (answer.size() < m_response_suffix.size())
  {
#ifdef DEBUG
      std::cout << "Answer has less characters than expected [ (got) " << answer.size() << " vs " << m_response_suffix.size() << "]" << std::endl;
#endif
      // in this case just trim special chars
      std::string ta = util::rtrim(answer);
      answer = ta;
    }
    else
    {
      // trim the suffix chars
      answer.erase(answer.size() - m_response_suffix.size());
    }
    return true;
  }

  bool Device::exchange_cmd(const std::string cmd, std::string &answer)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (!m_serial.is_open())
    {
      m_serial.open();
    }

    m_serial.flush_input();
    m_serial.flush_output();

    const std::string msg = m_com_pre + cmd + m_request_suffix;
#ifdef DEBUG
    std::cout << "Device::exchange_cmd : Sending command [" << util::escape(msg.c_str()) << "]" << std::endl;
#endif

    std::string raw_answer;
    const bool success = m_serial.transaction(msg, raw_answer, m_response_suffix, 0xFFFF);
    if (!success)
    {
      return false;
    }

    answer = raw_answer;
    if (answer.size() >= m_response_suffix.size())
    {
      answer.erase(answer.size() - m_response_suffix.size());
    }
    else
    {
      answer = util::rtrim(answer);
    }

#ifdef DEBUG
    std::cout << "Device::exchange_cmd : Received answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif
    return true;
  }

  bool Device::read_lines(std::vector<std::string> &lines)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    // wait for the port to be ready
    //size_t nbytes = 0;
    lines = m_serial.readlines(0xFFFF,m_response_suffix);
  #ifdef DEBUG
    std::cout << "Device::read_lines : Received " << lines.size() << " strings" << std::endl;
    for (auto entry: lines)
    {
      std::cout << "[" << util::escape(entry.c_str()) << "]" << std::endl;
    }
  #endif
  return true;
  }

  void Device::reset_connection()
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (m_serial.is_open()) {
      m_serial.close();
    }
    m_serial.open();
  }

} /* namespace device */
