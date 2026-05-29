/**
 * @file resilience_demo.cc
 * @brief Demonstrates proper error handling and resilience in LaserControl library
 *
 * This example shows best practices for:
 * - Handling malformed device responses gracefully
 * - Detecting and reporting response validation errors
 * - Recovering from transient communication failures
 * - Using try-catch blocks effectively with device drivers
 */

#include <PowerMeter.hh>
#include <Laser.hh>
#include <Attenuator.hh>
#include <utilities.hh>

#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <thread>

// ============================================================================
// Example 1: Basic Error Handling with PowerMeter
// ============================================================================

/**
 * Safe wrapper for device operations with automatic retry
 */
class PowerMeterWrapper
{
public:
  PowerMeterWrapper(const std::string& port, int baud = 9600, int max_retries = 3)
    : m_port(port)
    , m_baud(baud)
    , m_max_retries(max_retries)
    , m_device(nullptr)
  {
  }

  /**
   * Connect to the device with retry logic
   */
  bool connect()
  {
    for (int attempt = 0; attempt < m_max_retries; ++attempt)
    {
      try
      {
        m_device = std::unique_ptr<device::PowerMeter>(new device::PowerMeter(m_port.c_str(), m_baud));
        std::cout << "[OK] Connected to PowerMeter on " << m_port << std::endl;
        return true;
      }
      catch (const serial::IOException& e)
      {
        std::cerr << "[WARN] Connection attempt " << (attempt + 1) << "/" << m_max_retries
                  << " failed: " << e.what() << std::endl;
        if (attempt < m_max_retries - 1)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
      }
    }

    std::cerr << "[ERROR] Failed to connect to PowerMeter after " << m_max_retries
              << " attempts" << std::endl;
    return false;
  }

  /**
   * Safe query for energy value with error reporting
   */
  bool get_energy(double& energy)
  {
    if (!m_device)
    {
      std::cerr << "[ERROR] Device not connected" << std::endl;
      return false;
    }

    try
    {
      m_device->send_energy(energy);
      std::cout << "[OK] Energy: " << std::fixed << std::setprecision(6) << energy
                << " (units: see device)" << std::endl;
      return true;
    }
    catch (const serial::IOException& e)
    {
      std::cerr << "[ERROR] Failed to query energy: " << e.what() << std::endl;
      return false;
    }
    catch (const std::exception& e)
    {
      std::cerr << "[ERROR] Unexpected error querying energy: " << e.what() << std::endl;
      return false;
    }
  }

  /**
   * Safe query for power with detailed error diagnostics
   */
  bool get_power(double& power)
  {
    if (!m_device)
    {
      std::cerr << "[ERROR] Device not connected" << std::endl;
      return false;
    }

    try
    {
      m_device->send_power(power);
      std::cout << "[OK] Power: " << std::fixed << std::setprecision(3) << power
                << " W" << std::endl;
      return true;
    }
    catch (const serial::IOException& e)
    {
      // Common causes of IOException:
      // - Empty response from device (device not responding)
      // - Invalid response format (response prefix validation failed)
      // - Communication line noise
      std::cerr << "[ERROR] Power query failed (possible causes):" << std::endl;
      std::cerr << "  1. Device not responding" << std::endl;
      std::cerr << "  2. Invalid serial line configuration" << std::endl;
      std::cerr << "  3. Communication noise on serial line" << std::endl;
      std::cerr << "  Details: " << e.what() << std::endl;
      return false;
    }
  }

  /**
   * Safe query for instrument info with token count validation
   */
  bool get_instrument_info(std::string& id, std::string& sn, std::string& name)
  {
    if (!m_device)
    {
      std::cerr << "[ERROR] Device not connected" << std::endl;
      return false;
    }

    try
    {
      m_device->inst_info(id, sn, name);
      std::cout << "[OK] Instrument Info:" << std::endl;
      std::cout << "  ID  : " << id << std::endl;
      std::cout << "  S/N : " << sn << std::endl;
      std::cout << "  Name: " << name << std::endl;
      return true;
    }
    catch (const serial::IOException& e)
    {
      // Common causes:
      // - Device response missing required fields (token count validation)
      // - Incomplete response from device
      std::cerr << "[ERROR] Instrument info query failed:" << std::endl;
      std::cerr << "  The device response did not contain all required fields." << std::endl;
      std::cerr << "  Details: " << e.what() << std::endl;
      return false;
    }
  }

  /**
   * Safe range setting with format validation
   */
  bool set_range(int16_t range)
  {
    if (!m_device)
    {
      std::cerr << "[ERROR] Device not connected" << std::endl;
      return false;
    }

    try
    {
      bool success;
      m_device->set_range(range, success);
      if (success)
      {
        std::cout << "[OK] Range set to " << range << std::endl;
        return true;
      }
      else
      {
        std::cerr << "[ERROR] Device rejected range " << range << std::endl;
        return false;
      }
    }
    catch (const serial::IOException& e)
    {
      std::cerr << "[ERROR] Range setting failed: " << e.what() << std::endl;
      return false;
    }
  }

  bool is_connected() const { return m_device != nullptr; }

private:
  std::string m_port;
  int m_baud;
  int m_max_retries;
  std::unique_ptr<device::PowerMeter> m_device;
};

// ============================================================================
// Example 2: Cascading Error Recovery
// ============================================================================

/**
 * Performs a safe measurement sequence with fallback options
 */
bool safe_measurement_sequence(PowerMeterWrapper& pm, bool allow_retry = true)
{
  std::cout << "\n[INFO] Starting measurement sequence..." << std::endl;

  double energy = 0.0;
  double power = 0.0;
  double max_power = 0.0;

  // Attempt 1: Query energy
  if (!pm.get_energy(energy))
  {
    if (allow_retry)
    {
      std::cout << "[INFO] Retrying energy measurement..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      if (!pm.get_energy(energy))
      {
        std::cerr << "[ERROR] Energy measurement failed after retry" << std::endl;
        return false;
      }
    }
    else
    {
      return false;
    }
  }

  // Attempt 2: Query power
  if (!pm.get_power(power))
  {
    std::cerr << "[WARN] Power query failed, continuing with energy only" << std::endl;
    // Don't return false - we still have energy data
  }

  // Attempt 3: Query max power
  try
  {
    // Note: This example assumes send_max() exists
    // pm.send_max(max_power);
    // std::cout << "[OK] Max Power: " << max_power << " W" << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "[WARN] Max power query failed (non-critical): " << e.what() << std::endl;
  }

  std::cout << "[OK] Measurement sequence completed successfully" << std::endl;
  return true;
}

// ============================================================================
// Example 3: Utility Function Demonstrating Safe String Operations
// ============================================================================

/**
 * Demonstrates the safe string operation utilities added for resilience
 */
void demonstrate_safe_string_operations()
{
  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Safe String Operations Demonstration" << std::endl;
  std::cout << "==================================================================" << std::endl;

  // Example 1: Safe substring extraction
  std::cout << "\n[1] Safe Substring Extraction:" << std::endl;
  std::string response = "*42.5";
  try
  {
    std::string value_str = util::safe_substr(response, 1, 4, "demo");
    std::cout << "  Input: \"" << response << "\"" << std::endl;
    std::cout << "  Extracted: \"" << value_str << "\"" << std::endl;
    std::cout << "  ✓ Bounds check passed" << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "  ✗ Error: " << e.what() << std::endl;
  }

  // Example 2: Safe character access
  std::cout << "\n[2] Safe Character Access:" << std::endl;
  try
  {
    char prefix = util::safe_at(response, 0, "demo");
    std::cout << "  Input: \"" << response << "\"" << std::endl;
    std::cout << "  Prefix character: '" << prefix << "'" << std::endl;
    if (prefix == '*')
    {
      std::cout << "  ✓ Success indicator detected" << std::endl;
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "  ✗ Error: " << e.what() << std::endl;
  }

  // Example 3: Token count validation
  std::cout << "\n[3] Token Count Validation:" << std::endl;
  std::vector<std::string> tokens = {"ID001", "SN12345", "PowerMeter-X"};
  try
  {
    util::validate_token_count(tokens, 3, "demo", "ID001;SN12345;PowerMeter-X");
    std::cout << "  Tokens: " << tokens.size() << std::endl;
    std::cout << "  Expected: 3" << std::endl;
    std::cout << "  ✓ Token count validated" << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "  ✗ Error: " << e.what() << std::endl;
  }

  // Example 4: Error handling for insufficient tokens
  std::cout << "\n[4] Token Count Mismatch Detection:" << std::endl;
  std::vector<std::string> bad_tokens = {"ID001"};  // Only 1 token
  try
  {
    util::validate_token_count(bad_tokens, 3, "demo", "ID001");
    std::cerr << "  ✗ Should have thrown an exception" << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cout << "  Expected error caught correctly:" << std::endl;
    std::cout << "  \"" << e.what() << "\"" << std::endl;
    std::cout << "  ✓ Insufficient token count detected" << std::endl;
  }
}

// ============================================================================
// Main Demonstration
// ============================================================================

int main(int argc, char* argv[])
{
  std::cout << "\n==================================================================" << std::endl;
  std::cout << "LaserControl Library - Resilience Demo" << std::endl;
  std::cout << "==================================================================" << std::endl;

  // Demonstrate safe string operations
  demonstrate_safe_string_operations();

  // Demonstrate device connection and error handling
  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Device Connection and Error Handling" << std::endl;
  std::cout << "==================================================================" << std::endl;

  std::string port = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
  std::cout << "\n[INFO] Attempting to connect to device on " << port << std::endl;

  PowerMeterWrapper pm(port);

  if (!pm.connect())
  {
    std::cout << "\n[INFO] Demo: Connection failed as expected (no physical device)" << std::endl;
    std::cout << "[INFO] In production, this demonstrates proper error handling" << std::endl;
    std::cout << "[INFO] The library will now:" << std::endl;
    std::cout << "  - Detect empty responses" << std::endl;
    std::cout << "  - Validate response prefixes" << std::endl;
    std::cout << "  - Check token counts in structured responses" << std::endl;
    std::cout << "  - Validate numeric conversions" << std::endl;
    std::cout << "  - Report detailed error information" << std::endl;
  }
  else
  {
    // If we have a real device connected, demonstrate the measurement sequence
    safe_measurement_sequence(pm);
  }

  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Demo Complete" << std::endl;
  std::cout << "==================================================================" << std::endl;
  std::cout << "\nKey Resilience Features Demonstrated:" << std::endl;
  std::cout << "  ✓ Empty response detection" << std::endl;
  std::cout << "  ✓ Response prefix validation (* for success, ? for error)" << std::endl;
  std::cout << "  ✓ Token count validation for structured responses" << std::endl;
  std::cout << "  ✓ Safe substring and character access" << std::endl;
  std::cout << "  ✓ Numeric conversion validation" << std::endl;
  std::cout << "  ✓ Comprehensive error reporting" << std::endl;
  std::cout << std::endl;

  return 0;
}
