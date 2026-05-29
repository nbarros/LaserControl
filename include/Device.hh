/*
 * Device.hh
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#ifndef INCLUDE_DEVICE_HH_
#define INCLUDE_DEVICE_HH_
#include <string>
#include <cstdint>
#include <mutex>
#include <cstddef>
#include <asio_serial/serial.hpp>

//#define DEBUG 1
namespace device
{

  class Device
  {
  public:
    enum RetStatus {Success=0, Failed=0x1};

    Device ( ) {};

    Device (const char* port, const uint32_t baud_rate);
    virtual ~Device ();

    bool is_open();

    void close();

    void set_timeout(const uint32_t ms) { m_timeout_ms = ms; }
    void get_timeout(uint32_t &ms) {ms = m_timeout_ms;}

    void set_com_prefix(const std::string pre) {m_com_pre = pre;}

    void set_request_suffix(const std::string suffix) { m_request_suffix = suffix; }
    void set_response_suffix(const std::string suffix) { m_response_suffix = suffix; }

    void set_com_suffix(const std::string suffix) { set_request_suffix(suffix); }
    void set_read_suffix(const std::string suffix) { set_response_suffix(suffix); }

    const std::string get_port() {return m_comport;}
    const uint32_t get_baud() {return m_baud;}

    bool read_lines(std::vector<std::string> &lines);
    void set_timeout_ms(uint32_t t);

  protected:
    /// local member declaration
    ///
    bool write_cmd(const std::string cmd, bool allow_inactive = false);

    bool read_cmd(std::string &answer, bool allow_inactive = false);

    bool exchange_cmd(const std::string cmd, std::string &answer, bool allow_inactive = false);

    bool probe_connection(const std::string& probe_cmd,
                std::size_t retries = 2,
                uint32_t backoff_ms = 50);

    bool try_recover_connection(std::size_t retries = 1,
                  uint32_t backoff_ms = 50);

    bool is_online();

    std::recursive_mutex& io_mutex() { return m_io_mutex; }

    void reset_connection();

    std::string m_comport;
    uint32_t m_baud;

    std::string m_com_pre;
    std::string m_request_suffix;
    std::string m_response_suffix;
    //
    uint32_t m_timeout_ms;
    serial::Serial m_serial;
    std::recursive_mutex m_io_mutex;
    bool m_is_online;
    uint32_t m_consecutive_failures;
    bool m_user_requested_close;
    std::string m_probe_cmd;


  private:

    Device (const Device &other) = delete;
    Device (Device &&other) = delete;
    Device& operator= (const Device &other) = delete;
    Device& operator= (Device &&other) = delete;


  };

} /* namespace device */

#endif /* INCLUDE_DEVICE_HH_ */
