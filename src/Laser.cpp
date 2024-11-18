/*
 * Laser.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <Laser.hh>
#include <iostream>
#include <serial/serial.h>
#include <utilities.hh>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
//#define DEBUG 1

namespace device {

Laser::Laser (const char* port, const uint32_t baud_rate)
: Device(port,baud_rate),
  m_is_firing(false),
  m_prescale(0),
  m_pump_hv(1.1),
  m_rate(10.0),
  m_qswitch(170),
  m_wait_read(false)
{

  // this is the really messed up truth...the real termination char is the \n
  m_read_sfx = m_com_sfx;
  // -- change the timeout to something smaller
  // 50 ms?
  // by default leave timeout to max
  //  serial::Timeout t = serial::Timeout::simpleTimeout(500);
  //  m_serial.setTimeout(t);
  // note that in C++ the base class constructor is the first to be called
  // by now the serial connection is set up and ready to be opened

  // -- init the security map
  m_sec_map.insert({"00","Normal"});
  m_sec_map.insert({"01","Surelite not in serial mode"});
  m_sec_map.insert({"02","Coolant flow interrupted"});
  m_sec_map.insert({"03","Coolant temperature over temp"});
  m_sec_map.insert({"04","(not used)"}); // this should actually be a problem
  m_sec_map.insert({"05","Laser head problem"});
  m_sec_map.insert({"06","External interlock"});
  m_sec_map.insert({"07","End of charge not detected before lamp fire"});
  m_sec_map.insert({"08","Simmer not detected"});
  m_sec_map.insert({"09","Flow switch stuck on"});

  // RS232 settings for surelite operation
  // pp. 42 of manual
  // 9600 baud
  // 8 bit byte
  // no parity
  // 1 stop bit
  m_serial.setPort(m_comport);
  m_serial.setBaudrate(m_baud);
  m_serial.setBytesize(serial::eightbits);
  m_serial.setParity(serial::parity_none);
  m_serial.setStopbits(serial::stopbits_one);
  serial::Timeout t = serial::Timeout::simpleTimeout(m_timeout_ms);
  m_serial.setTimeout(t);
  m_serial.open();
  if (!m_serial.isOpen())
  {
    std::ostringstream msg;
    msg << "Failed to open the port ["<< m_comport << ":" << m_baud << "]";
#ifdef DEBUG
    std::cerr << "Laser::Laser : "<< msg.str() << std::endl;
#endif
    throw serial::PortNotOpenedException(msg.str().c_str());
  } else
  {
#ifdef DEBUG
    std::cout << "Laser::Laser : Connection established" << std::endl;
#endif

  }
}

Laser::~Laser ()
{

}

void Laser::shutter(enum Shutter s)
{
  std::ostringstream cmd;
  cmd << "SH " << static_cast<uint32_t>(s);
  bool resp = write_cmd(cmd.str());
  if (!resp) 
  {
    // retry
    reset_connection();
    resp = write_cmd(cmd.str());
    if (!resp)
    {
      // command failed. We should throw an exception
      throw serial::IOException("Failed to send SH command");
    }
  }
}


void Laser::fire(enum Fire s)
{
  std::ostringstream cmd;
  cmd << "ST " << static_cast<uint32_t>(s);
  bool st = write_cmd(cmd.str());
  if (st != true)
  {
    reset_connection();
    st = write_cmd(cmd.str());
    if (!st)
    {
      throwserial::IOException("Failed to send ST command");
    }
  }
  if (s == Start)
  {
    m_is_firing = true;
  } else
  {
    m_is_firing = false;
  }
}


void Laser::set_prescale(uint32_t pre)
{
  /**
      "scale" is an integer, as a string
      MUST BE FORMATTED AS "PD XXX"
      ANYTHING ELSE WILL THROW AN ERROR
   *
   */
  // -- first do a range check
  if (pre > 99)
  {
#ifdef DEBUG
    std::cout << "Laser::set_prescale : prescale out of range (" << pre << "). Setting to 99" << std::endl;
#endif
    pre = 99;
  }

  std::ostringstream cmd;
  cmd << "PD " << std::setfill('0') << std::setw(3) << pre;
#ifdef DEBUG
    std::cout << "Laser::set_prescale : Setting prescale to [" << cmd.str() << "]." << std::endl;
#endif
  bool success = write_cmd(cmd.str());
  if (!success)
  {
    // reset connection adn retry
    reset_connection();
    success = write_cmd(cmd.str());
    if (!success)
    {
      throw serial::IOException("Failed to send PD command");
    }
  }
  m_prescale = pre;
}

void Laser::set_pump_voltage(float hv)
{
  /*
    "VA" is a float w/ two decimal places, as a string
    MUST BE FORMATTED AS "HV X.XX"
    ANYTHING ELSE WILL THROW AN ERROR
    We should avoid setting the pump voltage REALLY high...
    Perhaps set a ceiling at 1.30kV for now?
    All lasers seem to need less than 1.30kV for normal operations
    unit is kV
   */

  if (hv > 1.3 || hv < 0)
  {
    std::ostringstream msg;
    msg << "Argument out of range [" << hv << "] --> [0; 1.3].";
#ifdef DEBUG
    std::cout << "Laser::set_pump_voltage : " << msg.str() << std::endl;
#endif

    throw std::range_error(msg.str());
  }

  std::ostringstream cmd;
  cmd << "VA " << std::fixed << std::setprecision(2) << hv;
#ifdef DEBUG
    std::cout << "Laser::set_pump_voltage : Setting pump voltage to [" << cmd.str() << "]." << std::endl;
#endif

  bool success = write_cmd(cmd.str());
  if (!success)
  {
    // reset connection adn retry
    reset_connection();
    success = write_cmd(cmd.str());
    if (!success)
    {
      throw serial::IOException("Failed to send VA command");
    }
  }

  m_pump_hv = hv;
}

// enable single shot mode.
// this implies setting the prescale to 0, if it is not so
void Laser::single_shot()
{
  // set the prescale to 0 first, regardless of the previous value
  set_prescale(0);

  std::string cmd = "SS";
#ifdef DEBUG
    std::cout << "Laser::set_ss_mode : Enabling single shot mode." << std::endl;
#endif
  bool success = write_cmd(cmd.str());
  if (!success)
  {
    // reset connection adn retry
    reset_connection();
    success = write_cmd(cmd.str());
    if (!success)
    {
      throw serial::IOException("Failed to send SS command");
    }
  }
}

void Laser::get_shot_count(uint32_t &count)
{
  std::string cmd = "SC";

   write_cmd(cmd);
   std::string resp;
   //read_cmd(resp);

   std::vector<std::string> lines;
   read_lines(lines);

#ifdef DEBUG
  std::cout << "Laser::security : Received [" << lines.size() << "] answer tokens" << std::endl;
#endif
  if (lines.size() == 0)
  {
    reset_connection();
    read_lines(lines);
    if (lines.size() == 0)
    {
      throw serial::IOException("Failed to read shot count");
    } 
  }
  else if (lines.size() == 1)
  {
    resp = lines.at(0);
    if (resp.size() > 0)
    {
      resp.erase(resp.size()-1);
    }
  }
  else
  {
    // the second is the answer
    resp = lines.at(1);
    if (resp.size() > 0)
    {
      resp.erase(resp.size()-1);
    }
  }

   //   std::string resp = m_serial.readline(0xFFFF,m_com_sfx);
   //resp.erase(resp.size()-1);
//#ifdef DEBUG
//   std::cout << "Laser::get_shot_count : Received answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
//   resp = resp.substr(cmd.size()+1); // drop the echoed command
#ifdef DEBUG
   std::cout << "Laser::get_shot_count : Trimmed [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
   // according to the python script, the answer we want is in the bytes [3:11]
   count = stoul(resp); // 9 is the max length
   //FIXME: This may require revising
   // manual states
   // The response to SC (Shot count) is a 9 digit ASCII code terminated by a Carriage Return
   // but why is it starting on byte 3?

#ifdef DEBUG
   std::cout << "Laser::get_shot_count : Received answer [" << count << "]" << std::endl;
#endif

}

void Laser::security(std::string &code,std::string &msg)
{
  security(code);
  if (m_sec_map.count(code)!= 0)
  {
    msg = m_sec_map[code];
  }
  else
  {
    msg = "";
  }
}

void Laser::security(uint16_t &code,std::string &msg)
{
  std::string tmp_code;
  security(tmp_code,msg);
  code = static_cast<uint16_t>(std::stoul(tmp_code) & 0xFFFF);
}

void Laser::security(Security &code,std::string &msg)
{
  std::string tmp_code;
  security(tmp_code,msg);
  code = static_cast<Security>(std::stol(tmp_code));
}

void Laser::security(std::string &code)
{
  /* pp. 42 of manual
   * The response to SE is a 2 digit ASCII code, terminated by a Carriage Return
character. This response gives the status of the system. The possible values returned are listed in
Table 6 below.

00 Normal return
01 Surelite not in serial mode
02 Coolant flow interrupted
03 Coolant temperature over temp
04 (not used)
05 Laser head problem
06 External interlock
07 End of charge not detected before lamp fire
08 Simmer not detected
09 Flow switch stuck on
   */
  std::string cmd = "SE";
  std::string resp;
  bool success = write_cmd(cmd);
  if (!success)
  {
    // reset connection, retry
    reset_connection();
    success = write_cmd(cmd);
    if (!success)
    {
      throw serial::IOException("Failed to send SE command");
    }  
  }
  std::vector<std::string> lines;
  read_lines(lines);
  if (lines.size() == 0)
  {
    // failed to read. We already set the connection once. Just throw or try again?
    reset_connection();
    read_lines(lines);
    if (lines.size() == 0)
    {
      throw serial::IOException(std::string("Failed to read security code").c_str());
    }
  }
  else if (lines.size() != 2)
  {
    throw serial::IOException("Failed to read security code. Got unexpected number of tokens : " + std::to_string(lines.size()));
  }
  // expect 2 answers
#ifdef DEBUG
  std::cout << "Laser::security : Received [" << lines.size() << "] answer tokens" << std::endl;
#endif
  // the second is the answer
  resp = lines.at(1);
  resp.erase(resp.size()-1);
  //read_cmd(resp);

#ifdef DEBUG
   std::cout << "Laser::security : Received answer [" << util::escape(resp.c_str()) << "]" << std::endl;
#endif
   // get rid of the echoed command (plus termination)
   //resp = resp.substr(cmd.size()+1);
   //resp.erase(resp.size()-1);
//#ifdef DEBUG
//   std::cout << "Laser::security : Received answer [" << util::escape(resp.c_str()) << "]" << std::endl;
//#endif
   code = resp;
}


void Laser::set_repetition_rate(float rate)
{
#ifdef DEBUG
  std::cout << "Laser::set_repetition_rate : Setting repetition rate to [" << rate << "]." << std::endl;
#endif
  std::ostringstream cmd;
  cmd << "RR " << std::fixed << std::setprecision(1) << rate;
#ifdef DEBUG
  std::cout << "Laser::set_repetition_rate : submitting command [" << cmd.str() << "]." << std::endl;
#endif
  bool success = write_cmd(cmd.str());
  if (!success)
  {
    // reset connection, retry
    reset_connection();
    success = write_cmd(cmd);
    if (!success)
    {
      throw serial::IOException("Failed to send RR command");
    }
  }

#ifdef DEBUG
  std::cout << "Laser::set_repetition_rate : Command written" << std::endl;
#endif
  m_rate = rate;

}

void Laser::set_qswitch(uint32_t qs)
{
  if (qs > 999)
  {
#ifdef DEBUG
    std::cout << "Laser::set_qswitch : Attempting to set a ridiculously large value (" << qs << "). Setting to 999" << std::endl;
#endif
    qs = 999;
  }
  std::ostringstream cmd;
  cmd << "QS " << std::setfill('0') << std::setw(3) << qs;

#ifdef DEBUG
    std::cout << "Laser::set_qswitch : Submitting command (" << cmd.str() << "). " << std::endl;
#endif
    bool success = write_cmd(cmd.str());
    if (!success)
    {
      // reset connection, retry
      reset_connection();
      success = write_cmd(cmd);
      if (!success)
      {
        throw serial::IOException("Failed to send QS command");
      }
    }
    m_qswitch = qs;
}


bool Laser::write_cmd(const std::string cmd)
{
  bool ret = Device::write_cmd(cmd);
//  printf("Exit from write_cmd. Going to sleep for a bit\n");

  // attenuator instruction on page 31 say that we need to
  // add an interval of 50ms between commands
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
#ifdef DEBUG
    printf("Passed here\n");
    std::cout << "Laser::write_cmd : Command submitted (" << util::escape(cmd.c_str()) << ")." << std::endl;
#endif
  return ret;
}

bool Laser::read_cmd(std::string &answer)
{
  // wait for the port to be ready
  size_t nbytes = 0;
  // only do this wait if the timeout is not 0
  if (m_wait_read)
  {
    if(!m_serial.waitReadable())
    {
  #ifdef DEBUG
    std::cout << "Laser::read_cmd : Timed out waiting for a readable state. Attempting to read anyway." << std::endl;
  #endif
    }
  }
  // Need to read it twice...the first to get the echo command, and the second to get the answer
  nbytes = m_serial.readline(answer,0xFFFF,m_read_sfx);
#ifdef DEBUG
  std::cout << "Laser::read_cmd : Received " << nbytes << " bytes with answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif
  if (nbytes == 0)
  {
    // retry
    reset_connection();
    nbytes = m_serial.readline(answer,0xFFFF,m_read_sfx);
    if (nbytes == 0)
    {
      return false;
    }
  }
  else if (nbytes <= m_read_sfx.length())
  {
    // got something unexpected
    return false;
  }
  else
  {
    answer.erase(answer.size()-m_read_sfx.length());
  }
#ifdef DEBUG
  std::cout << "Laser::read_cmd : Trimmed answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif
  return true;

}

}
