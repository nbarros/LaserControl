/*
 * Laser.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <Laser.hh>
#include <iostream>
#include <serial/serial.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include  <string>

namespace device {

Laser::Laser (const char* port, const uint32_t baud_rate)
: Device(port,baud_rate),
  m_is_firing(false),
  // FIXME: Add reasonable defaults to this
  m_prescale(0),
  m_pump_hv(1.0),
  m_rate(10.0),
  m_qswitch(400)
{
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
  m_serial.setBytesize(serial::eightbits);
  m_serial.setParity(serial::parity_none);
  m_serial.setStopbits(serial::stopbits_one);

  m_serial.open();
  if (!m_serial.isOpen())
  {
#ifdef DEBUG
    std::cerr << "Laser::Laser : Failed to open the port ["<< m_comport << ":" << m_baud << "]" << std::endl;
#endif
    throw serial::PortNotOpenedException("initial communication");
  } else
  {
#ifdef DEBUG
    std::cout << "Laser::Laser : Connection established" << std::endl;
#endif
    /// we have a connection. Lets initialize some settings

    set_prescale(m_prescale);

    set_pump_voltage(m_pump_hv);

    set_qswitch(m_qswitch);

  }

}

Laser::~Laser ()
{
}

void Laser::shutter(enum Shutter s)
{
  std::ostringstream cmd;
  cmd << "SH " << static_cast<uint32_t>(s);
  write_cmd(cmd.str());
//  if (s == Open)
//  {
//    m_is_firing = true;
//  } else
//  {
//    m_is_firing = false;
//  }
}

void Laser::fire(enum Fire s)
{
  std::ostringstream cmd;
  cmd << "ST " << static_cast<uint32_t>(s);
  write_cmd(cmd.str());
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
  write_cmd(cmd.str());

    m_prescale = pre;
}

void Laser::set_pump_voltage(float hv)
{
  /*
    "HV" is a float w/ two decimal places, as a string
    MUST BE FORMATTED AS "HV X.XX"
    ANYTHING ELSE WILL THROW AN ERROR
    We should avoid setting the pump voltage REALLY high...
    Perhaps set a ceiling at 1.30kV for now?
    All lasers seem to need less than 1.30kV for normal operations
    unit is kV
   */

  if (hv > 1.3 || hv < 0)
  {
#ifdef DEBUG
    std::cout << "Laser::set_pump_voltage : Argument out of range [" << hv << "] --> [0; 1.3]." << std::endl;
#endif

    throw std::range_error("Pump HV must be in the range [0,1.3]");
  }

  std::ostringstream cmd;
  cmd << "HV " << std::fixed << std::setprecision(2) << hv;
#ifdef DEBUG
    std::cout << "Laser::set_pump_voltage : Setting pump voltage to [" << cmd.str() << "]." << std::endl;
#endif

  write_cmd(cmd.str());

}

void Laser::set_ss_mode(bool force)
{
  if (force)
  {
    // set the prescale to 0 first, regardless of the previous value
    set_prescale(0);
  } else
  {
    // if prescale is not zero, throw an exception

  }

  std::string cmd = "SS";

#ifdef DEBUG
    std::cout << "Laser::set_ss_mode : Firing single shot." << std::endl;
#endif

  write_cmd(cmd);

}

void Laser::get_shot_count(uint32_t &count)
{
  std::string cmd = "SC";

   write_cmd(cmd);

   std::string resp = m_serial.readline(0xFFFF,m_com_post);
#ifdef DEBUG
   std::cout << "Laser::get_shot_count : Received answer [" << resp << "]" << std::endl;
#endif
   // according to the python script, the answer we want is in the bytes [3:11]
   count = stoul(resp.substr(3,9)); // 9 is the max length
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
  write_cmd(cmd);

  std::string resp = m_serial.readline(0xFFFF,m_com_post);
#ifdef DEBUG
   std::cout << "Laser::security : Received answer [" << resp << "]" << std::endl;
#endif

   code = resp;
   msg = m_sec_map[code];

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
  write_cmd(cmd.str());
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
    write_cmd(cmd.str());

    m_qswitch = qs;

}

}
