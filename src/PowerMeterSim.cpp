/*
 * PowerMeterSim.cpp
 *
 *  Created on: Apr 3, 2024
 *      Author: Nuno Barros
 */

#include "PowerMeterSim.hh"
#include <utilities.hh>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

namespace device
{

  PowerMeterSim::PowerMeterSim ()
          :
            m_mmode(mmEnergy),
            m_range(2),
            m_wavelength(266),
            m_e_threshold(1),
            m_ave_query_state(aNone),
            m_pulse_length(0)
            {
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

  PowerMeterSim::~PowerMeterSim ()
  {

  }


  void PowerMeterSim::get_range_fast(int16_t &range)
  {
    // m_range is always in sync with the instrument
    range = m_range;
  }

  void PowerMeterSim::set_wavelength(const uint32_t wl)
  {
    bool r;
    wavelength(wl,r);
    if (r)
    {
      m_wavelength = wl;
    }
  }

  void PowerMeterSim::get_wavelength(uint32_t &wl)
  {
    // TODO: Implement this to query the instrument
    wl =  m_wavelength;
  }


  void PowerMeterSim::get_e_threshold(uint32_t &et)
  {
    // TODO: Implement this to query the instrument
    et =  m_e_threshold;
  }

  void PowerMeterSim::get_energy(double &energy)
  {
    send_energy(energy);
  }

  //// -- these are validated

  void PowerMeterSim::get_average_flag(bool &flag)
  {
    flag = true;
  }

  void PowerMeterSim::average_query(const AQSetting q, AQSetting &answer)
  {
    uint16_t qq = q;
    uint16_t aa;
    average_query(qq,aa);
    answer = static_cast<AQSetting>(aa);
  }

  // typical answer
  // [* 1      NONE 0.5sec   1sec   3sec  10sec  30sec \r\n]
  void PowerMeterSim::average_query(const uint16_t q, uint16_t &a)
  {
    a = 1,

    // only non-empty tokens are added
    m_ave_windows.insert({0,"NONE"});
    m_ave_windows.insert({1,"0.5sec"});
    m_ave_windows.insert({2,"1sec"});
    m_ave_windows.insert({3,"3sec"});
    m_ave_windows.insert({4,"10sec"});
    m_ave_windows.insert({5,"30sec"});
  }

  // typical answer
  // [*4 10.0J 2.00J 200mJ 20.0mJ 2.00mJ \r\n]
  void PowerMeterSim::get_all_ranges(int16_t &current_setting)
  {
    std::string cmd = "AR";
    std::string resp = "*4 10.0J 2.00J 200mJ 20.0mJ 2.00mJ \r\n";

    // strip the first two bytes, as it indicates whether the command failed or not and in this case it does not matter
    resp = resp.substr(1);
    // NOTE:There is no space after the first token in this command
    // now we want to tokenize the returned string and set the map
    std::vector<std::string> tokens;
    resp = util::trim(resp); // trim leading and trailing whitespaces
    util::tokenize_string(resp, tokens, " ");

#ifdef DEBUG
    std::cout << "PowerMeterSim::get_all_ranges : Found " << tokens.size() << " entries" << std::endl;
    for (auto t: tokens)
    {
      std::cout << "[" << t << "]" << std::endl;
    }
#endif

    // however the first token is the current setting
    current_setting = std::stol(tokens.at(0)) & 0xFFFF;

    // remove that token
    //tokens.pop_front();
    tokens.erase(tokens.begin());

    // now clear the map
    m_ranges.clear();
    // the first token should be "AUTO"
    int counter = 0;
    for (std::string entry: tokens)
    {
#ifdef DEBUG
      std::cout << "PowerMeterSim::get_all_ranges : Adding entry [" << counter << " : " << entry << "]" << std::endl;
#endif
      m_ranges.insert({counter,entry});
      counter ++;
    }
  }

  // the logic is very simular to the previous method (for ranges)
  // the problem is that the output is very device specific
  // TODO: Get information from LANL
  void PowerMeterSim::get_all_wavelengths(uint16_t &current_setting)
  {
    throw std::runtime_error("Not implemented");
  }

  // from experience, the answer is like
  // [*CONTINUOUS 190 3000 1 266 355 532 1064 2100 2940 \r\n]
  void PowerMeterSim::get_all_wavelengths(std::string &answer)
  {
    std::string cmd = "AW";
    //std::string rr;
#ifdef DEBUG
    std::cout << "PowerMeterSim::get_all_wavelengths : Sending query [" << cmd << "]" << std::endl;
#endif
    answer = "*CONTINUOUS 190 3000 1 266 355 532 1064 2100 2940 \r\n";
#ifdef DEBUG
    std::cout << "PowerMeterSim::get_all_wavelengths : got answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif

    // just remove the first byte and return the rest
    answer = answer.substr(1);
  }


  void PowerMeterSim::bc20_sensor_mode(BC20 query,BC20 &answer)
  {
    std::ostringstream cmd;
    std::string resp = "";
    answer = bHold;
//    cmd << "BQ " << static_cast<int>(query);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::bc20_sensor_mode : Sending query [" << cmd.str() << "]" << std::endl;
//#endif
//    send_cmd(cmd.str(),resp);
//    //  write_cmd(cmd.str());
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::bc20_sensor_mode : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    // strip the first byte, as it indicates whether the command failed or not
//    //char s= resp.at(0);
//    //success = (s == '*')?true:false;
//    // the third byte is the setting that is still in place
//    answer = static_cast<BC20>(std::stol(resp.substr(2,1)));
  }

  void PowerMeterSim::diffuser_query(const DiffuserSetting s, DiffuserSetting &answer)
  {
    std::ostringstream cmd;
    std::string resp;
//    cmd << "DQ " << static_cast<int>(s);
    answer = dNA;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::diffuser_query : Sending query [" << cmd.str() << "]" << std::endl;
//#endif
//    send_cmd(cmd.str(),resp);
//    //  write_cmd(cmd.str());
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::diffuser_query : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//
//
//    // Here we need to be a bit smarter, since the device is N/A the return code is the same
//    // as OUT mode on a valid device...how to not design drivers
//    // so, let's use that as the primary separator
//    if (resp.find("N/A") != resp.npos)
//    {
//      // it does not matter whether it succeeded or failed
//      // the setting has no meaning in this device
//      answer = dNA;
//    }
//    else
//    {
//      // actually we do not care whether it succeeded or failed
//      // all we care is to get the currently registered setting
//      // and since we already ruled out that it was NA, then
//      // we can simply do a cast
//      answer = static_cast<DiffuserSetting>(std::stol(resp.substr(2,1)));
//    }
  }

  void PowerMeterSim::exposure_energy(double &energy, uint32_t &pulses, uint32_t &et)
  {

    energy = 125.42;
    pulses = 5;
    et = 100.0;
//    std::string cmd = "EE";
//    std::string resp;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::exposure_energy : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,resp);
//    //  write_cmd(cmd);
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::exposure_energy : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//
//    if (resp.at(0) == '*')
//    {
//      // tokenize the response string
//      // now we want to tokenize the returned string and set the map
//      std::vector<std::string> tokens;
//      util::tokenize_string(resp, tokens, " ");
//#ifdef DEBUG
//      std::cout << "PowerMeterSim::exposure_energy : Found " << tokens.size() << " entries (expected 4)" << std::endl;
//#endif
//      energy = std::stod(tokens.at(1));
//      pulses = std::stoul(tokens.at(2));
//      et = std::stoul(tokens.at(3));
//    }
//    else
//    {
//      // for one reason or another, there is no measurement
//      // return all zeros
//      energy = 0.0;
//      pulses = 0;
//      et = 0;
//    }
  }

  void PowerMeterSim::energy_flag(bool &new_val)
  {
    new_val = true;
//    std::string cmd = "EF";
//    std::string resp;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::energy_flag : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,resp);
//    //  write_cmd(cmd);
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::energy_flag : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    if (resp.size() < 2) new_val = false;
//    new_val = (std::stol(resp.substr(1)) == 0)?false:true;
  }

  void PowerMeterSim::energy_ready(bool &new_val)
  {
    new_val = true;
//
//    std::string cmd = "ER";
//    std::string resp;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::energy_ready : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,resp);
//    //  write_cmd(cmd);
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::energy_ready : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    new_val = (std::stol(resp.substr(1)) == 0)?false:true;
  }

  //Note: This is *NOT* the method you want to use
  // You want to use UT
  void PowerMeterSim::energy_threshold(const uint32_t et, uint32_t &answer)
  {
//    std::ostringstream cmd;
//    std::string resp;
//    cmd << "ET " << et;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::energy_threshold : Sending query [" << cmd.str() << "]" << std::endl;
//#endif
//    send_cmd(cmd.str(),resp);
//    //  write_cmd(cmd);
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::energy_threshold : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//
//    answer = std::stoul(resp.substr(1,1));
//
    answer = 0.00;
    m_e_threshold = answer;
  }

  void PowerMeterSim::force_energy(bool &success)
  {
    success = true;

//    std::string cmd = "FE";
//    std::string resp ;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::force_energy : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,resp);
//    //  write_cmd(cmd);
//    //  std::string resp = m_serial.readline(0xFFFF, "\r\n");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::force_energy : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    success = (resp.at(0) == '?')?false:true;
  }

  void PowerMeterSim::force_power(bool &success)
  {
success = false;
//
//    std::string cmd = "FP";
//    std::string resp;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::force_power : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::force_power : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    success = (resp.at(0) == '?')?false:true;
  }

  void PowerMeterSim::filter_query(const DiffuserSetting s, DiffuserSetting &answer)
  {
    answer = dNA;
//    std::ostringstream cmd;
//    cmd << "FQ " << static_cast<int>(s);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::filter_query : Sending query [" << cmd.str() << "]" << std::endl;
//#endif
//    std::string resp;
//    send_cmd(cmd.str(),resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::filter_query : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//
//
//    // Here we need to be a bit smarter, since the device is N/A the return code is the same
//    // as OUT mode on a valid device...how to not design drivers
//    // so, let's use that as the primary separator
//    if (resp.find("N/A") != resp.npos)
//    {
//      // it does not matter whether it succeeded or failed
//      // the setting has no meaning in this device
//      answer = dNA;
//    }
//    else
//    {
//      // actually we do not care whether it succeeded or failed
//      // all we care is to get the currently registered setting
//      // and since we already ruled out that it was NA, then
//      // we can simply do a cast
//      answer = static_cast<DiffuserSetting>(std::stol(resp.substr(2,1)));
//    }
  }

  void PowerMeterSim::force_exposure(bool &success)
  {
    success = false;
//    std::string cmd = "FX";
//    std::string resp;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::force_exposure : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::force_exposure : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    success = (resp.at(0) == '?')?false:true;
  }


  void PowerMeterSim::head_info_raw(std::string &answer)
  {
    answer = "";
//    std::string cmd = "HI";
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::head_info_raw : Sending query [" << cmd << "]" << std::endl;
//#endif
//    send_cmd(cmd,answer);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::head_info_raw : got answer [" << util::escape(answer.c_str()) << "]" << std::endl;
//#endif
  }


  void PowerMeterSim::head_info(std::string &type, std::string &sn, std::string &name, bool &power, bool &energy, bool &freq)
  {

    type = "energy";
    sn = "DEADBEEF";
    name="simulation";
    power = false;
    energy = true;
    freq = true;

//    std::string rr;
//    head_info_raw(rr);
//    // get rid of trailing and leading whitespace
//    rr = util::trim(rr);
//    // tokenize it and get rid of first entry
//    std::vector<std::string> tokens;
//    util::tokenize_string(rr, tokens, " ");
//    tokens.erase(tokens.begin());
//    type = tokens.at(0);
//    sn = tokens.at(1);
//    name = tokens.at(2);
//    uint32_t word = std::stoul(tokens.at(3),0, 16);
//
//    // power is bit 0
//    power = word & (1 << 0);
//    // energy is bit 1
//    energy = word & (1 << 0);
//    // frequency is bit 31
//    freq = word & (1 << 31);
//
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::head_info : \n"
//        << "Type   : " << type
//        << "\nSN     : " << sn
//        << "\nname   : " << name
//        << "\npower  : " << power
//        << "\nenergy : " << energy
//        << "\nfreq   : " << freq << std::endl;
//#endif

  }

  void PowerMeterSim::head_type(std::string &type)
  {
    type = "";
//    std::string resp;
//    send_cmd("HT",resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::head_type : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    type = resp.substr(1);
  }

  void PowerMeterSim::inst_config(bool &success)
  {
    success = true;
//
//    std::string resp;
//    send_cmd("IC",resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::inst_config : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    success = (resp.at(0) == '?')?false:true;
  }

  void PowerMeterSim::inst_info(std::string &id, std::string &sn, std::string &name)
  {
    id = "whatever";
    sn = "DEADBEEF";
    name = "simulation";
//
//    std::string rr;
//    send_cmd("II",rr);
//    rr = util::trim(rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::inst_info : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // tokenize it and get rid of first entry
//    std::vector<std::string> tokens;
//    util::tokenize_string(rr, tokens, " ");
//    // drop the first token
//    tokens.erase(tokens.begin());
//    id = tokens.at(0);
//    sn = tokens.at(1);
//    name = tokens.at(2);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::inst_info : \n"
//        << "ID      : " << id
//        << "\nSN     : " << sn
//        << "\nname   : " << name << std::endl;
//#endif

  }

  void PowerMeterSim::mains_frequency(MainsFreq q, MainsFreq &answer)
  {
    answer = mf50Hz;
//    uint16_t a;
//    mains_frequency(static_cast<uint16_t>(q),a);
//    answer = static_cast<MainsFreq>(a);
  }

  void PowerMeterSim::mains_frequency(uint16_t q, uint16_t &answer)
  {
    answer = 1;
//    std::ostringstream msg;
//    std::string resp;
//    msg << "MA " << q;
//    send_cmd(msg.str(),resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::mains_frequency : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//    answer = std::stoul(resp.substr(2,1)) & 0xFFFF;
  }


  void PowerMeterSim::max_freq(uint32_t &value)
  {
    value = 1000;
//
//    std::string rr;
//    send_cmd("MF",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::max_freq : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//
//    value = std::stoul(rr.substr(1));
  }

  void PowerMeterSim::measurement_mode(const MeasurementMode q, MeasurementMode &a)
  {
    a = mmEnergy;
//    uint16_t query = static_cast<uint16_t>(q);
//    uint16_t answer;
//    measurement_mode(query,answer);
//    a = static_cast<MeasurementMode>(answer);
  }

  void PowerMeterSim::measurement_mode(const uint16_t q, uint16_t &a)
  {
    a = 3;

//    std::ostringstream msg;
//    std::string resp;
//
//    msg << "MM " << q;
//    send_cmd(msg.str(),resp);
//    resp = util::trim(resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::measurement_mode : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//
//    // this command is silly. The answer varies wildly depending on the setting
//    if (q == 0)
//    {
//      // we were querying current setting. All we care about is
//      // the second byte
//      a = std::stoi(resp.substr(1));
//    }
//    else {
//      // we are trying to set a new setting
//      // the answer is different depending whether it was successful or not
//      if (resp.at(0) == '*')
//      {
//        // success.
//        a = q;
//      } else
//      {
//        // failed
//        // then query what is the current setting
//        // because the failed command does not return the current setting...
//        // why?!?!?! Don't know ¯\_(ツ)_/¯
//        measurement_mode(0,a);
//      }
//    }
  }

  // typical answer
  // [*1 1.0ms 2.0ms 5.0ms 10ms 20ms \r\n]
  void PowerMeterSim::pulse_length(const uint16_t value, uint16_t &answer)
  {
    std::ostringstream msg;
    std::string resp;

    msg << "PL " << value;
    //send_cmd(msg.str(),resp);
    resp = "*1 1.0ms 2.0ms 5.0ms 10ms 20ms \r\n";
#ifdef DEBUG
    std::cout << "PowerMeterSim::pulse_length : Got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
    if (value == 0)
    {
      // we were querying current setting. All we care about is
      // the second byte
      answer = std::stoul(resp.substr(1,1));

      // if the map is not filled, then fill it
      if (m_pulse_lengths.size() == 0)
      {
        resp= resp.substr(2);
        // next trim leading and trailing spaces
        resp = util::trim(resp);

#ifdef DEBUG
        std::cout << "PowerMeterSim::pulse_length : string to tokenize : [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif

        // tokenize it and get rid of first entry
        std::vector<std::string> tokens;
        util::tokenize_string(resp, tokens, " ");
        // now insert these into the map
        uint16_t k = 1;
        for (std::string entry: tokens)
        {
#ifdef DEBUG
          std::cout << "PowerMeterSim::pulse_length : Adding entry [" << k << " : " << entry << "]" << std::endl;
#endif
          m_pulse_lengths.insert({k,entry});
          k++;
        }
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
        // failed
        answer = std::stoul(resp.substr(1,1)) & 0xFFFF;
      }
    }
  }


  void PowerMeterSim::reset()
  {
//    std::string rr;
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::reset : Sendint a reset. This may lead to complete disconnect of the system" << std::endl;
//#endif
//    send_cmd("RE",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::reset : Got answer [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
  }

  void PowerMeterSim::get_range(int32_t &value)
  {
    value = 1;
//
//    std::string rr;
//    send_cmd("RN",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::get_range : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // drop the first byte
//    value = std::stoi(rr.substr(1));
  }

  /**
   * Typical usage:
   * Example.
    1. User sends EF command.
    2. Read device response. If response is “*0” repeat step 1. If response is “*1” continue with step 3.
    3. User send SE command
    4. Device responds “*1.100E-4” (110uJ)
   */
  void PowerMeterSim::send_energy(double &value)
  {

    value = 1.2e-3;
//    std::string rr;
//    send_cmd("SE",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_energy : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // drop the first byte
//    value = std::stod(rr.substr(1));
  }

  bool PowerMeterSim::read_energy(double &energy)
  {
    energy = 1.2e-3;
    return true;
//    bool status;
//    energy_flag(status);
//    if (status)
//    {
//      send_energy(energy);
//    }
//    return status;
  }

  void PowerMeterSim::send_frequency(double &value)
  {
    value = 50.0;
//    std::string rr;
//    send_cmd("SF",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_frequency : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // drop the first byte
//    value = std::stod(rr.substr(1));
  }

  void PowerMeterSim::send_average(double &value)
  {
    value = 1.2e-3;
    //    std::string rr;
//    send_cmd("SG",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_average : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // drop the first byte
//    value = std::stod(rr.substr(1));
  }


  bool  PowerMeterSim::read_average(double &value)
  {
    bool st;
    get_average_flag(st);
    if (st)
    {
      send_average(value);
    }

    return st;
  }
  void PowerMeterSim::send_units(char &unit)
  {
    unit = 'J';
//    std::string rr;
//    send_cmd("SI",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_units : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // drop the first byte
//    unit = rr.at(1);
  }
  void PowerMeterSim::send_units_long(std::string &unit)
  {
    char u = 'J';
    send_units(u);
    unit = m_measurement_units.at(u);
  }

  void PowerMeterSim::send_power(double &value)
  {
    value =0.0;
//    std::string rr;
//    send_cmd("SP",rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_power : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // drop the first byte
//    value = std::stod(rr.substr(1));
  }

  void PowerMeterSim::send_max(double &value)
  {
    value = 100.0;
//    std::string rr;
//    send_cmd("SX",rr);
//    // drop the first byte
//    rr = rr.substr(1);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_max : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // -- check if we are in auto mode
//    if (rr == "AUTO")
//    {
//      value = 0.0;
//    }
//    else
//    {
//      // convert the value
//      value = std::stod(rr);
//    }
  }

  void PowerMeterSim::trigger_window(const uint16_t value, uint16_t &answer)
  {
    answer = 1;
//
//    std::ostringstream cmd;
//    cmd << "TW " << value;
//    std::string rr;
//    send_cmd(cmd.str(),rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::trigger_window : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // first strip the return byte
//    rr = rr.substr(1);
//    answer = std::stoul(rr) & 0xFFFF;
  }

  void PowerMeterSim::user_threshold(const uint16_t value, uint16_t &answer)
  {
//    answer =
//    std::ostringstream cmd;
//    cmd << "UT " << value;
//    std::string rr;
//    send_cmd(cmd.str(),rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::user_threshold : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // -- undocumented difference
//    // out of range is answered with ?OUT OF RANGE
//    if (rr.at(0) == '?')
//    {
//      // command somehow failed.
//      // either throw an exception or just return the answer of what setting it is right now
//      answer = m_e_threshold;
//#ifdef DEBUG
//      std::cout << "PowerMeterSim::user_threshold : Failed to set the threshold. Answer : [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//      return;
//    }
//    // -- if it didn't fail, just continue
//    // first strip the return byte
//    rr = rr.substr(1);
//    // now tokenize the answer
//    std::vector<std::string> tokens;
//    util::tokenize_string(rr, tokens, " ");
//
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::user_threshold : Tokens : " << std::endl;
//    for (auto t: tokens)
//    {
//      std::cout << t << std::endl;
//    }
//#endif
    answer = 1;
    m_e_threshold = answer;

//    if (m_threshold_ranges.second == 0xFFFF)
//    {
//      // it has not been set yet
//      uint16_t min = std::stoul(tokens.at(1)) & 0xFFFF;
//      m_threshold_ranges.first = min;
//      uint16_t  max = std::stoul(tokens.at(2)) & 0xFFFF;
//      m_threshold_ranges.second = max;
//    }
  }

  void PowerMeterSim::query_user_threshold(uint16_t &current, uint16_t &min, uint16_t &max)
  {
//    std::string cmd = "UT";
//    std::string rr;
//    send_cmd(cmd,rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::query_user_threshold : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // first strip the return byte
//    rr = rr.substr(1);
//    // now tokenize the answer
//    std::vector<std::string> tokens;
//    util::tokenize_string(rr, tokens, " ");
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::query_user_threshold : Tokens : " << std::endl;
//    for (auto t: tokens)
//    {
//      std::cout << t << std::endl;
//    }
//#endif
//
//    current = std::stoul(tokens.at(0)) & 0xFFFF;
//    m_e_threshold = current;
//    min = std::stoul(tokens.at(1)) & 0xFFFF;
//    m_threshold_ranges.first = min;
//    max = std::stoul(tokens.at(2)) & 0xFFFF;
//    m_threshold_ranges.second = max;
    m_e_threshold = 100;
    m_threshold_ranges.first = 1;
    m_threshold_ranges.second = 1000;
  }

  void PowerMeterSim::version(std::string &value)
  {
    value = "1.25";
//
//    std::string cmd = "VE";
//    std::string rr;
//    send_cmd(cmd,rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::version : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // first strip the return byte
//    value = rr.substr(1);
  }

  void PowerMeterSim::wavelength_index(const uint16_t index, bool &success)
  {
    success = false;
//    std::ostringstream cmd;
//    cmd << "WI " << index;
//    std::string rr;
//    send_cmd(cmd.str(),rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::wavelength_index : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // first strip the return byte
//    success=(rr.at(0)=='*')?true:false;

  }

  void PowerMeterSim::wavelength(const uint16_t wl, bool &success)
  {
    m_wavelength = wl;
    success = true;
//    std::ostringstream cmd;
//    cmd << "WL " << wl;
//    std::string rr;
//    send_cmd(cmd.str(),rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::wavelength : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // first strip the return byte
//    success=(rr.at(0)=='*')?true:false;
  }

  void PowerMeterSim::write_range(const int16_t range, bool &success)
  {
    m_range = range;
    success = true;
//    std::ostringstream cmd;
//    cmd << "WN " << range;
//    std::string rr;
//    send_cmd(cmd.str(),rr);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::write_range : Got response [" << util::escape(rr.c_str()) << "]" << std::endl;
//#endif
//    // first strip the return byte
//    success=(rr.at(0)=='*')?true:false;
  }


  //void PowerMeterSim::set_measurement_mode(const uint16_t mode)
  //{
  //  switch(mode)
  //  {
  //    case 0: // passive
  //  }
  //}

  ///
  ///
  /// PRIVATE METHODS
  ///
  ///

//  void PowerMeterSim::send_cmd(const std::string cmd, std::string &resp)
//  {
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_cmd : Sending query [" << cmd << "]" << std::endl;
//#endif
//    write_cmd(cmd);
//
//    std::this_thread::sleep_for(std::chrono::milliseconds(50));
//    read_cmd(resp);
//#ifdef DEBUG
//    std::cout << "PowerMeterSim::send_cmd : got answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//  }

//  void PowerMeterSim::init_pulse_lengths()
//  {
//    m_pulse_lengths.clear();
//    std::string resp;
//    // do first a query
//    std::string cmd = "MM 0";
//    send_cmd(cmd,resp);
//    // now parse response
//    std::vector<std::string> tokens;
//    util::tokenize_string(resp, tokens, " ");
//    // drop the first token (success)
//    tokens.erase(tokens.begin());
//    // place all others in the map
//    size_t c = 1;
//    for (std::string t : tokens)
//    {
//      m_pulse_lengths.insert({c,t});
//      c++;
//    }
//  }


} /* namespace device */
