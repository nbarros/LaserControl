/*
 * PowerMeter.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <PowerMeter.hh>
#include <utilities.hh>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

//#define DEBUG 1

namespace device {

  PowerMeter::PowerMeter (const char* port, const uint32_t baud_rate)
    : Device(port,baud_rate),
      m_mmode(mmEnergy),
      m_range(2),
      m_wavelength(266),
      m_e_threshold(1),
      m_ave_query_state(aNone),
      m_pulse_length(0),
      m_interval_between_cmds_ms(1)

      {
    // override the prefix
    m_com_pre = "$";
    m_request_suffix = "\r\n";
    m_response_suffix= "\r\n";
    // initialize to something unrealistic
    m_threshold_ranges = {0xFFFF,0xFFFF};
    m_timeout_ms = 100;

    // initialize the serial connection
    m_serial.set_port(m_comport);
    m_serial.set_baudrate(m_baud);
    m_serial.set_bytesize(serial::eightbits);
    m_serial.set_parity(serial::parity_none);
    serial::Timeout t = serial::Timeout::simpleTimeout(m_timeout_ms);
    m_serial.set_timeout(t);
    m_serial.set_stopbits(serial::stopbits_one);

    m_serial.open();
    if (!m_serial.is_open())
    {
      std::ostringstream msg;
      msg << "Failed to open the port ["<< m_comport << ":" << m_baud << "]";
#ifdef DEBUG
      std::cerr << "PowerMeter::PowerMeter : "<< msg.str() << std::endl;
#endif
      throw serial::PortNotOpenedException(msg.str().c_str());
    } else
    {
#ifdef DEBUG
      std::cout << "PowerMeter::PowerMeter : Connection established" << std::endl;
#endif
      /// we have a connection. Lets initialize some settings

    }

    const bool powermeter_online = probe_connection("II", 2, 50);
    if (!powermeter_online)
    {
      throw serial::IOException(__FILE__, __LINE__, "PowerMeter is connected but not responding");
    }

    // init measurement units map
    m_measurement_units.insert({'c',"Foot-Candles"});
    m_measurement_units.insert({'d',"dBm"});
    m_measurement_units.insert({'i',"Irradiance (Joules/cm^2)"});
    m_measurement_units.insert({'J',"Joules"});
    m_measurement_units.insert({'I',"Lux"});
    m_measurement_units.insert({'u',"Lumens"});
    m_measurement_units.insert({'w',"Dosage (Watts/cm^2)"});
    m_measurement_units.insert({'W',"Watts"});
    m_measurement_units.insert({'X',"No measurement"});

      }

  PowerMeter::~PowerMeter ()
  {

  }

  void PowerMeter::set_range(const int16_t range, bool &success)
  {
    std::ostringstream cmd;
    cmd << "WN " << range;
    std::string rr;
    bool st = send_cmd(cmd.str(), rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to set range");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::set_range : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // first strip the return byte
    if (rr.size() > 0)
    {
      success=(rr.at(0)=='*')?true:false;
    }
    else
    {
      success = false;
    }
  }

  void PowerMeter::get_range_fast(int16_t &range)
  {
    // m_range is always in sync with the instrument
    range = m_range;
  }

  void PowerMeter::set_wavelength(const uint32_t wl)
  {
    bool r;
    wavelength(wl,r);
    if (r)
    {
      m_wavelength = wl;
    }
  }

  void PowerMeter::get_wavelength(uint32_t &wl)
  {
    // TODO: Implement this to query the instrument
    wl =  m_wavelength;
  }



  //
  //void PowerMeter::set_e_threshold(const uint32_t et)
  //{
  //  std::ostringstream cmd;
  //  cmd << "ET " << et;
  //  write_cmd(cmd.str());
  //  m_e_threshold = et;
  //
  //}

  void PowerMeter::get_e_threshold(uint32_t &et)
  {
    // TODO: Implement this to query the instrument
    et =  m_e_threshold;
  }

  void PowerMeter::get_energy(double &energy)
  {
    send_energy(energy);
  }

  //// -- these are validated

  void PowerMeter::get_average_flag(bool &flag)
  {
    std::string cmd = "AF";
    std::string resp;
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query average flag");
    }

    //  write_cmd(cmd);
    //  std::string answer = m_serial.readline(0xFFFF, "\r\n");
    // the answer should be "*0\n\r"
#ifdef DEBUG
    std::cout << "PowerMeter::get_average_flag : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response format: expect "*X" where X is 0 or 1
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::get_average_flag", 
        std::string("Empty response to AF command"));
    }

    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::get_average_flag", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    // Extract the value (should be just after the '*')
    try
    {
      // Safe to access substr(1) because we know resp has at least 1 char (the '*')
      const std::string value_str = util::safe_substr(resp, 1, 1, "PowerMeter::get_average_flag");
      flag = (std::stol(value_str) != 0) ? true : false;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::get_average_flag", ex);
    }
  }

  void PowerMeter::average_query(const AQSetting q, AQSetting &answer)
  {
    uint16_t qq = q;
    uint16_t aa;
    average_query(qq,aa);
    answer = static_cast<AQSetting>(aa);
  }

  // typical answer
  // [* 1      NONE 0.5sec   1sec   3sec  10sec  30sec \r\n]
  void PowerMeter::average_query(const uint16_t q, uint16_t &a)
  {
    std::ostringstream cmd;
    cmd << "AQ " << q;
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::set_average_query : Sending query [" << cmd.str() << "]" << std::endl;
#endif
    bool st = send_cmd(cmd.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query average");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::get_average_query : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::average_query", 
        std::string("Empty response to AQ command"));
    }

    // Validate response format
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::average_query", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    // The third byte is the setting that is still in place
    uint16_t parsed_answer = 0;
    try
    {
      parsed_answer = std::stoul(util::safe_substr(resp, 2, 1, "PowerMeter::average_query")) & 0xFFFF;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::average_query", ex);
    }
    a = parsed_answer;

    // if the map has no entries, fill it
    if (m_ave_windows.size() == 0)
    {
#ifdef DEBUG
      std::cout << "PowerMeter::set_average_query : got answer [" << resp << "]" << std::endl;
#endif
      // The first option is going to be NONE. Search for it.
      size_t p = resp.find("NONE");
      if (p == std::string::npos)
      {
        util::throw_parse_error("PowerMeter::average_query", "NONE token not found");
      }
      std::string range = util::safe_substr(resp, p, resp.size() - p, "PowerMeter::average_query");
      std::vector<std::string> tokens;
      util::tokenize_string(range, tokens, " ");

      std::map<uint16_t, std::string> parsed_windows;
      uint16_t k = 1;
      for (std::string t : tokens)
      {
        t = util::trim(t); // trim leading and trailing chars
        // only non-empty tokens are added
        if (t.size())
        {
          parsed_windows.insert({k,t});
          k++;
        }
      }
      m_ave_windows = parsed_windows;
    }
  }

  // typical answer
  // [*4 10.0J 2.00J 200mJ 20.0mJ 2.00mJ \r\n]
  void PowerMeter::get_all_ranges(int16_t &current_setting)
  {
    std::string cmd = "AR";
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::get_all_ranges : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query all ranges");
    }
    //  write_cmd(cmd.str());
    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
    std::cout << "PowerMeter::get_all_ranges : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::get_all_ranges", 
        std::string("Empty response to AR command"));
    }

    // strip the first byte (success indicator)
    resp = util::safe_substr(resp, 1, resp.size() - 1, "PowerMeter::get_all_ranges");
    
    // NOTE: There is no space after the first token in this command
    // now we want to tokenize the returned string and set the map
    std::vector<std::string> tokens;
    resp = util::trim(resp); // trim leading and trailing whitespaces
    util::tokenize_string(resp, tokens, " ");

#ifdef DEBUG
    std::cout << "PowerMeter::get_all_ranges : Found " << tokens.size() << " entries" << std::endl;
    for (auto t: tokens)
    {
      std::cout << "[" << t << "]" << std::endl;
    }
#endif

    int16_t parsed_setting = 0;
    try
    {
      // Validate we have at least one token
      util::validate_token_count(tokens, 1, "PowerMeter::get_all_ranges", resp);
      
      // The first token is the current setting
      parsed_setting = std::stol(tokens.at(0)) & 0xFFFF;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::get_all_ranges", ex);
    }

    // remove that token
    //tokens.pop_front();
    tokens.erase(tokens.begin());

    // now clear the map
    std::map<int16_t, std::string> parsed_ranges;
    // the first token should be "AUTO"
    int counter = 0;
    for (std::string entry: tokens)
    {
#ifdef DEBUG
      std::cout << "PowerMeter::get_all_ranges : Adding entry [" << counter << " : " << entry << "]" << std::endl;
#endif
      parsed_ranges.insert({counter,entry});
      counter ++;
    }
    current_setting = parsed_setting;
    m_ranges = parsed_ranges;
  }

  // the logic is very simular to the previous method (for ranges)
  // the problem is that the output is very device specific
  // TODO: Get information from LANL
  void PowerMeter::get_all_wavelengths(uint16_t &current_setting)
  {
    throw std::runtime_error("Not implemented");
    /**
  std::ostringstream cmd;
  cmd << "AW";
#ifdef DEBUG
  std::cout << "PowerMeter::get_all_wavelengths : Sending query [" << cmd.str() << "]" << std::endl;
#endif
  write_cmd(cmd.str());

  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::get_all_wavelengths : got answer [" << resp << "]" << std::endl;
#endif
  // strip the first byte, as it indicates whether the command failed or not and in this case it does not matter
  resp = resp.substr(1);

  // now we want to tokenize the returned string and set the map
  std::vector<std::string> tokens;
  util::tokenize_string(resp, tokens, " ");

#ifdef DEBUG
  std::cout << "PowerMeter::get_all_ranges : Found " << tokens.size() << " entries" << std::endl;
#endif

  //FIXME: Finish implementing
  current_setting = -1;
     */
  }

  // from experience, the answer is like
  // [*CONTINUOUS 190 3000 1 266 355 532 1064 2100 2940 \r\n]
  void PowerMeter::get_all_wavelengths(std::string &answer)
  {
    std::string cmd = "AW";
    //std::string rr;
#ifdef DEBUG
    std::cout << "PowerMeter::get_all_wavelengths : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, answer);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query all wavelengths");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::get_all_wavelengths : got answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif

    // Validate response is not empty and remove the success marker
    if (answer.empty())
    {
      util::throw_parse_error("PowerMeter::get_all_wavelengths", 
        std::string("Empty response to AW command"));
    }

    // Remove the first byte (success marker) and return the rest
    answer = util::safe_substr(answer, 1, answer.size() - 1, "PowerMeter::get_all_wavelengths");
  }


  void PowerMeter::bc20_sensor_mode(BC20 query,BC20 &answer)
  {
    std::ostringstream cmd;
    std::string resp;
    cmd << "BQ " << static_cast<int>(query);
#ifdef DEBUG
    std::cout << "PowerMeter::bc20_sensor_mode : Sending query [" << cmd.str() << "]" << std::endl;
#endif
    bool st = send_cmd(cmd.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query b20 sensor mode");
    }
    //  write_cmd(cmd.str());
    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
    std::cout << "PowerMeter::bc20_sensor_mode : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response format: expect "*Xs" or similar with at least 3 chars
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::bc20_sensor_mode", 
        std::string("Empty response to BQ command"));
    }
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::bc20_sensor_mode", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }
    // The third byte is the setting that is still in place
    try
    {
      answer = static_cast<BC20>(std::stol(util::safe_substr(resp, 2, 1, "PowerMeter::bc20_sensor_mode")));
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::bc20_sensor_mode", ex);
    }
  }

  void PowerMeter::diffuser_query(const DiffuserSetting s, DiffuserSetting &answer)
  {
    std::ostringstream cmd;
    std::string resp;
    cmd << "DQ " << static_cast<int>(s);
#ifdef DEBUG
    std::cout << "PowerMeter::diffuser_query : Sending query [" << cmd.str() << "]" << std::endl;
#endif
    bool st = send_cmd(cmd.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query diffuser");
    }
    //  write_cmd(cmd.str());
    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
    std::cout << "PowerMeter::diffuser_query : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

    // Validate response is not empty first
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::diffuser_query", 
        std::string("Empty response to DQ command"));
    }

    // Check if device reports N/A (not applicable)
    // Note: the return code is the same as OUT mode on a valid device
    if (resp.find("N/A") != resp.npos)
    {
      // the setting has no meaning in this device
      answer = dNA;
    }
    else
    {
      // Get the currently registered setting
      // Validate response format before parsing
      if (resp.at(0) != '*')
      {
        util::throw_parse_error("PowerMeter::diffuser_query", 
          std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
      }
      try
      {
        answer = static_cast<DiffuserSetting>(std::stol(util::safe_substr(resp, 2, 1, "PowerMeter::diffuser_query")));
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::diffuser_query", ex);
      }
    }
  }

  void PowerMeter::exposure_energy(double &energy, uint32_t &pulses, uint32_t &et)
  {
    std::string cmd = "EE";
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::exposure_energy : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query exposure energy");
    }
    //  write_cmd(cmd);
    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
    std::cout << "PowerMeter::exposure_energy : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

    // Validate response is not empty and starts with success marker
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::exposure_energy", 
        std::string("Empty response to EE command"));
    }

    if (resp.at(0) == '*')
    {
      // tokenize the response string
      std::vector<std::string> tokens;
      util::tokenize_string(resp, tokens, " ");
#ifdef DEBUG
      std::cout << "PowerMeter::exposure_energy : Found " << tokens.size() << " entries (expected 4)" << std::endl;
#endif
      try
      {
        // Validate we have enough tokens before accessing
        util::validate_token_count(tokens, 4, "PowerMeter::exposure_energy", resp);
        
        const double parsed_energy = std::stod(tokens.at(1));
        const uint32_t parsed_pulses = std::stoul(tokens.at(2));
        const uint32_t parsed_et = std::stoul(tokens.at(3));
        energy = parsed_energy;
        pulses = parsed_pulses;
        et = parsed_et;
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::exposure_energy", ex);
      }
    }
    else
    {
      // for one reason or another, there is no measurement
      // return all zeros
      energy = 0.0;
      pulses = 0;
      et = 0;
    }
  }

  void PowerMeter::energy_flag(bool &new_val)
  {
    std::string cmd = "EF";
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::energy_flag : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query energy flag");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::energy_flag : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::energy_flag", 
        std::string("Empty response to EF command"));
    }

    // Validate response starts with success marker
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::energy_flag", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    try
    {
      new_val = (std::stol(util::safe_substr(resp, 1, resp.size() - 1, "PowerMeter::energy_flag")) == 0)?false:true;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::energy_flag", ex);
    }
#ifdef DEBUG
    std::cout << "PowerMeter::energy_flag : returning [" << new_val << "]" << std::endl;
#endif
  }

  void PowerMeter::energy_ready(bool &new_val)
  {
    std::string cmd = "ER";
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::energy_ready : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query energy ready");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::energy_ready : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::energy_ready", 
        std::string("Empty response to ER command"));
    }

    // Validate response starts with success marker
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::energy_ready", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    try
    {
      new_val = (std::stol(util::safe_substr(resp, 1, resp.size() - 1, "PowerMeter::energy_ready")) == 0)?false:true;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::energy_ready", ex);
    }
  }

  //Note: This is *NOT* the method you want to use
  // You want to use UT
  void PowerMeter::energy_threshold(const uint32_t et, uint32_t &answer)
  {
    std::ostringstream cmd;
    std::string resp;
    cmd << "ET " << et;
#ifdef DEBUG
    std::cout << "PowerMeter::energy_threshold : Sending query [" << cmd.str() << "]" << std::endl;
#endif
    bool st = send_cmd(cmd.str(),resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query energy threshold");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::energy_threshold : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::energy_threshold", 
        std::string("Empty response to ET command"));
    }

    // Validate response starts with success marker
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::energy_threshold", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    try
    {
      const uint32_t parsed_answer = std::stoul(util::safe_substr(resp, 1, 1, "PowerMeter::energy_threshold"));
      answer = parsed_answer;
      m_e_threshold = parsed_answer;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::energy_threshold", ex);
    }
  }

  void PowerMeter::force_energy(bool &success)
  {
    std::string cmd = "FE";
    std::string resp ;
#ifdef DEBUG
    std::cout << "PowerMeter::force_energy : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to force energy");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::force_energy : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::force_energy", 
        std::string("Empty response to FE command"));
    }
    // '?' indicates failure, '*' indicates success
    success = (resp.at(0) != '?');
  }

  void PowerMeter::force_power(bool &success)
  {
    std::string cmd = "FP";
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::force_power : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to force power");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::force_power : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::force_power", 
        std::string("Empty response to FP command"));
    }
    // '?' indicates failure, '*' indicates success
    success = (resp.at(0) != '?');
  }

  void PowerMeter::filter_query(const DiffuserSetting s, DiffuserSetting &answer)
  {
    std::ostringstream cmd;
    cmd << "FQ " << static_cast<int>(s);
#ifdef DEBUG
    std::cout << "PowerMeter::filter_query : Sending query [" << cmd.str() << "]" << std::endl;
#endif
    std::string resp;
    bool st = send_cmd(cmd.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query filter info");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::filter_query : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

    // Validate response is not empty first
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::filter_query", 
        std::string("Empty response to FQ command"));
    }

    // Check if device reports N/A (not applicable)
    if (resp.find("N/A") != resp.npos)
    {
      // the setting has no meaning in this device
      answer = dNA;
    }
    else
    {
      // Get the currently registered setting
      // Validate response format before parsing
      if (resp.at(0) != '*')
      {
        util::throw_parse_error("PowerMeter::filter_query", 
          std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
      }
      try
      {
        answer = static_cast<DiffuserSetting>(std::stol(util::safe_substr(resp, 2, 1, "PowerMeter::filter_query")));
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::filter_query", ex);
      }
    }
  }

  void PowerMeter::force_exposure(bool &success)
  {
    std::string cmd = "FX";
    std::string resp;
#ifdef DEBUG
    std::cout << "PowerMeter::force_exposure : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to force exposure");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::force_exposure : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::force_exposure", 
        std::string("Empty response to FX command"));
    }
    // '?' indicates failure, '*' indicates success
    success = (resp.at(0) != '?');
  }


  void PowerMeter::head_info_raw(std::string &answer)
  {
    std::string cmd = "HI";
#ifdef DEBUG
    std::cout << "PowerMeter::head_info_raw : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = send_cmd(cmd, answer);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query head info");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::head_info_raw : got answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif
  }


  void PowerMeter::head_info(std::string &type, std::string &sn, std::string &name, bool &power, bool &energy, bool &freq)
  {
    std::string rr;
    head_info_raw(rr);
    // get rid of trailing and leading whitespace
    rr = util::trim(rr);
    // tokenize it and get rid of first entry
    std::vector<std::string> tokens;
    util::tokenize_string(rr, tokens, " ");
    try
    {
      tokens.erase(tokens.begin());
      const std::string parsed_type = tokens.at(0);
      const std::string parsed_sn = tokens.at(1);
      const std::string parsed_name = tokens.at(2);
      const uint32_t word = std::stoul(tokens.at(3),0, 16);

      const bool parsed_power = word & (1 << 0);
      const bool parsed_energy = word & (1 << 0);
      const bool parsed_freq = word & (1 << 31);

      type = parsed_type;
      sn = parsed_sn;
      name = parsed_name;
      power = parsed_power;
      energy = parsed_energy;
      freq = parsed_freq;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::head_info", ex);
    }

#ifdef DEBUG
    std::cout << "PowerMeter::head_info : \n"
        << "Type   : " << type
        << "\nSN     : " << sn
        << "\nname   : " << name
        << "\npower  : " << power
        << "\nenergy : " << energy
        << "\nfreq   : " << freq << std::endl;
#endif

  }

  void PowerMeter::head_type(std::string &type)
  {
    std::string resp;
    std::string cmd = "HT";
    bool st = send_cmd(cmd, resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query head type");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::head_type : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::head_type", 
        std::string("Empty response to HT command"));
    }

    // Validate response starts with success marker
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::head_type", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    type = util::safe_substr(resp, 1, resp.size() - 1, "PowerMeter::head_type");
  }

  void PowerMeter::inst_config(bool &success)
  {
    std::string rr;
    std::string cmd = "IC";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query instrument config");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::inst_config : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::inst_config", 
        std::string("Empty response to IC command"));
    }

    // '?' indicates failure, '*' indicates success
    success = (rr.at(0) != '?');
  }

  void PowerMeter::inst_info(std::string &id, std::string &sn, std::string &name)
  {
    std::string rr;
    std::string cmd = "II";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query instrument info");
    }

    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::inst_info", 
        std::string("Empty response to II command"));
    }

    rr = util::trim(rr);
#ifdef DEBUG
    std::cout << "PowerMeter::inst_info : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // tokenize it and get rid of first entry (success marker)
    std::vector<std::string> tokens;
    util::tokenize_string(rr, tokens, " ");
    
    try
    {
      // Validate token count (should be at least 4: marker + id + sn + name)
      util::validate_token_count(tokens, 4, "PowerMeter::inst_info", rr);
      
      tokens.erase(tokens.begin());
      const std::string parsed_id = tokens.at(0);
      const std::string parsed_sn = tokens.at(1);
      const std::string parsed_name = tokens.at(2);
      id = parsed_id;
      sn = parsed_sn;
      name = parsed_name;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::inst_info", ex);
    }
#ifdef DEBUG
    std::cout << "PowerMeter::inst_info : \n"
        << "ID      : " << id
        << "\nSN     : " << sn
        << "\nname   : " << name << std::endl;
#endif

  }

  void PowerMeter::mains_frequency(MainsFreq q, MainsFreq &answer)
  {
    uint16_t a;
    mains_frequency(static_cast<uint16_t>(q),a);
    answer = static_cast<MainsFreq>(a);
  }

  void PowerMeter::mains_frequency(uint16_t q, uint16_t &answer)
  {
    std::ostringstream msg;
    std::string resp;
    msg << "MA " << q;
    bool st = send_cmd(msg.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to send mains frequency");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::mains_frequency : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::mains_frequency", 
        std::string("Empty response to MA command"));
    }

    // Validate response format
    if (resp.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::mains_frequency", 
        std::string("Expected '*' prefix, got: [") + util::escape(resp) + "]");
    }

    try
    {
      answer = std::stoul(util::safe_substr(resp, 2, 1, "PowerMeter::mains_frequency")) & 0xFFFF;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::mains_frequency", ex);
    }
  }


  void PowerMeter::max_freq(uint32_t &value)
  {
    std::string rr;
    std::string cmd = "MF";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to send max frequency");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::max_freq : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif

    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::max_freq", 
        std::string("Empty response to MF command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::max_freq", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    try
    {
      value = std::stoul(util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::max_freq"));
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::max_freq", ex);
    }
  }

  void PowerMeter::measurement_mode(const MeasurementMode q, MeasurementMode &a)
  {

    uint16_t query = static_cast<uint16_t>(q);
    uint16_t answer;
    measurement_mode(query,answer);
    a = static_cast<MeasurementMode>(answer);
  }

  void PowerMeter::measurement_mode(const uint16_t q, uint16_t &a)
  {
    std::ostringstream msg;
    std::string resp;

    msg << "MM " << q;
    bool st = send_cmd(msg.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to set measurement mode");
    }
    resp = util::trim(resp);
#ifdef DEBUG
    std::cout << "PowerMeter::measurement_mode : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

    // Validate response is not empty
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::measurement_mode", 
        std::string("Empty response to MM command"));
    }

    // This command's answer varies depending on the setting
    if (q == 0)
    {
      // We are querying the current setting
      // Response format: "*<digit>" where digit is the mode
      try
      {
        a = std::stoi(util::safe_substr(resp, 1, resp.size() - 1, "PowerMeter::measurement_mode"));
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::measurement_mode", ex);
      }
    }
    else {
      // We are trying to set a new setting
      // The answer format differs based on whether it was successful
      if (resp.at(0) == '*')
      {
        // Success - command was applied
        a = q;
      } else
      {
        // Failed - command was not applied
        // Query the current setting instead
        // Note: be careful with recursion to avoid infinite loops
        try
        {
          measurement_mode(0, a);
        }
        catch (const std::exception&)
        {
          // If query also fails, re-throw original error
          util::throw_parse_error("PowerMeter::measurement_mode", 
            std::string("Failed to set or query measurement mode. Response: [") + 
            util::escape(resp) + "]");
        }
      }
    }
  }

  // typical answer
  // [*1 1.0ms 2.0ms 5.0ms 10ms 20ms \r\n]
  void PowerMeter::pulse_length(const uint16_t value, uint16_t &answer)
  {
    std::ostringstream msg;
    std::string resp;

    msg << "PL " << value;
    bool st = send_cmd(msg.str(), resp);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to set pulse length");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::pulse_length : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

    // Validate response
    if (resp.empty())
    {
      util::throw_parse_error("PowerMeter::pulse_length", 
        std::string("Empty response to PL command"));
    }

    if (value == 0)
    {
      // we were querying current setting. Validate success marker and extract current setting
      if (resp.at(0) != '*')
      {
        util::throw_parse_error("PowerMeter::pulse_length", 
          std::string("Expected '*' prefix for query, got: [") + util::escape(resp) + "]");
      }

      try
      {
        answer = std::stoul(util::safe_substr(resp, 1, 1, "PowerMeter::pulse_length"));
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::pulse_length", ex);
      }

      // if the map is not filled, then fill it
      if (m_pulse_lengths.size() == 0)
      {
        resp = util::safe_substr(resp, 2, resp.size() - 2, "PowerMeter::pulse_length");
        // next trim leading and trailing spaces
        resp = util::trim(resp);

#ifdef DEBUG
        std::cout << "PowerMeter::pulse_length : string to tokenize : [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

        // tokenize it and get rid of first entry
        std::vector<std::string> tokens;
        util::tokenize_string(resp, tokens, " ");
        // now insert these into the map
        std::map<uint16_t, std::string> parsed_lengths;
        uint16_t k = 1;
        for (std::string entry: tokens)
        {
#ifdef DEBUG
          std::cout << "PowerMeter::pulse_length : Adding entry [" << k << " : " << entry << "]" << std::endl;
#endif
          parsed_lengths.insert({k,entry});
          k++;
        }
        m_pulse_lengths = parsed_lengths;
      }
    }
    else {
      // we are trying to set a new setting
      // the answer is different depending whether it was successful or not
      if (resp.at(0) == '*')
      {
        // success.
        answer = value;
      } else
      {
        // failed - get current setting
        try
        {
          answer = std::stoul(util::safe_substr(resp, 1, 1, "PowerMeter::pulse_length")) & 0xFFFF;
        }
        catch (const std::exception& ex)
        {
          util::throw_parse_error("PowerMeter::pulse_length", ex);
        }
      }
    }
  }


  void PowerMeter::reset()
  {
    std::string rr;
#ifdef DEBUG
    std::cout << "PowerMeter::reset : Sendint a reset. This may lead to complete disconnect of the system" << std::endl;
#endif
    std::string cmd = "RE";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to send reset");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::reset : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
  }

  void PowerMeter::get_range(int32_t &value)
  {
    std::string rr;
    std::string cmd = "RN";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query range");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::get_range : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::get_range", 
        std::string("Empty response to RN command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::get_range", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // drop the success marker and parse the value
    try
    {
      value = std::stoi(util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::get_range"));
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::get_range", ex);
    }
  }

  /**
   * Typical usage:
   * Example.
  1. User sends EF command.
  2. Read device response. If response is “*0” repeat step 1. If response is “*1” continue with step 3.
  3. User send SE command
  4. Device responds “*1.100E-4” (110uJ)
   */
  void PowerMeter::send_energy(double &value)
  {
    std::string rr;
    std::string cmd = "SE";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query energy");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::send_energy : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response is not empty
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::send_energy", 
        std::string("Empty response to SE command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::send_energy", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // Extract numeric value after the success marker
    try
    {
      value = std::stod(util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::send_energy"));
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::send_energy", ex);
    }
  }

  bool PowerMeter::read_energy(double &energy)
  {
    bool status;
    energy_flag(status);
    if (status)
    {
      // printf("Energy is ready, sending SE command\n");
      send_energy(energy);
    }
    return status;
  }

  void PowerMeter::send_frequency(double &value)
  {
    std::string rr;
    std::string cmd = "SF";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query frequency");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::send_frequency : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Typical answer: [*10] or failure: [?FREQ TOO LOW]
    // Validate response is not empty
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::send_frequency", 
        std::string("Empty response to SF command"));
    }

    if (rr.at(0) == '?')
    {
      // Command failed - return 0.0
      value = 0.0;
    }
    else if (rr.at(0) == '*')
    {
      // Success - extract the numeric value
      try
      {
        value = std::stod(util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::send_frequency"));
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::send_frequency", ex);
      }
    }
    else
    {
      util::throw_parse_error("PowerMeter::send_frequency", 
        std::string("Unexpected response prefix. Expected '*' or '?', got: [") + 
        util::escape(rr) + "]");
    }
  }

  void PowerMeter::send_average(double &value)
  {
    std::string rr;
    std::string cmd = "SG";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query average");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::send_average : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
// Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::send_average", 
        std::string("Empty response to SG command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::send_average", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // Extract numeric value after the success marker
    try
    {
      value = std::stod(util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::send_average"));
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::send_average", ex);
    }
  }


  bool  PowerMeter::read_average(double &value)
  {
    bool st;
    get_average_flag(st);
    if (st)
    {
      send_average(value);
    }

    return st;
  }
  void PowerMeter::send_units(char &unit)
  {
    std::string rr;
    std::string cmd = "SI";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query units");
    }

#ifdef DEBUG
    std::cout << "PowerMeter::send_units : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response format: expect "*X" where X is the unit character
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::send_units", 
        std::string("Empty response to SI command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::send_units", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // Extract the unit character
    try
    {
      unit = util::safe_at(rr, 1, "PowerMeter::send_units");
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::send_units", ex);
    }
  }
  void PowerMeter::send_units_long(std::string &unit)
  {
    char u;
    send_units(u);
    unit = m_measurement_units.at(u);
  }

  void PowerMeter::send_power(double &value)
  {
    std::string rr;
    std::string cmd = "SP";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query power ");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::send_power : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::send_power", 
        std::string("Empty response to SP command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::send_power", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // Extract numeric value after the success marker
    try
    {
      value = std::stod(util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::send_power"));
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::send_power", ex);
    }
  }

  void PowerMeter::send_max(double &value)
  {
    std::string rr;
    std::string cmd = "SX";
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query max");
    }

    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::send_max", 
        std::string("Empty response to SX command"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::send_max", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // drop the first byte (success marker)
    rr = util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::send_max");
#ifdef DEBUG
    std::cout << "PowerMeter::send_max : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Check if we are in auto mode
    if (rr == "AUTO")
    {
      value = 0.0;
    }
    else
    {
      // convert the value
      try
      {
        value = std::stod(rr);
      }
      catch (const std::exception& ex)
      {
        util::throw_parse_error("PowerMeter::send_max", ex);
      }
    }
  }

  void PowerMeter::trigger_window(const uint16_t value, uint16_t &answer)
  {
    // -- this command is not implemented in the Vega device
    answer = 0;
    return;

//     std::ostringstream cmd;
//     cmd << "TW " << value;
//     std::string rr;
//     bool st = send_cmd(cmd.str(), rr);
//     if (!st)
//     {
//       throw serial::IOException(__FILE__, __LINE__, "Failed to query trigger window");
//     }
// #ifdef DEBUG
//     std::cout << "PowerMeter::trigger_window : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
// #endif
//     // first strip the return byte
//     rr = rr.substr(1);
//     answer = std::stoul(rr) & 0xFFFF;
  }

  void PowerMeter::user_threshold(const uint16_t value, uint16_t &answer)
  {
    std::ostringstream cmd;
    cmd << "UT " << value;
    std::string rr;
    bool st = send_cmd(cmd.str(), rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query user threshold");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::user_threshold : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif

    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::user_threshold", 
        std::string("Empty response to UT command"));
    }

    // -- undocumented difference
    // out of range is answered with ?OUT OF RANGE
    if (rr.at(0) == '?')
    {
      // command somehow failed.
      // either throw an exception or just return the answer of what setting it is right now
      answer = m_e_threshold;
#ifdef DEBUG
      std::cout << "PowerMeter::user_threshold : Failed to set the threshold. Answer : [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
      return;
    }
    else
    {
      // confirm that we do have a success
      if (rr.at(0) == '*')
      {
        // -- if it didn't fail, just continue
        // first strip the success marker
        rr = util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::user_threshold");
        // now tokenize the answer
        std::vector<std::string> tokens;
        util::tokenize_string(rr, tokens, " ");
    #ifdef DEBUG
        std::cout << "PowerMeter::user_threshold : Tokens : " << tokens.size() << ":" << std::endl;
        for (auto t: tokens)
        {
          std::cout << t << std::endl;
        }
    #endif
        if (tokens.size() == 3)
        {
          // this should be the usually expected anwer
          uint16_t parsed_answer = 0;
          uint16_t parsed_min = m_threshold_ranges.first;
          uint16_t parsed_max = m_threshold_ranges.second;
          try
          {
            parsed_answer = std::stoul(tokens.at(0)) & 0xFFFF;
            if (m_threshold_ranges.second == 0xFFFF)
            {
              parsed_min = std::stoul(tokens.at(1)) & 0xFFFF;
              parsed_max = std::stoul(tokens.at(2)) & 0xFFFF;
            }
          }
          catch (const std::exception& ex)
          {
            util::throw_parse_error("PowerMeter::user_threshold", ex);
          }

          answer = parsed_answer;
          m_e_threshold = parsed_answer;

          if (m_threshold_ranges.second == 0xFFFF)
          {
            // it has not been set yet
            m_threshold_ranges.first = parsed_min;
            m_threshold_ranges.second = parsed_max;
          }
        }
        else
        {
          // setting was successful but for whatever reason didn't receive the expected answer.
          uint16_t min,max;
          query_user_threshold(answer,min,max);
        }
      }
      else
      {
        // this should never happen.
#ifdef DEBUG
        std::cout << "Got an unexpected answer...what is going on?" << std::endl;
#endif
      }
    }
  }

  void PowerMeter::query_user_threshold(uint16_t &current, uint16_t &min, uint16_t &max)
  {
    std::string cmd = "UT";
    std::string rr;
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query user threshold");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::query_user_threshold : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif

    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::query_user_threshold", 
        std::string("Empty response to UT query"));
    }

    // Validate response starts with success marker
    if (rr.at(0) != '*')
    {
      util::throw_parse_error("PowerMeter::query_user_threshold", 
        std::string("Expected '*' prefix, got: [") + util::escape(rr) + "]");
    }

    // first strip the return byte
    rr = util::safe_substr(rr, 1, rr.size() - 1, "PowerMeter::query_user_threshold");
    // now tokenize the answer
    std::vector<std::string> tokens;
    util::tokenize_string(rr, tokens, " ");
#ifdef DEBUG
    std::cout << "PowerMeter::query_user_threshold : Tokens : " << std::endl;
    for (auto t: tokens)
    {
      std::cout << t << std::endl;
    }
#endif

    try
    {
      // Validate token count (expect: current, min, max)
      util::validate_token_count(tokens, 3, "PowerMeter::query_user_threshold", rr);
      
      const uint16_t parsed_current = std::stoul(tokens.at(0)) & 0xFFFF;
      const uint16_t parsed_min = std::stoul(tokens.at(1)) & 0xFFFF;
      const uint16_t parsed_max = std::stoul(tokens.at(2)) & 0xFFFF;
      current = parsed_current;
      min = parsed_min;
      max = parsed_max;
      m_e_threshold = parsed_current;
      m_threshold_ranges.first = parsed_min;
      m_threshold_ranges.second = parsed_max;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::query_user_threshold", ex);
    }
  }

  void PowerMeter::version(std::string &value)
  {
    std::string cmd = "VE";
    std::string rr;
    bool st = send_cmd(cmd, rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to query version");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::version : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // first strip the return byte
    value = rr.substr(1);
  }

  void PowerMeter::wavelength_index(const uint16_t index, bool &success)
  {
    std::ostringstream cmd;
    cmd << "WI " << index;
    std::string rr;
    bool st = send_cmd(cmd.str(), rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to set wavelength index");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::wavelength_index : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::wavelength_index", 
        std::string("Empty response to WI command"));
    }

    // Check if command succeeded (starts with '*') or failed (anything else)
    success = (rr.at(0) == '*');
  }

  void PowerMeter::wavelength(const uint16_t wl, bool &success)
  {
    std::ostringstream cmd;
    cmd << "WL " << wl;
    std::string rr;
    bool st = send_cmd(cmd.str(), rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to set wavelength");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::wavelength : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::wavelength", 
        std::string("Empty response to WL command"));
    }

    // Check if command succeeded (starts with '*') or failed (anything else)
    success = (rr.at(0) == '*');
  }

  void PowerMeter::write_range(const int16_t range, bool &success)
  {
    std::ostringstream cmd;
    cmd << "WN " << range;
    std::string rr;
    bool st = send_cmd(cmd.str(), rr);
    if (!st)
    {
      throw serial::IOException(__FILE__, __LINE__, "Failed to write range");
    }
#ifdef DEBUG
    std::cout << "PowerMeter::write_range : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
#endif
    // Validate response
    if (rr.empty())
    {
      util::throw_parse_error("PowerMeter::write_range", 
        std::string("Empty response to WN command"));
    }

    // Check if command succeeded (starts with '*') or failed (anything else)
    success = (rr.at(0) == '*');
  }


  ///
  ///
  /// PRIVATE METHODS
  ///
  ///

  bool PowerMeter::send_cmd(const std::string cmd, std::string &resp, bool repeat)
  {
    const std::lock_guard<std::mutex> lock(m_cmd_mutex);
#ifdef DEBUG
    std::cout << "PowerMeter::send_cmd : Sending query [" << cmd << "]" << std::endl;
#endif
    bool st = exchange_cmd(cmd, resp);
    if (!st)
    {
      if (repeat)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_interval_between_cmds_ms));
        reset_connection();
        std::this_thread::sleep_for(std::chrono::milliseconds(m_interval_between_cmds_ms));
        return exchange_cmd(cmd, resp);
      }
      else
      {
        return false;
      }
    }
#ifdef DEBUG
    std::cout << "PowerMeter::send_cmd : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    return true;
  }

  void PowerMeter::init_pulse_lengths()
  {
    std::string resp;
    // do first a query
    std::string cmd = "MM 0";
    bool st = send_cmd(cmd,resp);
    if (!st)
    {
      throw serial::IOException(__FILE__,__LINE__,"Failed to query pulse lengths");
    }
    // now parse response
    std::vector<std::string> tokens;
    util::tokenize_string(resp, tokens, " ");
    // drop the first token (success)
    try
    {
      tokens.erase(tokens.begin());
      std::map<uint16_t, std::string> parsed_lengths;
      size_t c = 1;
      for (std::string t : tokens)
      {
        parsed_lengths.insert({c,t});
        c++;
      }
      m_pulse_lengths = parsed_lengths;
    }
    catch (const std::exception& ex)
    {
      util::throw_parse_error("PowerMeter::init_pulse_lengths", ex);
    }
  }

}

