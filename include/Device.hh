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
#include <serial/serial.h>

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

    bool is_open() {return m_serial.isOpen();}

    void close() {m_serial.close();}

    void set_timeout(const uint32_t ms) { m_timeout_ms = ms; }
    void get_timeout(uint32_t &ms) {ms = m_timeout_ms;}

    void set_com_prefix(const std::string pre) {m_com_pre = pre;}
    void set_com_suffix(const std::string suf) {m_com_sfx = suf;}
    void set_read_suffix(const std::string suf) {m_read_sfx = suf;}

    const std::string get_port() {return m_comport;}
    const uint32_t get_baud() {return m_baud;}

    void read_lines(std::vector<std::string> &lines);
    void set_timeout_ms(uint32_t t);

  protected:
    /// local member declaration
    ///
    void write_cmd(const std::string cmd);

    void read_cmd(std::string &answer);

    std::string m_comport;
    uint32_t m_baud;

    std::string m_com_pre;
    std::string m_com_sfx;
    std::string m_read_sfx;
    //
    uint32_t m_timeout_ms;
    serial::Serial m_serial;


  private:

    Device (const Device &other) = delete;
    Device (Device &&other) = delete;
    Device& operator= (const Device &other) = delete;
    Device& operator= (Device &&other) = delete;


  };

} /* namespace device */

#endif /* INCLUDE_DEVICE_HH_ */
