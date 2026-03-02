#ifndef ASIO_SERIAL_SERIAL_HPP_
#define ASIO_SERIAL_SERIAL_HPP_

#include <limits>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <cstdint>
#include <functional>

namespace serial {

typedef enum {
  fivebits = 5,
  sixbits = 6,
  sevenbits = 7,
  eightbits = 8
} bytesize_t;

typedef enum {
  parity_none = 0,
  parity_odd = 1,
  parity_even = 2,
  parity_mark = 3,
  parity_space = 4
} parity_t;

typedef enum {
  stopbits_one = 1,
  stopbits_two = 2,
  stopbits_one_point_five
} stopbits_t;

typedef enum {
  flowcontrol_none = 0,
  flowcontrol_software,
  flowcontrol_hardware
} flowcontrol_t;

struct Timeout {
#ifdef max
# undef max
#endif
  static uint32_t max() {return std::numeric_limits<uint32_t>::max();}

  static Timeout simpleTimeout(uint32_t timeout)
  {
    return Timeout(max(), timeout, 0, timeout, 0);
  }

  uint32_t inter_byte_timeout;
  uint32_t read_timeout_constant;
  uint32_t read_timeout_multiplier;
  uint32_t write_timeout_constant;
  uint32_t write_timeout_multiplier;

  explicit Timeout(uint32_t inter_byte_timeout_ = 0,
                   uint32_t read_timeout_constant_ = 0,
                   uint32_t read_timeout_multiplier_ = 0,
                   uint32_t write_timeout_constant_ = 0,
                   uint32_t write_timeout_multiplier_ = 0)
    : inter_byte_timeout(inter_byte_timeout_),
      read_timeout_constant(read_timeout_constant_),
      read_timeout_multiplier(read_timeout_multiplier_),
      write_timeout_constant(write_timeout_constant_),
      write_timeout_multiplier(write_timeout_multiplier_)
  {}
};

class SerialException : public std::exception
{
  SerialException& operator=(const SerialException&);
  std::string e_what_;
public:
  explicit SerialException(const char* description)
  {
    std::stringstream ss;
    ss << "SerialException " << description << " failed.";
    e_what_ = ss.str();
  }
  SerialException(const SerialException& other) : e_what_(other.e_what_) {}
  virtual ~SerialException() throw() {}
  virtual const char* what() const throw() { return e_what_.c_str(); }
};

class IOException : public std::exception
{
  IOException& operator=(const IOException&);
  std::string file_;
  int line_;
  std::string e_what_;
  int errno_;
public:
  explicit IOException(std::string file, int line, int errnum)
    : file_(file), line_(line), errno_(errnum)
  {
    std::stringstream ss;
#if defined(_WIN32) && !defined(__MINGW32__)
    char error_str [1024];
    strerror_s(error_str, 1024, errnum);
#else
    char* error_str = strerror(errnum);
#endif
    ss << "IO Exception (" << errno_ << "): " << error_str;
    ss << ", file " << file_ << ", line " << line_ << ".";
    e_what_ = ss.str();
  }

  explicit IOException(std::string file, int line, const char* description)
    : file_(file), line_(line), errno_(0)
  {
    std::stringstream ss;
    ss << "IO Exception: " << description;
    ss << ", file " << file_ << ", line " << line_ << ".";
    e_what_ = ss.str();
  }

  virtual ~IOException() throw() {}
  IOException(const IOException& other)
    : line_(other.line_), e_what_(other.e_what_), errno_(other.errno_)
  {}

  int getErrorNumber() const { return errno_; }
  virtual const char* what() const throw() { return e_what_.c_str(); }
};

class PortNotOpenedException : public std::exception
{
  const PortNotOpenedException& operator=(PortNotOpenedException);
  std::string e_what_;
public:
  explicit PortNotOpenedException(const char* description)
  {
    std::stringstream ss;
    ss << "PortNotOpenedException " << description << " failed.";
    e_what_ = ss.str();
  }
  PortNotOpenedException(const PortNotOpenedException& other) : e_what_(other.e_what_) {}
  virtual ~PortNotOpenedException() throw() {}
  virtual const char* what() const throw() { return e_what_.c_str(); }
};

struct PortInfo {
  std::string port;
  std::string description;
  std::string hardware_id;
};

class Serial {
public:
  typedef std::function<void(bool, const std::string&)> TransactionCallback;

  Serial(const std::string& port = "",
         uint32_t baudrate = 9600,
         Timeout timeout = Timeout(),
         bytesize_t bytesize = eightbits,
         parity_t parity = parity_none,
         stopbits_t stopbits = stopbits_one,
         flowcontrol_t flowcontrol = flowcontrol_none);

  virtual ~Serial();

  void open();
  bool is_open() const { return isOpen(); }
  bool isOpen() const;
  void close();

  size_t available();
  bool wait_readable() { return waitReadable(); }
  bool waitReadable();
  void wait_byte_times(size_t count) { waitByteTimes(count); }
  void waitByteTimes(size_t count);

  size_t read(uint8_t* buffer, size_t size);
  size_t read(std::vector<uint8_t>& buffer, size_t size = 1);
  size_t read(std::string& buffer, size_t size = 1);
  std::string read(size_t size = 1);

  size_t readline(std::string& buffer, size_t size = 65536, std::string eol = "\n");
  std::string readline(size_t size = 65536, std::string eol = "\n");
  std::vector<std::string> readlines(size_t size = 65536, std::string eol = "\n");

  size_t write(const uint8_t* data, size_t size);
  size_t write(const std::vector<uint8_t>& data);
  size_t write(const std::string& data);

  void async_transaction(const std::string& request,
                         const TransactionCallback& callback,
                         std::string response_eol = "\n",
                         size_t max_size = 65536)
  {
    asyncTransaction(request, callback, response_eol, max_size);
  }

  void asyncTransaction(const std::string& request,
                        const TransactionCallback& callback,
                        std::string response_eol = "\n",
                        size_t max_size = 65536);

  bool transaction(const std::string& request,
                   std::string& response,
                   std::string response_eol = "\n",
                   size_t max_size = 65536);

  void set_port(const std::string& port) { setPort(port); }
  void setPort(const std::string& port);
  std::string get_port() const { return getPort(); }
  std::string getPort() const;

  void set_timeout(Timeout& timeout) { setTimeout(timeout); }
  void setTimeout(Timeout& timeout);
  void set_timeout(uint32_t inter_byte_timeout,
                   uint32_t read_timeout_constant,
                   uint32_t read_timeout_multiplier,
                   uint32_t write_timeout_constant,
                   uint32_t write_timeout_multiplier)
  {
    setTimeout(inter_byte_timeout,
               read_timeout_constant,
               read_timeout_multiplier,
               write_timeout_constant,
               write_timeout_multiplier);
  }
  void setTimeout(uint32_t inter_byte_timeout,
                  uint32_t read_timeout_constant,
                  uint32_t read_timeout_multiplier,
                  uint32_t write_timeout_constant,
                  uint32_t write_timeout_multiplier)
  {
    Timeout timeout(inter_byte_timeout, read_timeout_constant,
                    read_timeout_multiplier, write_timeout_constant,
                    write_timeout_multiplier);
    return setTimeout(timeout);
  }

  Timeout get_timeout() const { return getTimeout(); }
  Timeout getTimeout() const;

  void set_baudrate(uint32_t baudrate) { setBaudrate(baudrate); }
  void setBaudrate(uint32_t baudrate);
  uint32_t get_baudrate() const { return getBaudrate(); }
  uint32_t getBaudrate() const;

  void set_bytesize(bytesize_t bytesize) { setBytesize(bytesize); }
  void setBytesize(bytesize_t bytesize);
  bytesize_t get_bytesize() const { return getBytesize(); }
  bytesize_t getBytesize() const;

  void set_parity(parity_t parity) { setParity(parity); }
  void setParity(parity_t parity);
  parity_t get_parity() const { return getParity(); }
  parity_t getParity() const;

  void set_stopbits(stopbits_t stopbits) { setStopbits(stopbits); }
  void setStopbits(stopbits_t stopbits);
  stopbits_t get_stopbits() const { return getStopbits(); }
  stopbits_t getStopbits() const;

  void set_flowcontrol(flowcontrol_t flowcontrol) { setFlowcontrol(flowcontrol); }
  void setFlowcontrol(flowcontrol_t flowcontrol);
  flowcontrol_t get_flowcontrol() const { return getFlowcontrol(); }
  flowcontrol_t getFlowcontrol() const;

  void flush();
  void flush_input() { flushInput(); }
  void flushInput();
  void flush_output() { flushOutput(); }
  void flushOutput();

  void send_break(int duration) { sendBreak(duration); }
  void sendBreak(int duration);
  void set_break(bool level = true) { setBreak(level); }
  void setBreak(bool level = true);
  void set_rts(bool level = true) { setRTS(level); }
  void setRTS(bool level = true);
  void set_dtr(bool level = true) { setDTR(level); }
  void setDTR(bool level = true);

  bool wait_for_change() { return waitForChange(); }
  bool waitForChange();
  bool get_cts() { return getCTS(); }
  bool getCTS();
  bool get_dsr() { return getDSR(); }
  bool getDSR();
  bool get_ri() { return getRI(); }
  bool getRI();
  bool get_cd() { return getCD(); }
  bool getCD();

private:
  Serial(const Serial&);
  Serial& operator=(const Serial&);

  class Impl;
  Impl* m_impl;
};

std::vector<PortInfo> list_ports();

} // namespace serial

#endif // ASIO_SERIAL_SERIAL_HPP_
