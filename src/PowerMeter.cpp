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

namespace device {

PowerMeter::PowerMeter (const char* port, const uint32_t baud_rate)
: Device(port,baud_rate),
  m_mmode(mmEnergy),
  m_range(2),
  m_wavelength(266),
  m_e_threshold(1),
  m_ave_query_state(aNone),
  m_pulse_length()

{
  // override the prefix
  m_com_pre = "$";
  m_com_post = "\n\r";

//  // initialize the ranges map
//  m_ranges.insert({0,std::string("10J")});
//  m_ranges.insert({1,std::string("2J")});
//  m_ranges.insert({2,std::string("200mJ")});
//  m_ranges.insert({3,std::string("2mJ")});

  // initialize the serial connection
  m_serial.setPort(m_comport);
  m_serial.setBaudrate(m_baud);
  m_serial.setBytesize(serial::eightbits);
  m_serial.setParity(serial::parity_none);
  serial::Timeout t = serial::Timeout::simpleTimeout(m_timeout_ms);
  m_serial.setTimeout(t);
  m_serial.setStopbits(serial::stopbits_one);

  m_serial.open();
  if (!m_serial.isOpen())
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

    // set the range (and fill in the map)
    //set_range(m_range);
    // query a couple of settings that need to be listed
    //get_all_ranges();


    // This is a function called "Average Query" in the guide to programmers
    // The aim is to turn off averaging on bootup... but that doesn't seem to work?
    //NFB: This is not what the programmers guide says.

    average_query(aNone,m_ave_query_state);

    // Set the wavelength to 266nm
    set_wavelength(m_wavelength);

    // set the measurement to energy
    bool s;
    force_energy(s);
    if (!s)
    {
      std::runtime_error("Failed to set device to energy mode. This should not happen");
    }
    // We're *trying* to set the Energy Threshold to the minimum value.
    // This is a function called Energy Treshold in the guide to programmers.
    // This doesn't work! Fix it!
    //set_e_threshold(m_e_threshold);

    user_threshold(m_e_threshold,m_e_threshold);
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


  get_all_ranges(m_range);


}

PowerMeter::~PowerMeter ()
{

}


//void PowerMeter::set_range(const int16_t range, std::string &answer)
//void PowerMeter::set_range(const int16_t range, std::string &answer)
//{
//  std::map<int16_t,std::string>::iterator it = m_power_ranges.begin();
//  if ((it = m_power_ranges.find(range)) != m_power_ranges.end())
//  {
//    // it is a valid range
//#ifdef DEBUG
//    std::cout << "PowerMeter::set_range : Setting range to " << it->second << std::endl;
//#endif
//    std::ostringstream cmd;
//    cmd << "WN " << it->first;
//    write_cmd(cmd.str());
//    // then it is supposed to issue yet another command, but I am not sure what it does
//    cmd.str("");
//    cmd.flush();
//    cmd << "AR";
//    write_cmd(cmd.str());
//    // this command is meant to have an answer, and therefore one should read it
//    // TODO: What do we do with the answer? What's its structure?
//    answer = m_serial.readline(0xFFF, "\r");
//    m_range = range;
//
//  } else
//  {
//#ifdef DEBUG
//  std::cerr << "PowerMeter::set_range : Range "<< range << " out of valid values [0,3]" << std::endl;
//#endif
//    throw serial::SerialException("bad range value");
//  }
//}

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
  write_cmd(cmd);
  std::string answer = m_serial.readline(0xFFFF, "\r\n");
  // the answer should be "*0\n\r"
#ifdef DEBUG
  std::cout << "PowerMeter::get_average_flag : got answer [" << answer << "]" << std::endl;
#endif
  // strip the leading * and the trailing \n\r
  answer = answer.substr(1,answer.size() - 3);
  flag = (std::stol(answer)==0)?false:true;
}

void PowerMeter::average_query(const AQSetting q, AQSetting &answer)
{
  uint16_t qq = q;
  uint16_t aa;
  average_query(qq,aa);
  answer = static_cast<AQSetting>(aa);
}

void PowerMeter::average_query(const uint16_t q, uint16_t &a)
{
  std::ostringstream cmd;
  cmd << "AQ " << q;
#ifdef DEBUG
  std::cout << "PowerMeter::set_average_query : Sending query [" << cmd.str() << "]" << std::endl;
#endif
  write_cmd(cmd.str());

  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::get_average_query : got answer [" << resp << "]" << std::endl;
#endif
  // strip the first byte, as it indicates whether the command failed or not
//  char s= resp.at(0);
//  success = (s == '*')?true:false;
  // the third byte is the setting that is still in place
  a = std::stoul(resp.substr(2,1)) & 0xFFFF;

  // if the map has no entries, fill it
  if (m_ave_windows.size() == 0)
  {
#ifdef DEBUG
  std::cout << "PowerMeter::set_average_query : got answer [" << resp << "]" << std::endl;
#endif
    // the first option is going to be AUTO.search for it
    size_t p = resp.find("NONE");
    std::string range = resp.substr(p);
    std::vector<std::string> tokens;
    util::tokenize_string(range, tokens, " ");

    uint16_t k = 1;
    for (std::string t : tokens)
    {
      m_ave_windows.insert({k,t});
      k++;
    }
  }
}

void PowerMeter::get_all_ranges(int16_t &current_setting)
{
  std::ostringstream cmd;
  cmd << "AR";
#ifdef DEBUG
  std::cout << "PowerMeter::get_all_ranges : Sending query [" << cmd.str() << "]" << std::endl;
#endif
  write_cmd(cmd.str());
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::get_all_ranges : got answer [" << resp << "]" << std::endl;
#endif
  // strip the first two bytes, as it indicates whether the command failed or not and in this case it does not matter
  resp = resp.substr(2);

  // now we want to tokenize the returned string and set the map
  std::vector<std::string> tokens;
  util::tokenize_string(resp, tokens, " ");

#ifdef DEBUG
  std::cout << "PowerMeter::get_all_ranges : Found " << tokens.size() << " entries" << std::endl;
#endif

  // however the first token is the current setting
  current_setting = std::stol(tokens.at(0)) & 0xFFFF;

  // remove that token
  //tokens.pop_front();
  tokens.erase(tokens.begin());

  // now clear the map
  m_ranges.clear();
  // the first token should be "AUTO"
  int counter = -1;
  for (std::string entry: tokens)
  {
#ifdef DEBUG
    std::cout << "PowerMeter::get_all_ranges : Adding entry [" << counter << " : " << entry << "]" << std::endl;
#endif
    m_ranges.insert({counter,entry});
    counter ++;
  }
}

// the logic is very simular to the previous method (for ranges)
// the problem is that the output is very device specific
// TODO: Get information from LANL
void PowerMeter::get_all_wavelengths(int16_t &current_setting)
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

void PowerMeter::get_all_wavelengths(std::string &answer)
{
  std::string cmd = "AW";
  std::string rr;
#ifdef DEBUG
  std::cout << "PowerMeter::get_all_wavelengths : Sending query [" << cmd << "]" << std::endl;
#endif
  send_cmd(cmd,rr);
  // just remove the first byte and return the rest
  answer = rr.substr(1);
}


void PowerMeter::bc20_sensor_mode(BC20 query,BC20 &answer)
{
  std::ostringstream cmd;
  cmd << "BQ " << static_cast<int>(query);
#ifdef DEBUG
  std::cout << "PowerMeter::bc20_sensor_mode : Sending query [" << cmd.str() << "]" << std::endl;
#endif
  write_cmd(cmd.str());
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::bc20_sensor_mode : got answer [" << resp << "]" << std::endl;
#endif
  // strip the first byte, as it indicates whether the command failed or not
  //char s= resp.at(0);
  //success = (s == '*')?true:false;
  // the third byte is the setting that is still in place
  answer = static_cast<BC20>(std::stol(resp.substr(2,1)));
}

void PowerMeter::diffuser_query(const DiffuserSetting s, DiffuserSetting &answer)
{
  std::ostringstream cmd;
  cmd << "DQ " << static_cast<int>(s);
#ifdef DEBUG
  std::cout << "PowerMeter::diffuser_query : Sending query [" << cmd.str() << "]" << std::endl;
#endif
  write_cmd(cmd.str());
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::diffuser_query : got answer [" << resp << "]" << std::endl;
#endif


  // Here we need to be a bit smarter, since the device is N/A the return code is the same
  // as OUT mode on a valid device...how to not design drivers
  // so, let's use that as the primary separator
  if (resp.find("N/A") != resp.npos)
  {
    // it does not matter whether it succeeded or failed
    // the setting has no meaning in this device
    answer = dNA;
  }
  else
  {
    // actually we do not care whether it succeeded or failed
    // all we care is to get the currently registered setting
    // and since we already ruled out that it was NA, then
    // we can simply do a cast
    answer = static_cast<DiffuserSetting>(std::stol(resp.substr(2,1)));
  }
}

void PowerMeter::exposure_energy(double &energy, uint32_t &pulses, uint32_t &et)
{
  std::string cmd = "EE";
#ifdef DEBUG
  std::cout << "PowerMeter::exposure_energy : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::exposure_energy : got answer [" << resp << "]" << std::endl;
#endif

  if (resp.at(0) == '*')
  {
    // tokenize the response string
    // now we want to tokenize the returned string and set the map
    std::vector<std::string> tokens;
    util::tokenize_string(resp, tokens, " ");
#ifdef DEBUG
    std::cout << "PowerMeter::exposure_energy : Found " << tokens.size() << " entries (expected 4)" << std::endl;
#endif
    energy = std::stod(tokens.at(1));
    pulses = std::stoul(tokens.at(2));
    et = std::stoul(tokens.at(3));
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
#ifdef DEBUG
  std::cout << "PowerMeter::energy_flag : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::energy_flag : got answer [" << resp << "]" << std::endl;
#endif
  new_val = (std::stol(resp.substr(1)) == 0)?false:true;
}

void PowerMeter::energy_ready(bool &new_val)
{
  std::string cmd = "ER";
#ifdef DEBUG
  std::cout << "PowerMeter::energy_ready : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::energy_ready : got answer [" << resp << "]" << std::endl;
#endif
  new_val = (std::stol(resp.substr(1)) == 0)?false:true;
}

//FIXME: Confirm the settings (mismatch between documentation and slow control spreadsheet)
void PowerMeter::energy_threshold(const uint32_t et, uint32_t &answer)
{
  std::ostringstream cmd;
  cmd << "ET " << et;
#ifdef DEBUG
  std::cout << "PowerMeter::energy_threshold : Sending query [" << cmd.str() << "]" << std::endl;
#endif

  write_cmd(cmd.str());

  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::energy_threshold : got answer [" << resp << "]" << std::endl;
#endif

  answer = std::stoul(resp.substr(1,1));

  m_e_threshold = answer;
}

void PowerMeter::force_energy(bool &success)
{
  std::string cmd = "FE";
#ifdef DEBUG
  std::cout << "PowerMeter::force_energy : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::force_energy : got answer [" << resp << "]" << std::endl;
#endif
  success = (resp.at(0) == '?')?false:true;
}

void PowerMeter::force_power(bool &success)
{
  std::string cmd = "FP";
#ifdef DEBUG
  std::cout << "PowerMeter::force_power : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::force_power : got answer [" << resp << "]" << std::endl;
#endif
  success = (resp.at(0) == '?')?false:true;
}

void PowerMeter::filter_query(const DiffuserSetting s, DiffuserSetting &answer)
{
  std::ostringstream cmd;
  cmd << "FQ " << static_cast<int>(s);
#ifdef DEBUG
  std::cout << "PowerMeter::filter_query : Sending query [" << cmd.str() << "]" << std::endl;
#endif
  write_cmd(cmd.str());
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::filter_query : got answer [" << resp << "]" << std::endl;
#endif


  // Here we need to be a bit smarter, since the device is N/A the return code is the same
  // as OUT mode on a valid device...how to not design drivers
  // so, let's use that as the primary separator
  if (resp.find("N/A") != resp.npos)
  {
    // it does not matter whether it succeeded or failed
    // the setting has no meaning in this device
    answer = dNA;
  }
  else
  {
    // actually we do not care whether it succeeded or failed
    // all we care is to get the currently registered setting
    // and since we already ruled out that it was NA, then
    // we can simply do a cast
    answer = static_cast<DiffuserSetting>(std::stol(resp.substr(2,1)));
  }
}

void PowerMeter::force_exposure(bool &success)
{
  std::string cmd = "FX";
#ifdef DEBUG
  std::cout << "PowerMeter::force_exposure : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::force_exposure : got answer [" << resp << "]" << std::endl;
#endif
  success = (resp.at(0) == '?')?false:true;
}


void PowerMeter::head_info_raw(std::string &answer)
{
  std::string cmd = "HI";
#ifdef DEBUG
  std::cout << "PowerMeter::head_info_raw : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  std::string resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::head_info_raw : got answer [" << resp << "]" << std::endl;
#endif
  answer = resp;
}


void PowerMeter::head_info(std::string &type, std::string &sn, std::string &name, bool &power, bool &energy, bool &freq)
{
  std::string rr;
  head_info_raw(rr);
  // tokenize it and get rid of first entry
  std::vector<std::string> tokens;
  util::tokenize_string(rr, tokens, " ");
  tokens.erase(tokens.begin());
  type = tokens.at(0);
  sn = tokens.at(1);
  name = tokens.at(2);
  int word = std::stoi(tokens.at(3),0, 16);

  // power is bit 0
  power = word & (1 << 0);
  // energy is bit 1
  energy = word & (1 << 0);
  // frequency is bit 31
  freq = word & (1 << 31);
}

void PowerMeter::head_type(std::string &type)
{
  std::string resp;
  send_cmd("HT",resp);
  type = resp.substr(1);
}

void PowerMeter::inst_config(bool &success)
{
  std::string resp;
  send_cmd("IC",resp);
  success = (resp.at(0) == '?')?false:true;
}

void PowerMeter::inst_info(std::string &id, std::string &sn, std::string &name)
{
  std::string rr;
  send_cmd("II",rr);
  // tokenize it and get rid of first entry
  std::vector<std::string> tokens;
  util::tokenize_string(rr, tokens, " ");
  // drop the first token
  tokens.erase(tokens.begin());
  id = tokens.at(0);
  sn = tokens.at(1);
  name = tokens.at(2);
}

void PowerMeter::mains_frequency(MainsFreq q, MainsFreq &answer)
{
  std::ostringstream msg;
  std::string resp;
  msg << "MA " << static_cast<int>(q);
  send_cmd(msg.str(),resp);
  answer = static_cast<MainsFreq>(std::stoi(resp.substr(2,1)));
}

void PowerMeter::max_freq(uint32_t &value)
{
  std::string rr;
  send_cmd("MF",rr);
  value = std::stoul(rr.substr(1));
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
  send_cmd(msg.str(),resp);
  // this command is silly. The answer varies wildly depending on the setting
  if (q == 0)
  {
    // we were querying current setting. All we care about is
    // the second byte
    a = std::stoi(resp.substr(1));
  }
  else {
    // we are trying to set a new setting
    // the answer is different depending whether it was successful or not
    if (resp.at(0) == '*')
    {
      // success.
      a = q;
    } else
    {
      // failed
      // then query what is the current setting
      measurement_mode(0,a);
    }
  }
}

void PowerMeter::pulse_length(const uint16_t value, uint16_t &answer)
{
  std::ostringstream msg;
  std::string resp;

  msg << "PL " << value;
  send_cmd(msg.str(),resp);
  // this command is silly. The answer varies wildly depending on the setting
  if (value == 0)
  {
    // we were querying current setting. All we care about is
    // the second byte
    answer = std::stoul(resp.substr(1,1));

    // if the map is not filled, then fill it
    if (m_pulse_lengths.size() == 0)
    {
      resp= resp.substr(2);
#ifdef DEBUG
  std::cout << "PowerMeter::pulse_length : string to tokenize : [" << resp << "]" << std::endl;
#endif

      // tokenize it and get rid of first entry
      std::vector<std::string> tokens;
      util::tokenize_string(resp, tokens, " ");
      // now insert these into the map
      uint16_t k = 1;
      for (std::string entry: tokens)
      {
#ifdef DEBUG
        std::cout << "PowerMeter::get_all_ranges : Adding entry [" << k << " : " << entry << "]" << std::endl;
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


void PowerMeter::reset()
{
  std::string rr;
  send_cmd("RE",rr);
}

void PowerMeter::get_range(int32_t &value)
{
  std::string rr;
  send_cmd("RN",rr);
  // drop the first byte
  value = std::stoi(rr.substr(1));
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
  send_cmd("SE",rr);
  // drop the first byte
  value = std::stod(rr.substr(1));
}

void PowerMeter::send_frequency(double &value)
{
  std::string rr;
  send_cmd("SF",rr);
  // drop the first byte
  value = std::stod(rr.substr(1));
}

void PowerMeter::send_average(double &value)
{
  std::string rr;
  send_cmd("SG",rr);
  // drop the first byte
  value = std::stod(rr.substr(1));
}

void PowerMeter::send_units(char &unit)
{
  std::string rr;
  send_cmd("SI",rr);
  // drop the first byte
  unit = rr.at(1);
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
  send_cmd("SP",rr);
  // drop the first byte
  value = std::stod(rr.substr(1));
}

void PowerMeter::send_max(double &value)
{
  std::string rr;
  send_cmd("SX",rr);
  // drop the first byte
  rr = rr.substr(1);
  // -- check if we are in auto mode
  if (rr == "AUTO")
  {
    value = 0.0;
  }
  else
  {
    // convert the value
    value = std::stod(rr);
  }
}

void PowerMeter::trigger_window(const uint16_t value, uint16_t &answer)
{
  std::ostringstream cmd;
  cmd << "TW " << value;
  std::string rr;
  send_cmd(cmd.str(),rr);
  // first strip the return byte
  rr = rr.substr(1);
  answer = std::stoul(rr) & 0xFFFF;
}

void PowerMeter::user_threshold(const uint16_t value, uint16_t &answer)
{
  std::ostringstream cmd;
  cmd << "UT " << value;
  std::string rr;
  send_cmd(cmd.str(),rr);
  // first strip the return byte
  rr = rr.substr(1);
  // now tokenize the answer
  std::vector<std::string> tokens;
  util::tokenize_string(rr, tokens, " ");
  answer = std::stoul(tokens.at(0)) & 0xFFFF;
  m_e_threshold = answer;

  // we should also store these values
  // the next two values are the minimum range and max range
  // min = tokens.at(1);
  // max = tokens.at(2);
}

void PowerMeter::query_user_threshold(uint16_t &current, uint16_t &min, uint16_t &max)
{
  std::string cmd = "UT";
  std::string rr;
  send_cmd(cmd,rr);
  // first strip the return byte
  rr = rr.substr(1);
  // now tokenize the answer
  std::vector<std::string> tokens;
  util::tokenize_string(rr, tokens, " ");
  current = std::stoul(tokens.at(0)) & 0xFFFF;
  m_e_threshold = current;
  min = std::stoul(tokens.at(1)) & 0xFFFF;
  max = std::stoul(tokens.at(2)) & 0xFFFF;


}

void PowerMeter::version(std::string &value)
{
  std::string cmd = "VE";
  std::string rr;
  send_cmd(cmd,rr);
  // first strip the return byte
  value = rr.substr(1);
}

void PowerMeter::wavelength_index(const uint16_t index, bool &success)
{
  std::ostringstream cmd;
    cmd << "UT " << index;
    std::string rr;
    send_cmd(cmd.str(),rr);

    // first strip the return byte
    success=(rr.at(0)=='*')?true:false;

}

void PowerMeter::wavelength(const uint16_t wl, bool &success)
{
  std::ostringstream cmd;
    cmd << "UT " << wl;
    std::string rr;
    send_cmd(cmd.str(),rr);

    // first strip the return byte
    success=(rr.at(0)=='*')?true:false;
}

void PowerMeter::write_range(const int16_t range, bool &success)
{
  std::ostringstream cmd;
    cmd << "WN " << range;
    std::string rr;
    send_cmd(cmd.str(),rr);

    // first strip the return byte
    success=(rr.at(0)=='*')?true:false;

}


//void PowerMeter::set_measurement_mode(const uint16_t mode)
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

void PowerMeter::send_cmd(const std::string cmd, std::string &resp)
{
#ifdef DEBUG
  std::cout << "PowerMeter::send_cmd : Sending query [" << cmd << "]" << std::endl;
#endif
  write_cmd(cmd);
  resp = m_serial.readline(0xFFFF, "\r\n");
#ifdef DEBUG
  std::cout << "PowerMeter::send_cmd : got answer [" << resp << "]" << std::endl;
#endif
}

void PowerMeter::init_pulse_lengths()
{
  m_pulse_lengths.clear();
  std::string resp;
  // do first a query
   std::string cmd = "MM 0";
   send_cmd(cmd,resp);
   // now parse response
   std::vector<std::string> tokens;
   util::tokenize_string(resp, tokens, " ");
   // drop the first token (success)
   tokens.erase(tokens.begin());
   // place all others in the map
   size_t c = 1;
   for (std::string t : tokens)
   {
     m_pulse_lengths.insert({c,t});
     c++;
   }

}

}

