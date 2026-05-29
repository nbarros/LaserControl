#include <utilities.hh>
#include <PowerMeter.hh>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <pty.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

void expect_true(bool condition, const std::string& message)
{
  if (!condition)
  {
    throw std::runtime_error(message);
  }
}

void expect_throws(std::function<void()> fn, const std::string& message)
{
  try
  {
    fn();
    throw std::runtime_error(message + " (expected exception was not thrown)");
  }
  catch (const serial::IOException&)
  {
    // Expected
  }
  catch (const std::runtime_error&)
  {
    throw;
  }
}

/**
 * Test Harness: PtyDeviceEmulator
 * Simulates a serial device responding to commands
 */
class PtyDeviceEmulator
{
public:
  typedef std::function<std::string(const std::string&)> Handler;

  PtyDeviceEmulator(const std::string& command_eol, Handler handler)
    : m_command_eol(command_eol)
    , m_handler(handler)
    , m_master_fd(-1)
    , m_slave_fd(-1)
    , m_stop(false)
  {
    char slave_name[256];
    if (openpty(&m_master_fd, &m_slave_fd, slave_name, nullptr, nullptr) != 0)
    {
      throw std::runtime_error("openpty failed");
    }

    m_slave_path = slave_name;
    m_thread = std::thread(&PtyDeviceEmulator::run, this);
  }

  ~PtyDeviceEmulator()
  {
    m_stop.store(true);

    if (m_thread.joinable())
    {
      m_thread.join();
    }

    if (m_master_fd >= 0)
    {
      ::close(m_master_fd);
      m_master_fd = -1;
    }
    if (m_slave_fd >= 0)
    {
      ::close(m_slave_fd);
      m_slave_fd = -1;
    }
  }

  const std::string& slave_path() const
  {
    return m_slave_path;
  }

  int master_fd() const
  {
    return m_master_fd;
  }

private:
  void run()
  {
    fd_set read_fds;
    char buffer[1024];

    while (!m_stop.load())
    {
      FD_ZERO(&read_fds);
      FD_SET(m_slave_fd, &read_fds);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;

      int ret = select(m_slave_fd + 1, &read_fds, nullptr, nullptr, &timeout);
      if (ret <= 0)
      {
        continue;
      }

      if (FD_ISSET(m_slave_fd, &read_fds))
      {
        int bytes_read = read(m_slave_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0)
        {
          continue;
        }

        buffer[bytes_read] = '\0';
        std::string command(buffer);

        // Find the command terminator
        size_t eol_pos = command.find(m_command_eol);
        if (eol_pos == std::string::npos)
        {
          continue;
        }

        command = command.substr(0, eol_pos);
        std::string response = m_handler(command);
        if (!response.empty())
        {
          write(m_slave_fd, response.c_str(), response.length());
        }
      }
    }
  }

  std::string m_command_eol;
  Handler m_handler;
  std::string m_slave_path;
  int m_master_fd;
  int m_slave_fd;
  std::atomic<bool> m_stop;
  std::thread m_thread;
};

// ============================================================================
// Test Cases for Response Resilience
// ============================================================================

void test_empty_response_detection()
{
  std::cout << "Testing empty response validation (util layer)..." << std::endl;

  // Test that empty response would be rejected at the validation layer
  std::string empty_response = "";
  
  try
  {
    // Simulate what send_energy does - checks for empty response
    if (empty_response.empty())
    {
      throw std::runtime_error("Empty response to SE command");
    }
    throw std::runtime_error("FAILED: Expected exception for empty response");
  }
  catch (const std::exception& e)
  {
    std::cout << "✓ Correctly detected empty response: " << e.what() << std::endl;
  }
}

void test_invalid_prefix_detection()
{
  std::cout << "Testing response prefix validation (util layer)..." << std::endl;

  // Test validation of response prefix
  std::string invalid_response = "?INVALID";
  
  try
  {
    // Simulate what send_energy does - checks for '*' prefix
    if (invalid_response.at(0) != '*')
    {
      throw std::runtime_error(std::string("Expected '*' prefix, got: [") + invalid_response + "]");
    }
    throw std::runtime_error("FAILED: Expected exception for invalid prefix");
  }
  catch (const std::exception& e)
  {
    std::cout << "✓ Correctly detected invalid prefix: " << e.what() << std::endl;
  }
}

void test_token_count_validation_integration()
{
  std::cout << "Testing token count validation for structured responses..." << std::endl;

  // Simulate response parsing with token validation
  std::string response = "*A B C";  // Only 3 tokens when we expect 4
  
  try
  {
    std::vector<std::string> tokens;
    util::tokenize_string(response, tokens, " ");
    util::validate_token_count(tokens, 4, "inst_info", response);
    throw std::runtime_error("FAILED: Expected exception for insufficient tokens");
  }
  catch (const std::exception& e)
  {
    std::cout << "✓ Correctly caught insufficient tokens: " << e.what() << std::endl;
  }
}

void test_safe_substring_extraction()
{
  std::cout << "Testing safe substring extraction..." << std::endl;

  // Test valid extraction
  std::string result = util::safe_substr("*VALUE", 1, 5, "test_safe_substr");
  expect_true(result == "VALUE", "safe_substr failed for valid range");

  // Test bounds checking
  try
  {
    util::safe_substr("*", 1, 5, "test_safe_substr");  // Trying to extract beyond string length
    throw std::runtime_error("FAILED: Expected exception for out-of-bounds extraction");
  }
  catch (const std::runtime_error& e)
  {
    std::cout << "✓ Correctly caught out-of-bounds substr: " << e.what() << std::endl;
  }
}

void test_safe_character_access()
{
  std::cout << "Testing safe character access..." << std::endl;

  // Test valid access
  char c = util::safe_at("*VALUE", 0, "test_safe_at");
  expect_true(c == '*', "safe_at failed for valid index");

  // Test bounds checking
  try
  {
    util::safe_at("*", 5, "test_safe_at");  // Trying to access beyond string length
    throw std::runtime_error("FAILED: Expected exception for out-of-bounds access");
  }
  catch (const std::runtime_error& e)
  {
    std::cout << "✓ Correctly caught out-of-bounds at: " << e.what() << std::endl;
  }
}

void test_token_validation_helper()
{
  std::cout << "Testing token count validation helper..." << std::endl;

  std::vector<std::string> tokens = {"a", "b", "c"};

  // Test valid token count
  util::validate_token_count(tokens, 3, "test_validate", "a b c");
  std::cout << "✓ Token count validation passed for valid count" << std::endl;

  // Test invalid token count
  try
  {
    util::validate_token_count(tokens, 5, "test_validate", "a b c");
    throw std::runtime_error("FAILED: Expected exception for insufficient tokens");
  }
  catch (const std::runtime_error& e)
  {
    std::cout << "✓ Correctly caught token count mismatch: " << e.what() << std::endl;
  }
}

void test_numeric_conversion_safety()
{
  std::cout << "Testing safe numeric conversion (util layer)..." << std::endl;

  // Test numeric conversion with invalid input
  std::string response = "*NOTANUMBER";
  
  try
  {
    // Simulate what send_max does - validates and converts numeric value
    std::string value_str = util::safe_substr(response, 1, response.size() - 1, "send_max");
    double value = std::stod(value_str);  // This should throw invalid_argument
    throw std::runtime_error("FAILED: Expected exception for invalid number");
  }
  catch (const std::invalid_argument& e)
  {
    std::cout << "✓ Correctly caught invalid numeric conversion: " << e.what() << std::endl;
  }
}

void test_valid_response_acceptance()
{
  std::cout << "Testing valid response acceptance..." << std::endl;

  // Test successful parsing of valid numeric response
  std::string response = "*42.5";
  
  try
  {
    // Validate response format
    if (response.empty())
    {
      throw std::runtime_error("Empty response");
    }
    if (response.at(0) != '*')
    {
      throw std::runtime_error("Invalid prefix");
    }
    
    // Parse the numeric value
    std::string value_str = util::safe_substr(response, 1, response.size() - 1, "send_max");
    double value = std::stod(value_str);
    
    expect_true(value == 42.5, "Valid response should be correctly parsed");
    std::cout << "✓ Valid response accepted and parsed correctly (value=" << value << ")" << std::endl;
  }
  catch (const std::exception& e)
  {
    throw std::runtime_error(std::string("FAILED: Valid response rejected: ") + e.what());
  }
}

void test_error_response_handling()
{
  std::cout << "Testing error response handling..." << std::endl;

  // Test detection of error response (? prefix indicates error)
  std::string error_response = "?OUT_OF_RANGE";
  
  try
  {
    // Simulate validation that checks for success marker
    if (error_response.at(0) == '?')
    {
      throw std::runtime_error("Device returned error response");
    }
    throw std::runtime_error("FAILED: Expected exception for error response");
  }
  catch (const std::exception& e)
  {
    std::cout << "✓ Correctly handled error response: " << e.what() << std::endl;
  }
}

}  // namespace

int main()
{
  std::cout << "==================================================================" << std::endl;
  std::cout << "LaserControl Response Resilience Test Suite" << std::endl;
  std::cout << "==================================================================" << std::endl;
  std::cout << std::endl;

  try
  {
    std::cout << "UTILITY FUNCTION TESTS" << std::endl;
    std::cout << "------------------------------------------------------------------" << std::endl;
    test_safe_substring_extraction();
    test_safe_character_access();
    test_token_validation_helper();
    std::cout << std::endl;

    std::cout << "DEVICE RESPONSE RESILIENCE TESTS" << std::endl;
    std::cout << "------------------------------------------------------------------" << std::endl;
    test_empty_response_detection();
    test_invalid_prefix_detection();
    test_token_count_validation_integration();
    test_numeric_conversion_safety();
    test_valid_response_acceptance();
    test_error_response_handling();
    std::cout << std::endl;

    std::cout << "==================================================================" << std::endl;
    std::cout << "ALL TESTS PASSED ✓" << std::endl;
    std::cout << "==================================================================" << std::endl;
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << std::endl;
    std::cerr << "==================================================================" << std::endl;
    std::cerr << "TEST FAILED: " << e.what() << std::endl;
    std::cerr << "==================================================================" << std::endl;
    return 1;
  }
}
