/*
 * LaserSim.cpp
 *
 *  Created on: Apr 3, 2024
 *      Author: Nuno Barros
 */

#include "LaserSim.hh"
#include <iostream>
#include <serial/serial.h>
#include <utilities.hh>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>

namespace device
{

  LaserSim::LaserSim ()
      :
      m_is_firing(false),
      m_prescale(0),
      m_pump_hv(1.1),
      m_rate(10.0),
      m_qswitch(170),
      m_wait_read(false)

  {
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

  }

  LaserSim::~LaserSim ()
  {
  }

  void LaserSim::shutter(enum Shutter s)
  {
    m_shutter_state = static_cast<uint32_t>(s);
  }


  void LaserSim::fire(enum Fire s)
  {
    if (s == Start)
    {
      m_is_firing = true;
    } else
    {
      m_is_firing = false;
    }
  }


  void LaserSim::set_prescale(uint32_t pre)
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
      pre = 99;
    }


    m_prescale = pre;
  }

  void LaserSim::set_pump_voltage(float hv)
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

      throw std::range_error(msg.str());
    }

     m_pump_hv = hv;
  }

  // enable single shot mode.
  // this implies setting the prescale to 0, if it is not so
  void LaserSim::single_shot()
  {
    // set the prescale to 0 first, regardless of the previous value
    set_prescale(0);

   }

  void LaserSim::get_shot_count(uint32_t &count)
  {
    count = m_shot_count;
  }

  void LaserSim::security(std::string &code,std::string &msg)
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

  void LaserSim::security(uint16_t &code,std::string &msg)
  {
    std::string tmp_code;
    security(tmp_code,msg);
    code = static_cast<uint16_t>(std::stoul(tmp_code) & 0xFFFF);
  }

  void LaserSim::security(Security &code,std::string &msg)
  {
    std::string tmp_code;
    security(tmp_code,msg);
    code = static_cast<Security>(std::stol(tmp_code));
  }

  void LaserSim::security(std::string &code)
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

     code = "00";
  }


  void LaserSim::set_repetition_rate(float rate)
  {
    m_rate = rate;

  }

  void LaserSim::set_qswitch(uint32_t qs)
  {
    if (qs > 999)
    {
      qs = 999;
    }
      m_qswitch = qs;
  }





} /* namespace device */
