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
  m_range(2),
  m_wavelength(266),
  m_e_threshold(1)
{
  // override the prefix
  m_com_pre = "$";

  // initialize the ranges map
  m_ranges.insert({0,std::string("10J")});
  m_ranges.insert({1,std::string("2J")});
  m_ranges.insert({2,std::string("200mJ")});
  m_ranges.insert({3,std::string("2mJ")});

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
    // FIXME: Decide how to better handle a failure to open
    std::cerr << "Failed to open the port ["<< m_comport << ":" << m_baud << "]" << std::endl;
  } else
  {
    /// we have a connection. Lets initialize some settings

    // This is a function called "Average Query" in the guide to programmers
    // The aim is to turn off averaging on bootup... but that doesn't seem to work?
    std::string cmd = "AQ 0";
    write_cmd(cmd);

    // Set the wavelength to 266nm
    set_wavelength(m_wavelength);

    // We're *trying* to set the Energy Threshold to the minimum value.
    // This is a function called Energy Treshold in the guide to programmers.
    // This doesn't work! Fix it!
    set_e_threshold(m_e_threshold);

  }


}

PowerMeter::~PowerMeter ()
{

}


void PowerMeter::set_range(const uint32_t range, std::string &answer)
{
  std::map<uint32_t,std::string>::iterator it = m_ranges.end();
  if ((it = m_ranges.find(range)) != m_ranges.end())
  {
    // it is a valid range
#ifdef DEBUG
    std::cout << "PowerMeter::set_range : Setting range to " << it->second << std::endl;
#endif
    std::ostringstream cmd;
    cmd << "WN " << it->first;
    write_cmd(cmd.str());
    // then it is supposed to issue yet another command, but I am not sure what it does
    cmd.str("");
    cmd.flush();
    cmd << "AR";
    write_cmd(cmd.str());
    // this command is meant to have an answer, and therefore one should read it
    // TODO: What do we do with the answer? What's its structure?
    answer = m_serial.readline(0xFFF, "\r");
    m_range = range;

  } else
  {
#ifdef DEBUG
  std::cerr << "PowerMeter::set_range : Range "<< range << " out of valid values [0,3]" << std::endl;
#endif
    throw serial::SerialException("bad range value");
  }
}

void PowerMeter::get_range(uint32_t &range)
{
  // TODO: Implement this to query the instrument
  range = m_range;
}

void PowerMeter::set_wavelength(const uint32_t wl)
{
  std::ostringstream cmd;
  cmd << "WL " << wl;
  write_cmd(cmd.str());
  m_wavelength = wl;
}

void PowerMeter::get_wavelength(uint32_t &wl)
{
  // TODO: Implement this to query the instrument
  wl =  m_wavelength;
}

void PowerMeter::set_e_threshold(const uint32_t et)
{
  std::ostringstream cmd;
  cmd << "ET " << et;
  write_cmd(cmd.str());
  m_e_threshold = et;

}

void PowerMeter::get_e_threshold(uint32_t &et)
{
  // TODO: Implement this to query the instrument
  et =  m_e_threshold;
}

std::string PowerMeter::get_energy()
{
  // FIXME: What is exactly the output from this?
  // It would be capital if one could get the hands on the programmers guide.
  // It is good practice to add a link to said guide when writting code
  std::string cmd = "SE";
  write_cmd(cmd);
  // now read back a line. Note that we have a timeout set to whatever
  // after that timeout, the read command will return, regardless of what has been answered by
  // the instrument
  std::string answer = m_serial.readline(0xFFF, "\r");
#ifdef DEBUG
  std::cout << "PowerMeter::get_energy : Received answer [" << answer << "]" << std::endl;
#endif
  // using the same method as the python code (with split and partition). If the split fails, I'll just return everything
  //
  // ok, scratch that...this can be done better
  // the original code was : self.readline().split(' ')[1].partition('\r')[0]
  // but with this library, the '\r' should already be stripped
  std::vector<std::string> tokens;
  util::tokenize_string(answer, tokens, " ");
  std::string at = tokens.at(1);
  tokens.clear();
  util::tokenize_string(at, tokens, "\r");
#ifdef DEBUG
  std::cout << "PowerMeter::get_energy : Returning  [" << tokens.at(0) << "]" << std::endl;
#endif

  return tokens.at(0);
}

}

