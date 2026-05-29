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

  namespace {
    const uint32_t k_offline_failure_threshold = 3;
  }

  bool Device::is_open()
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    return m_serial.is_open();
  }

  void Device::close()
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    m_user_requested_close = true;
    if (m_serial.is_open()) {
      m_serial.close();
    }
    m_is_online = false;
  }

  Device::Device (const char* port, const uint32_t baud_rate)
      : m_comport(port),
        m_baud(baud_rate),
        m_com_pre(""),
        m_request_suffix("\r"),
      m_timeout_ms(100),
      m_is_online(false),
      m_consecutive_failures(0),
      m_user_requested_close(false),
      m_probe_cmd("")
  {

  }

  Device::~Device ()
  {
    close();
  }

  bool Device::write_cmd(const std::string cmd, bool allow_inactive)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (!m_is_online && !allow_inactive)
    {
      if (!try_recover_connection())
      {
        return false;
      }
    }
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
      m_consecutive_failures++;
      if (m_consecutive_failures >= k_offline_failure_threshold)
      {
        m_is_online = false;
      }
      return false;
    }
  #ifdef DEBUG
    std::cout << "Device::write_cmd : Wrote "<< written_bytes << " bytes" << std::endl;
  #endif
    m_consecutive_failures = 0;
    m_is_online = true;
    m_user_requested_close = false;
    return true;
    }



  void Device::set_timeout_ms(uint32_t t)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    m_timeout_ms = t;
    serial::Timeout to = serial::Timeout::simpleTimeout(m_timeout_ms);
    m_serial.set_timeout(to);

  #ifdef DEBUG
    std::cout << "Device::set_timeout_ms : Setting timeout to [" << t << "] ms" << std::endl;
  #endif

  }

  bool Device::read_cmd(std::string &answer, bool allow_inactive)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (!m_is_online && !allow_inactive)
    {
      if (!try_recover_connection())
      {
        return false;
      }
    }

    // m_serial.waitReadable()
    size_t nbytes = m_serial.readline(answer, 0xFFFF, m_response_suffix);
    // one should remove the chars
#ifdef DEBUG
    std::cout << "Device::read_cmd : Received " << nbytes << " bytes answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif

  // if we got no answer
  if (nbytes == 0) 
  {
    m_consecutive_failures++;
    if (m_consecutive_failures >= k_offline_failure_threshold)
    {
      m_is_online = false;
    }
    return false;
  }
  else if (nbytes < m_response_suffix.size())
  {
    m_consecutive_failures++;
    if (m_consecutive_failures >= k_offline_failure_threshold)
    {
      m_is_online = false;
    }
    return false;
  }
  // carefully validate and remove the response suffix
  // verify that the answer actually ends with the expected suffix
  if (answer.size() < m_response_suffix.size())
  {
#ifdef DEBUG
      std::cout << "Answer has less characters than suffix [ (got) " << answer.size() << " vs " << m_response_suffix.size() << "]" << std::endl;
#endif
      // in this case just trim special chars
      std::string ta = util::rtrim(answer);
      answer = ta;
    }
    else
    {
      // verify that the actual suffix matches expected suffix
      const std::string actual_suffix = answer.substr(answer.size() - m_response_suffix.size());
      if (actual_suffix != m_response_suffix)
      {
#ifdef DEBUG
        std::cout << "Device::read_cmd : WARNING: Actual suffix [" << util::escape(actual_suffix.c_str()) 
                  << "] does not match expected [" << util::escape(m_response_suffix.c_str()) << "]" << std::endl;
#endif
        // Still remove it anyway, but log the mismatch
      }
      // trim the suffix chars
      answer.erase(answer.size() - m_response_suffix.size());
    }
    m_consecutive_failures = 0;
    m_is_online = true;
    m_user_requested_close = false;
    return true;
  }

  bool Device::exchange_cmd(const std::string cmd, std::string &answer, bool allow_inactive)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (!m_is_online && !allow_inactive)
    {
      if (!try_recover_connection())
      {
        return false;
      }
    }
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
      m_consecutive_failures++;
      if (m_consecutive_failures >= k_offline_failure_threshold)
      {
        m_is_online = false;
      }
      return false;
    }

    answer = raw_answer;
    if (answer.size() >= m_response_suffix.size())
    {
      // Verify the actual suffix matches expected before removing
      const std::string actual_suffix = answer.substr(answer.size() - m_response_suffix.size());
      if (actual_suffix != m_response_suffix)
      {
#ifdef DEBUG
        std::cout << "Device::exchange_cmd : WARNING: Actual suffix [" << util::escape(actual_suffix.c_str()) 
                  << "] does not match expected [" << util::escape(m_response_suffix.c_str()) << "]" << std::endl;
#endif
      }
      answer.erase(answer.size() - m_response_suffix.size());
    }
    else
    {
      answer = util::rtrim(answer);
    }

#ifdef DEBUG
    std::cout << "Device::exchange_cmd : Received answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif
  m_consecutive_failures = 0;
  m_is_online = true;
  m_user_requested_close = false;
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
    if (m_user_requested_close)
    {
      return;
    }
    if (m_serial.is_open()) {
      m_serial.close();
    }
    m_serial.open();
    m_is_online = false;
    m_consecutive_failures = 0;
    m_user_requested_close = false;
  }

  bool Device::probe_connection(const std::string& probe_cmd,
                                std::size_t retries,
                                uint32_t backoff_ms)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);

    if (!probe_cmd.empty())
    {
      m_probe_cmd = probe_cmd;
    }

    std::string response;
    for (std::size_t attempt = 0; attempt <= retries; ++attempt)
    {
      const bool success = exchange_cmd(probe_cmd, response, true);
      if (success && !response.empty())
      {
        m_is_online = true;
        m_consecutive_failures = 0;
        m_user_requested_close = false;
        return true;
      }

      if (attempt < retries)
      {
        reset_connection();
        if (backoff_ms > 0)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms * (attempt + 1)));
        }
      }
    }

    m_is_online = false;
    return false;
  }

  bool Device::try_recover_connection(std::size_t retries, uint32_t backoff_ms)
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    if (m_is_online)
    {
      return true;
    }

    // Do not reconnect after an explicit user-requested close().
    if (m_user_requested_close)
    {
      return false;
    }

    // A probe command is required to confirm the device is truly responsive.
    if (m_probe_cmd.empty())
    {
      return false;
    }

    return probe_connection(m_probe_cmd, retries, backoff_ms);
  }

  bool Device::is_online()
  {
    std::lock_guard<std::recursive_mutex> lock(m_io_mutex);
    return m_is_online;
  }

} /* namespace device */
