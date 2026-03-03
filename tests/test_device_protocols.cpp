#include <Attenuator.hh>
#include <Laser.hh>
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

    if (m_thread.joinable())
    {
      m_thread.join();
    }
  }

  const std::string& slave_path() const
  {
    return m_slave_path;
  }

  bool saw_command(const std::string& cmd) const
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& entry : m_commands)
    {
      if (entry == cmd)
      {
        return true;
      }
    }
    return false;
  }

private:
  void run()
  {
    std::string buffer;

    while (!m_stop.load())
    {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(m_master_fd, &readfds);
      timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 100000;

      const int ready = select(m_master_fd + 1, &readfds, nullptr, nullptr, &tv);
      if (ready <= 0)
      {
        continue;
      }

      char chunk[128];
      const ssize_t nread = ::read(m_master_fd, chunk, sizeof(chunk));
      if (nread <= 0)
      {
        break;
      }
      buffer.append(chunk, static_cast<size_t>(nread));

      for (;;)
      {
        const size_t pos = buffer.find(m_command_eol);
        if (pos == std::string::npos)
        {
          break;
        }

        const std::string raw_cmd = buffer.substr(0, pos);
        buffer.erase(0, pos + m_command_eol.size());

        {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_commands.push_back(raw_cmd);
        }

        const std::string response = m_handler(raw_cmd);
        if (!response.empty())
        {
          (void)::write(m_master_fd, response.data(), response.size());
        }
      }
    }
  }

  std::string m_command_eol;
  Handler m_handler;
  int m_master_fd;
  int m_slave_fd;
  std::string m_slave_path;
  std::thread m_thread;
  std::atomic<bool> m_stop;

  mutable std::mutex m_mutex;
  std::vector<std::string> m_commands;
};

void test_laser_protocols()
{
  PtyDeviceEmulator emulator(
    "\r",
    [](const std::string& cmd) -> std::string
    {
      if (cmd == "SE")
      {
        return "SE\r00\r";
      }
      if (cmd == "SC")
      {
        return "SC\r000001234\r";
      }
      return cmd + "\r";
    });

  device::Laser laser(emulator.slave_path().c_str(), 9600);

  std::string sec_code;
  std::string sec_msg;
  laser.security(sec_code, sec_msg);
  expect_true(sec_code == "00", "Laser::security should parse security code");
  expect_true(sec_msg == "Normal", "Laser::security should map code to message");

  uint32_t shot_count = 0;
  laser.get_shot_count(shot_count);
  expect_true(shot_count == 1234, "Laser::get_shot_count should parse numeric shot count");

  laser.set_prescale(123);
  expect_true(emulator.saw_command("PD 099"), "Laser::set_prescale should clamp and format command");

  laser.set_qswitch(1200);
  expect_true(emulator.saw_command("QS 999"), "Laser::set_qswitch should clamp and zero-pad command");

  laser.set_repetition_rate(20.0f);
  expect_true(emulator.saw_command("RR 20.0"), "Laser::set_repetition_rate should format one decimal place");
}

void test_powermeter_protocols()
{
  PtyDeviceEmulator emulator(
    "\r\n",
    [](const std::string& cmd) -> std::string
    {
      if (cmd == "$II")
      {
        return "* VEGA 12345 Meter\r\n";
      }
      if (cmd == "$HI")
      {
        return "* PY 967321 PE50BF-DFH-C 80000001 \r\n";
      }
      if (cmd == "$UT")
      {
        return "*5 1 2500\r\n";
      }
      if (cmd == "$UT 7")
      {
        return "*7 1 2500\r\n";
      }
      if (cmd == "$AR")
      {
        return "*4 10.0J 2.00J 200mJ 20.0mJ 2.00mJ \r\n";
      }
      if (cmd == "$AQ 0")
      {
        return "* 1      NONE 0.5sec   1sec   3sec  10sec  30sec \r\n";
      }
      if (cmd == "$SP")
      {
        return "*1.23\r\n";
      }
      if (cmd == "$WL 355")
      {
        return "*\r\n";
      }
      if (cmd == "$WN -1")
      {
        return "*\r\n";
      }
      return "*\r\n";
    });

  device::PowerMeter pm(emulator.slave_path().c_str(), 9600);

  std::string id;
  std::string sn;
  std::string name;
  pm.inst_info(id, sn, name);
  expect_true(id == "VEGA", "PowerMeter::inst_info should parse instrument id");
  expect_true(sn == "12345", "PowerMeter::inst_info should parse serial number");
  expect_true(name == "Meter", "PowerMeter::inst_info should parse instrument name");

  std::string type;
  bool power = false;
  bool energy = false;
  bool freq = false;
  pm.head_info(type, sn, name, power, energy, freq);
  expect_true(type == "PY", "PowerMeter::head_info should parse type");
  expect_true(power, "PowerMeter::head_info should parse power capability bit");
  expect_true(energy, "PowerMeter::head_info should parse energy capability bit");
  expect_true(freq, "PowerMeter::head_info should parse frequency capability bit");

  uint16_t current = 0;
  uint16_t min_threshold = 0;
  uint16_t max_threshold = 0;
  pm.query_user_threshold(current, min_threshold, max_threshold);
  expect_true(current == 5, "PowerMeter::query_user_threshold current mismatch");
  expect_true(min_threshold == 1, "PowerMeter::query_user_threshold min mismatch");
  expect_true(max_threshold == 2500, "PowerMeter::query_user_threshold max mismatch");

  uint16_t answer = 0;
  pm.user_threshold(7, answer);
  expect_true(answer == 7, "PowerMeter::user_threshold should return applied threshold");
  const std::pair<uint16_t, uint16_t> ranges = pm.get_threshold_ranges();
  expect_true(ranges.first == 1 && ranges.second == 2500, "PowerMeter threshold ranges should be cached");

  int16_t current_range = -999;
  pm.get_all_ranges(current_range);
  expect_true(current_range == 4, "PowerMeter::get_all_ranges should parse current setting");
  std::map<int16_t, std::string> range_map;
  pm.get_range_map(range_map);
  expect_true(range_map.size() == 5, "PowerMeter::get_all_ranges should populate range map");

  uint16_t aq_answer = 0;
  pm.average_query(0, aq_answer);
  expect_true(aq_answer == 1, "PowerMeter::average_query should parse answer");
  std::map<uint16_t, std::string> avg_map;
  pm.get_averages_map(avg_map);
  expect_true(!avg_map.empty(), "PowerMeter::average_query should populate average window map");

  double power_value = 0.0;
  pm.send_power(power_value);
  expect_true(power_value > 1.22 && power_value < 1.24, "PowerMeter::send_power should parse value");

  bool success = false;
  pm.wavelength(355, success);
  expect_true(success, "PowerMeter::wavelength should parse success marker");
}

void test_attenuator_protocols()
{
  PtyDeviceEmulator emulator(
    "\r",
    [](const std::string& cmd) -> std::string
    {
      if (cmd == "pc")
      {
        return "pc1;0;0;0;59000;114;36;114;2;1;0;0;0;0;0;0;1;1;0;0;0;0;0;0;\n\r";
      }
      if (cmd == "o")
      {
        return "o0;4000;\n\r";
      }
      if (cmd == "p")
      {
        return "ppc_dump;\n\r";
      }
      if (cmd == "n")
      {
        return "nSN0001\n\r";
      }
      return cmd + "\n\r";
    });

  device::Attenuator att(emulator.slave_path().c_str(), 38400);

  uint32_t max_speed = 0;
  att.get_max_speed(max_speed);
  expect_true(max_speed == 59000, "Attenuator constructor refresh should parse max speed");

  uint16_t accel = 0;
  att.get_acceleration(accel);
  expect_true(accel == 0, "Attenuator constructor refresh should parse acceleration");

  int32_t position = 0;
  uint16_t status = 99;
  att.get_position(position, status, false);
  expect_true(position == 4000, "Attenuator::get_position should parse position");
  expect_true(status == 0, "Attenuator::get_position should parse state");

  std::string raw_status = att.get_status_raw();
  expect_true(raw_status == "pc_dump;", "Attenuator::get_status_raw should strip command echo");

  std::string serial_number;
  att.get_serial_number(serial_number);
  expect_true(serial_number == "SN0001", "Attenuator::get_serial_number should parse value");

  att.set_idle_current(300);
  expect_true(emulator.saw_command("ws 44"), "Attenuator::set_idle_current should mask to 8 bits");

  att.set_resolution(device::Attenuator::Sixteeth);
  expect_true(emulator.saw_command("r 6"), "Attenuator::set_resolution should map 16 to command value 6");

  bool got_range_error = false;
  bool ignored_success = false;
  try
  {
    att.set_transmission(1.5, ignored_success, false);
  }
  catch (const std::range_error&)
  {
    got_range_error = true;
  }
  expect_true(got_range_error, "Attenuator::set_transmission should throw for invalid range");
}

}

int main()
{
  try
  {
    test_laser_protocols();
    test_powermeter_protocols();
    test_attenuator_protocols();

    std::cout << "All LaserControl device protocol tests passed" << std::endl;
    return 0;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Test failure: " << ex.what() << std::endl;
    return 1;
  }
}
