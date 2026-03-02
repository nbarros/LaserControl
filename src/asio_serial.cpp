#include <asio_serial/serial.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <memory>
#include <thread>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

uint32_t compute_timeout_ms(uint32_t constant, uint32_t multiplier, size_t size)
{
  const uint64_t total = static_cast<uint64_t>(constant) +
                         static_cast<uint64_t>(multiplier) * static_cast<uint64_t>(size);
  return total > std::numeric_limits<uint32_t>::max()
           ? std::numeric_limits<uint32_t>::max()
           : static_cast<uint32_t>(total);
}

}

namespace serial 
{

class Serial::Impl 
{
public:
  struct PendingTransaction 
  {
    PendingTransaction() : max_size(0) {}

    PendingTransaction(const std::string& request_in,
                       const std::string& response_eol_in,
                       size_t max_size_in,
                       const Serial::TransactionCallback& callback_in)
      : request(request_in),
        response_eol(response_eol_in),
        max_size(max_size_in),
        callback(callback_in)
    {}

    std::string request;
    std::string response_eol;
    size_t max_size;
    Serial::TransactionCallback callback;
  };

  Impl(const std::string& port,
       uint32_t baudrate,
       bytesize_t bytesize,
       parity_t parity,
       stopbits_t stopbits,
       flowcontrol_t flowcontrol)
    : io_(),
      serial_port_(io_),
      port_(port),
      baudrate_(baudrate),
      bytesize_(bytesize),
      parity_(parity),
      stopbits_(stopbits),
      flowcontrol_(flowcontrol),
      timeout_(),
      work_guard_(new boost::asio::io_context::work(io_)),
      tx_timer_(io_),
      tx_in_progress_(false),
      tx_sequence_(0)
  {
    io_thread_ = std::thread([this]() { io_.run(); });
    if (!port_.empty()) 
    {
      open();
    }
  }

  ~Impl()
  {
    close();
    work_guard_.reset();
    io_.stop();
    if (io_thread_.joinable()) 
    {
      io_thread_.join();
    }
  }

  void open()
  {
    if (port_.empty()) 
    {
      throw std::invalid_argument("Empty port is invalid.");
    }
    if (serial_port_.is_open()) 
    {
      throw SerialException("Serial port already open.");
    }

    boost::system::error_code ec;
    serial_port_.open(port_, ec);
    if (ec) 
    {
      throw IOException(__FILE__, __LINE__, ec.value());
    }

    applyPortConfig();
#if !defined(_WIN32)
    const int flags = ::fcntl(serial_port_.native_handle(), F_GETFL, 0);
    if (flags == -1) 
    {
      close();
      throw IOException(__FILE__, __LINE__, errno);
    }
    if (::fcntl(serial_port_.native_handle(), F_SETFL, flags | O_NONBLOCK) == -1) 
    {
      close();
      throw IOException(__FILE__, __LINE__, errno);
    }
#endif
  }

  void close()
  {
    if (!serial_port_.is_open()) 
    {
      return;
    }
    boost::system::error_code ec;
    serial_port_.close(ec);
  }

  bool isOpen() const { return serial_port_.is_open(); }

  size_t available()
  {
    ensureOpen();
#if defined(_WIN32)
    return 0;
#else
    int bytes_available = 0;
    if (::ioctl(serial_port_.native_handle(), FIONREAD, &bytes_available) == -1) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
    return bytes_available > 0 ? static_cast<size_t>(bytes_available) : 0;
#endif
  }

  bool waitReadable(uint32_t timeout_ms)
  {
    ensureOpen();
    if (available() > 0) 
    {
      return true;
    }
    if (timeout_ms == 0) 
    {
      return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) 
    {
      if (available() > 0) 
      {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return available() > 0;
  }

  void waitByteTimes(size_t count)
  {
    const unsigned int parity_bits = (parity_ == parity_none) ? 0U : 1U;
    const unsigned int stop_bits = (stopbits_ == stopbits_two) ? 2U : 1U;
    const unsigned int bits_per_char = 1U + static_cast<unsigned int>(bytesize_) + parity_bits + stop_bits;
    if (baudrate_ == 0 || bits_per_char == 0 || count == 0) 
    {
      return;
    }

    const auto micros = static_cast<uint64_t>(count) * bits_per_char * 1000000ULL /
                        static_cast<uint64_t>(baudrate_);
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
  }

  size_t read(uint8_t* buffer, size_t size)
  {
    ensureOpen();
    if (size == 0) 
    {
      return 0;
    }

    const uint32_t total_timeout_ms =
      compute_timeout_ms(timeout_.read_timeout_constant, timeout_.read_timeout_multiplier, size);
    const bool has_total_timeout = total_timeout_ms != Timeout::max();
    const bool has_interbyte_timeout = timeout_.inter_byte_timeout != 0 &&
                                       timeout_.inter_byte_timeout != Timeout::max();

    const auto start = std::chrono::steady_clock::now();
    auto last_byte_time = start;

    size_t total_read = 0;
    while (total_read < size) {
      const size_t bytes_ready = available();
      if (bytes_ready > 0) 
      {
        const size_t chunk = std::min(bytes_ready, size - total_read);
        boost::system::error_code ec;
        const size_t n = serial_port_.read_some(boost::asio::buffer(buffer + total_read, chunk), ec);

        if (ec) {
          if (ec == boost::asio::error::would_block ||
              ec == boost::asio::error::try_again ||
              ec == boost::asio::error::interrupted) 
          {

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
          }
          throw IOException(__FILE__, __LINE__, ec.value());
        }

        if (n > 0) 
        {
          total_read += n;
          last_byte_time = std::chrono::steady_clock::now();
          continue;
        }
      }

      const auto now = std::chrono::steady_clock::now();
      if (has_total_timeout && now - start >= std::chrono::milliseconds(total_timeout_ms)) 
      {
        break;
      }
      if (has_interbyte_timeout && total_read > 0 &&
          now - last_byte_time >= std::chrono::milliseconds(timeout_.inter_byte_timeout)) 
      {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return total_read;
  }

  size_t write(const uint8_t* data, size_t size)
  {
    ensureOpen();
    if (size == 0) 
    {
      return 0;
    }

    const uint32_t total_timeout_ms =
      compute_timeout_ms(timeout_.write_timeout_constant, timeout_.write_timeout_multiplier, size);
    const bool has_timeout = total_timeout_ms != Timeout::max();
    const auto start = std::chrono::steady_clock::now();

    size_t total_written = 0;
    while (total_written < size) 
    {
      boost::system::error_code ec;
      const size_t n = serial_port_.write_some(boost::asio::buffer(data + total_written, size - total_written), ec);

      if (ec) 
      {
        if (ec == boost::asio::error::would_block ||
            ec == boost::asio::error::try_again ||
            ec == boost::asio::error::interrupted) 
        {
          if (has_timeout && std::chrono::steady_clock::now() - start >=
                               std::chrono::milliseconds(total_timeout_ms)) 
          {
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        throw IOException(__FILE__, __LINE__, ec.value());
      }

      total_written += n;
      if (has_timeout && std::chrono::steady_clock::now() - start >=
                           std::chrono::milliseconds(total_timeout_ms)) 
      {
        break;
      }
    }

    return total_written;
  }

  void asyncTransaction(const std::string& request,
                        const Serial::TransactionCallback& callback,
                        const std::string& response_eol,
                        size_t max_size)
  {
    const Serial::TransactionCallback safe_callback = callback ? callback :
      [](bool, const std::string&) {};

    boost::asio::post(io_, [this, request, safe_callback, response_eol, max_size]() 
    {
      tx_queue_.push_back(PendingTransaction(request, response_eol, max_size, safe_callback));
      if (!tx_in_progress_) 
      {
        startNextTransaction();
      }
    });
  }

  void setPort(const std::string& port) { port_ = port; }
  std::string getPort() const { return port_; }

  void setTimeout(Timeout timeout) { timeout_ = timeout; }
  Timeout getTimeout() const { return timeout_; }

  void setBaudrate(uint32_t baudrate) { baudrate_ = baudrate; if (isOpen()) applyPortConfig(); }
  uint32_t getBaudrate() const { return baudrate_; }

  void setBytesize(bytesize_t bytesize) { bytesize_ = bytesize; if (isOpen()) applyPortConfig(); }
  bytesize_t getBytesize() const { return bytesize_; }

  void setParity(parity_t parity) { parity_ = parity; if (isOpen()) applyPortConfig(); }
  parity_t getParity() const { return parity_; }

  void setStopbits(stopbits_t stopbits) { stopbits_ = stopbits; if (isOpen()) applyPortConfig(); }
  stopbits_t getStopbits() const { return stopbits_; }

  void setFlowcontrol(flowcontrol_t flowcontrol) { flowcontrol_ = flowcontrol; if (isOpen()) applyPortConfig(); }
  flowcontrol_t getFlowcontrol() const { return flowcontrol_; }

  void flush()
  {
    flushInput();
    flushOutput();
  }

  void flushInput()
  {
#if !defined(_WIN32)
    ensureOpen();
    if (::tcflush(serial_port_.native_handle(), TCIFLUSH) != 0) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
#endif
  }

  void flushOutput()
  {
#if !defined(_WIN32)
    ensureOpen();
    if (::tcflush(serial_port_.native_handle(), TCOFLUSH) != 0) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
#endif
  }

  void sendBreak(int duration)
  {
#if !defined(_WIN32)
    ensureOpen();
    if (::tcsendbreak(serial_port_.native_handle(), duration) != 0) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
#endif
  }

  void setBreak(bool level)
  {
#if !defined(_WIN32)
    ensureOpen();
    const int request = level ? TIOCSBRK : TIOCCBRK;
    if (::ioctl(serial_port_.native_handle(), request) != 0) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
#endif
  }

  void setRTS(bool level)
  {
#if !defined(_WIN32)
    set_modem_bit(TIOCM_RTS, level);
#endif
  }

  void setDTR(bool level)
  {
#if !defined(_WIN32)
    set_modem_bit(TIOCM_DTR, level);
#endif
  }

  bool waitForChange()
  {
#if defined(_WIN32)
    return false;
#else
    ensureOpen();
    const int initial = get_modem_bits();
    const uint32_t timeout_ms = timeout_.read_timeout_constant;
    if (timeout_ms == 0) {
      return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) 
    {
      if (get_modem_bits() != initial) 
      {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
#endif
  }

  bool getCTS()
  {
#if defined(_WIN32)
    return false;
#else
    return (get_modem_bits() & TIOCM_CTS) != 0;
#endif
  }

  bool getDSR()
  {
#if defined(_WIN32)
    return false;
#else
    return (get_modem_bits() & TIOCM_DSR) != 0;
#endif
  }

  bool getRI()
  {
#if defined(_WIN32)
    return false;
#else
    return (get_modem_bits() & TIOCM_RI) != 0;
#endif
  }

  bool getCD()
  {
#if defined(_WIN32)
    return false;
#else
    return (get_modem_bits() & TIOCM_CD) != 0;
#endif
  }

  std::mutex read_mutex_;
  std::mutex write_mutex_;

private:
  void startNextTransaction()
  {
    if (tx_queue_.empty()) {
      tx_in_progress_ = false;
      active_tx_.reset();
      return;
    }

    tx_in_progress_ = true;
    ++tx_sequence_;
    active_tx_.reset(new PendingTransaction(tx_queue_.front()));
    tx_queue_.pop_front();

    if (!isOpen()) {
      finishTransaction(false, std::string());
      return;
    }

    read_buffer_.consume(read_buffer_.size());

    const uint32_t timeout_ms = timeout_.read_timeout_constant;
    if (timeout_ms > 0 && timeout_ms != Timeout::max()) 
    {
      tx_timer_.expires_from_now(std::chrono::milliseconds(timeout_ms));
      const uint64_t sequence = tx_sequence_;
      tx_timer_.async_wait([this, sequence](const boost::system::error_code& ec) 
      {
        if (ec || !transactionActive(sequence)) 
        {
          return;
        }
        boost::system::error_code cancel_ec;
        serial_port_.cancel(cancel_ec);
      });
    }

    const uint64_t sequence = tx_sequence_;
    boost::asio::async_write(
      serial_port_,
      boost::asio::buffer(active_tx_->request),
      [this, sequence](const boost::system::error_code& ec, size_t) 
      {
        if (!transactionActive(sequence)) 
        {
          return;
        }

        if (ec) 
        {
          finishTransaction(false, std::string());
          return;
        }

        boost::asio::async_read_until(
          serial_port_,
          read_buffer_,
          active_tx_->response_eol,
          [this, sequence](const boost::system::error_code& read_ec, size_t bytes_transferred) 
          {
            if (!transactionActive(sequence)) 
            {
              return;
            }

            if (read_ec) 
            {
              finishTransaction(false, std::string());
              return;
            }

            if (active_tx_->max_size != 0 && bytes_transferred > active_tx_->max_size) 
            {
              finishTransaction(false, std::string());
              return;
            }

            std::istream stream(&read_buffer_);
            std::string response(bytes_transferred, '\0');
            stream.read(&response[0], bytes_transferred);
            finishTransaction(true, response);
          });
      });
  }

  bool transactionActive(uint64_t sequence) const
  {
    return tx_in_progress_ && sequence == tx_sequence_ && active_tx_.get() != nullptr;
  }

  void finishTransaction(bool success, const std::string& response)
  {
    tx_timer_.cancel();

    PendingTransaction finished = *active_tx_;
    active_tx_.reset();
    tx_in_progress_ = false;

    try
    {
      finished.callback(success, response);
    }
    catch (...)
    {
    }
    startNextTransaction();
  }

  void ensureOpen() const
  {
    if (!isOpen()) 
    {
      throw PortNotOpenedException("Serial port is not open");
    }
  }

  void applyPortConfig()
  {
    boost::system::error_code ec;

    serial_port_.set_option(boost::asio::serial_port_base::baud_rate(baudrate_), ec);
    if (ec) 
    {
      throw IOException(__FILE__, __LINE__, ec.value());
    }

    serial_port_.set_option(boost::asio::serial_port_base::character_size(static_cast<unsigned int>(bytesize_)), ec);
    if (ec) 
    {
      throw IOException(__FILE__, __LINE__, ec.value());
    }

    boost::asio::serial_port_base::parity::type parity_type;
    switch (parity_) {
      case parity_none: parity_type = boost::asio::serial_port_base::parity::none; break;
      case parity_odd: parity_type = boost::asio::serial_port_base::parity::odd; break;
      case parity_even: parity_type = boost::asio::serial_port_base::parity::even; break;
      case parity_mark:
      case parity_space:
      default:
        throw std::invalid_argument("Unsupported parity setting");
    }
    serial_port_.set_option(boost::asio::serial_port_base::parity(parity_type), ec);
    if (ec) 
    {
      throw IOException(__FILE__, __LINE__, ec.value());
    }

    boost::asio::serial_port_base::stop_bits::type stop_bits_type;
    switch (stopbits_) 
    {
      case stopbits_one: stop_bits_type = boost::asio::serial_port_base::stop_bits::one; break;
      case stopbits_one_point_five: stop_bits_type = boost::asio::serial_port_base::stop_bits::onepointfive; break;
      case stopbits_two: stop_bits_type = boost::asio::serial_port_base::stop_bits::two; break;
      default: throw std::invalid_argument("Unsupported stop bits setting");
    }
    serial_port_.set_option(boost::asio::serial_port_base::stop_bits(stop_bits_type), ec);
    if (ec) 
    {
      throw IOException(__FILE__, __LINE__, ec.value());
    }

    boost::asio::serial_port_base::flow_control::type flow_control_type;
    switch (flowcontrol_) 
    {
      case flowcontrol_none: flow_control_type = boost::asio::serial_port_base::flow_control::none; break;
      case flowcontrol_software: flow_control_type = boost::asio::serial_port_base::flow_control::software; break;
      case flowcontrol_hardware: flow_control_type = boost::asio::serial_port_base::flow_control::hardware; break;
      default: throw std::invalid_argument("Unsupported flow control setting");
    }
    serial_port_.set_option(boost::asio::serial_port_base::flow_control(flow_control_type), ec);
    if (ec) 
    {
      throw IOException(__FILE__, __LINE__, ec.value());
    }
  }

#if !defined(_WIN32)
  int get_modem_bits()
  {
    ensureOpen();
    int status_bits = 0;
    if (::ioctl(serial_port_.native_handle(), TIOCMGET, &status_bits) != 0) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
    return status_bits;
  }

  void set_modem_bit(int bit, bool level)
  {
    int status = get_modem_bits();
    if (level) status |= bit;
    else status &= ~bit;

    if (::ioctl(serial_port_.native_handle(), TIOCMSET, &status) != 0) 
    {
      throw IOException(__FILE__, __LINE__, errno);
    }
  }
#endif

  boost::asio::io_context io_;
  boost::asio::serial_port serial_port_;
  std::unique_ptr<boost::asio::io_context::work> work_guard_;
  std::thread io_thread_;
  boost::asio::steady_timer tx_timer_;
  boost::asio::streambuf read_buffer_;
  std::deque<PendingTransaction> tx_queue_;
  std::unique_ptr<PendingTransaction> active_tx_;
  bool tx_in_progress_;
  uint64_t tx_sequence_;

  std::string port_;
  uint32_t baudrate_;
  bytesize_t bytesize_;
  parity_t parity_;
  stopbits_t stopbits_;
  flowcontrol_t flowcontrol_;
  Timeout timeout_;
};

Serial::Serial(const std::string& port,
               uint32_t baudrate,
               Timeout timeout,
               bytesize_t bytesize,
               parity_t parity,
               stopbits_t stopbits,
               flowcontrol_t flowcontrol)
  : m_impl(new Impl(port, baudrate, bytesize, parity, stopbits, flowcontrol))
{
  m_impl->setTimeout(timeout);
}

Serial::~Serial() { delete m_impl; }

void Serial::open() { m_impl->open(); }
bool Serial::isOpen() const { return m_impl->isOpen(); }
void Serial::close() { m_impl->close(); }

size_t Serial::available() { return m_impl->available(); }
bool Serial::waitReadable() { return m_impl->waitReadable(m_impl->getTimeout().read_timeout_constant); }
void Serial::waitByteTimes(size_t count) { m_impl->waitByteTimes(count); }

size_t Serial::read(uint8_t* buffer, size_t size)
{
  std::lock_guard<std::mutex> lock(m_impl->read_mutex_);
  return m_impl->read(buffer, size);
}

size_t Serial::read(std::vector<uint8_t>& buffer, size_t size)
{
  std::lock_guard<std::mutex> lock(m_impl->read_mutex_);
  std::vector<uint8_t> local(size);
  const size_t bytes_read = m_impl->read(local.data(), size);
  buffer.insert(buffer.end(), local.begin(), local.begin() + bytes_read);
  return bytes_read;
}

size_t Serial::read(std::string& buffer, size_t size)
{
  std::lock_guard<std::mutex> lock(m_impl->read_mutex_);
  std::vector<uint8_t> local(size);
  const size_t bytes_read = m_impl->read(local.data(), size);
  if (bytes_read > 0) {
    buffer.append(reinterpret_cast<const char*>(local.data()), bytes_read);
  }
  return bytes_read;
}

std::string Serial::read(size_t size)
{
  std::string buffer;
  read(buffer, size);
  return buffer;
}

size_t Serial::readline(std::string& buffer, size_t size, std::string eol)
{
  std::lock_guard<std::mutex> lock(m_impl->read_mutex_);
  const size_t eol_len = eol.length();
  std::vector<uint8_t> local;
  local.reserve(size);

  while (local.size() < size) {
    uint8_t byte = 0;
    const size_t bytes_read = m_impl->read(&byte, 1);
    if (bytes_read == 0) {
      break;
    }

    local.push_back(byte);
    if (local.size() >= eol_len) 
    {
      const char* end = reinterpret_cast<const char*>(local.data() + local.size() - eol_len);
      if (std::string(end, eol_len) == eol) 
      {
        break;
      }
    }
  }

  if (!local.empty()) {
    buffer.append(reinterpret_cast<const char*>(local.data()), local.size());
  }
  return local.size();
}

std::string Serial::readline(size_t size, std::string eol)
{
  std::string buffer;
  readline(buffer, size, eol);
  return buffer;
}

std::vector<std::string> Serial::readlines(size_t size, std::string eol)
{
  std::lock_guard<std::mutex> lock(m_impl->read_mutex_);
  std::vector<std::string> lines;
  std::vector<uint8_t> local;
  local.reserve(size);

  const size_t eol_len = eol.length();
  size_t start_of_line = 0;

  while (local.size() < size) 
  {
    uint8_t byte = 0;
    const size_t bytes_read = m_impl->read(&byte, 1);
    if (bytes_read == 0) 
    {
      if (start_of_line < local.size()) 
      {
        lines.emplace_back(reinterpret_cast<const char*>(local.data() + start_of_line),
                           local.size() - start_of_line);
      }
      break;
    }

    local.push_back(byte);
    if (local.size() >= eol_len) {
      const char* end = reinterpret_cast<const char*>(local.data() + local.size() - eol_len);
      if (std::string(end, eol_len) == eol) 
      {
        lines.emplace_back(reinterpret_cast<const char*>(local.data() + start_of_line),
                           local.size() - start_of_line);
        start_of_line = local.size();
      }
    }
  }

  if (local.size() == size && start_of_line < local.size()) 
  {
    lines.emplace_back(reinterpret_cast<const char*>(local.data() + start_of_line),
                       local.size() - start_of_line);
  }

  return lines;
}

size_t Serial::write(const uint8_t* data, size_t size)
{
  std::lock_guard<std::mutex> lock(m_impl->write_mutex_);
  return m_impl->write(data, size);
}

size_t Serial::write(const std::vector<uint8_t>& data)
{
  std::lock_guard<std::mutex> lock(m_impl->write_mutex_);
  return data.empty() ? 0 : m_impl->write(data.data(), data.size());
}

size_t Serial::write(const std::string& data)
{
  std::lock_guard<std::mutex> lock(m_impl->write_mutex_);
  return m_impl->write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void Serial::asyncTransaction(const std::string& request,
                              const TransactionCallback& callback,
                              std::string response_eol,
                              size_t max_size)
{
  m_impl->asyncTransaction(request, callback, response_eol, max_size);
}

bool Serial::transaction(const std::string& request,
                         std::string& response,
                         std::string response_eol,
                         size_t max_size)
{
  struct WaitState {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    bool success = false;
    std::string response;
  };

  std::shared_ptr<WaitState> wait_state = std::make_shared<WaitState>();

  asyncTransaction(
    request,
    [wait_state](bool tx_success, const std::string& tx_response) 
    {
      std::lock_guard<std::mutex> lock(wait_state->mutex);
      wait_state->success = tx_success;
      wait_state->response = tx_response;
      wait_state->done = true;
      wait_state->cv.notify_one();
    },
    response_eol,
    max_size);

  std::unique_lock<std::mutex> lock(wait_state->mutex);
  const uint32_t timeout_ms = getTimeout().read_timeout_constant;
  if (timeout_ms > 0 && timeout_ms != Timeout::max()) 
  {
    const bool completed = wait_state->cv.wait_for(lock,
                                                   std::chrono::milliseconds(timeout_ms),
                                                   [wait_state]() { return wait_state->done; });
    if (!completed) 
    {
      return false;
    }
  } else 
  {
    wait_state->cv.wait(lock, [wait_state]() { return wait_state->done; });
  }

  if (wait_state->success) 
  {
    response = wait_state->response;
  }
  return wait_state->success;
}

void Serial::setPort(const std::string& port)
{
  std::lock_guard<std::mutex> rlock(m_impl->read_mutex_);
  std::lock_guard<std::mutex> wlock(m_impl->write_mutex_);
  const bool was_open = m_impl->isOpen();
  if (was_open) 
  {
    m_impl->close();
  }
  m_impl->setPort(port);
  if (was_open) 
  {
    m_impl->open();
  }
}

std::string Serial::getPort() const { return m_impl->getPort(); }

void Serial::setTimeout(Timeout& timeout) { m_impl->setTimeout(timeout); }
Timeout Serial::getTimeout() const { return m_impl->getTimeout(); }

void Serial::setBaudrate(uint32_t baudrate) { m_impl->setBaudrate(baudrate); }
uint32_t Serial::getBaudrate() const { return m_impl->getBaudrate(); }

void Serial::setBytesize(bytesize_t bytesize) { m_impl->setBytesize(bytesize); }
bytesize_t Serial::getBytesize() const { return m_impl->getBytesize(); }

void Serial::setParity(parity_t parity) { m_impl->setParity(parity); }
parity_t Serial::getParity() const { return m_impl->getParity(); }

void Serial::setStopbits(stopbits_t stopbits) { m_impl->setStopbits(stopbits); }
stopbits_t Serial::getStopbits() const { return m_impl->getStopbits(); }

void Serial::setFlowcontrol(flowcontrol_t flowcontrol) { m_impl->setFlowcontrol(flowcontrol); }
flowcontrol_t Serial::getFlowcontrol() const { return m_impl->getFlowcontrol(); }

void Serial::flush()
{
  std::lock_guard<std::mutex> rlock(m_impl->read_mutex_);
  std::lock_guard<std::mutex> wlock(m_impl->write_mutex_);
  m_impl->flush();
}

void Serial::flushInput()
{
  std::lock_guard<std::mutex> lock(m_impl->read_mutex_);
  m_impl->flushInput();
}

void Serial::flushOutput()
{
  std::lock_guard<std::mutex> lock(m_impl->write_mutex_);
  m_impl->flushOutput();
}

void Serial::sendBreak(int duration) { m_impl->sendBreak(duration); }
void Serial::setBreak(bool level) { m_impl->setBreak(level); }
void Serial::setRTS(bool level) { m_impl->setRTS(level); }
void Serial::setDTR(bool level) { m_impl->setDTR(level); }

bool Serial::waitForChange() { return m_impl->waitForChange(); }
bool Serial::getCTS() { return m_impl->getCTS(); }
bool Serial::getDSR() { return m_impl->getDSR(); }
bool Serial::getRI() { return m_impl->getRI(); }
bool Serial::getCD() { return m_impl->getCD(); }

} // namespace serial
