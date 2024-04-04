/*
 * AttenuatorSim.cpp
 *
 *  Created on: Apr 3, 2024
 *      Author: Nuno Barros
 */

#include "AttenuatorSim.hh"
#include <utilities.hh>
#include <iostream>
#include <sstream>
#include <cmath>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

namespace device
{

  AttenuatorSim::AttenuatorSim ()
            : m_offset(0),
        m_max_speed(59000), // whatever was in the python code
        m_op_mode(Command),
        m_motor_state(Stopped),
        m_acceleration(0),
        m_deceleration(0),
        m_resolution(Half), // half-step mode
        m_motor_enabled(true),
        m_position(0),
        m_current_idle(10),
        m_current_move(100),
        m_reset_on_zero(false),
        m_report_on_zero(false),
        m_serial_number("unknown")
  {
    refresh_status();
    refresh_position();
  }

  AttenuatorSim::~AttenuatorSim ()
  {
    // nothing to be done because no real device was connected to
  }

  void AttenuatorSim::move(const int32_t steps, int32_t &position, bool wait)
  {
    refresh_position();
    // check that state is '0'
    // if not throw an exception
    if (m_motor_state != Stopped)
    {
      std::ostringstream msg;
      msg << "Can't move motor until it is in a stopped (state=" << m_motor_state << ")";
      throw std::runtime_error(msg.str());
    }
    // there is no need to range check, as int32_t *is* the allowed range

    m_position += steps;
    position = m_position;
  }

  void AttenuatorSim::go(const int32_t target,int32_t &position, bool wait )
  {
    refresh_position();
    // check that state is '0'
    // if not throw an exception
    if (m_motor_state != Stopped)
    {
      std::ostringstream msg;
      msg << "Can't move motor until it is in a stopped (state=" << m_motor_state << ")";
      throw std::runtime_error(msg.str());
    }
    // magic, we just reached the destination
    m_position = target;
    position = target;
  }

  void AttenuatorSim::set_current_position(const int32_t pos)
  {
    refresh_position();
    // check that state is '0'
    // if not throw an exception
    if (m_motor_state != Stopped)
    {
      std::ostringstream msg;
      msg << "Can't reconfigure motor until it is in a stopped (state=" << m_motor_state << ")";
      throw std::runtime_error(msg.str());
    }

    // but now we should set the offset to the difference
    m_offset += (pos - m_position);
  }


  void AttenuatorSim::set_zero()
  {
    refresh_position();
    // check that state is '0'
    // if not throw an exception
    if (m_motor_state != Stopped)
    {
      std::ostringstream msg;
      msg << "Can't reconfigure motor until it is in a stopped (state=" << m_motor_state << ")";
      throw std::runtime_error(msg.str());
    }

    std::string cmd = "h";
    m_offset += m_position;
  }

  void AttenuatorSim::stop(bool force)
  {
    std::string cmd = "st";
     // this command should always be followed by a get_position
  }


  void AttenuatorSim::go_home()
  {
    refresh_position();
    // check that state is '0'
    // if not throw an exception
    if (m_motor_state != Stopped)
    {
      std::ostringstream msg;
      msg << "Can't reconfigure motor until it is in a stopped (state=" << m_motor_state << ")";
      throw std::runtime_error(msg.str());
    }

    m_offset = 0;
  }

  void AttenuatorSim::set_resolution(const enum Resolution res)
  {
    uint32_t r = static_cast<uint32_t>(res);
    if (r == 16)
    {
      r = 6;
    }
    m_resolution = res;
  }

  void AttenuatorSim::set_idle_current(const uint16_t val)
  {
    m_current_idle = val;
  }

  void AttenuatorSim::set_moving_current(const uint16_t val)
  {
    m_current_move = val;
  }

  void AttenuatorSim::set_acceleration(const uint16_t val)
  {
    m_acceleration = val;
  }

  void AttenuatorSim::set_deceleration(const uint16_t val)
  {
    m_deceleration = val;
  }

  void AttenuatorSim::set_max_speed(const uint32_t speed)
  {

    m_max_speed = speed;
  }

  const std::string AttenuatorSim::get_status_raw()
  {
    return "unknown";
  }

  /// This command returns a string finished with 0x0A followed by 0x0D (\r\n)
  void AttenuatorSim::refresh_status()
  {

    // we should have 24 tokens
    // actually, there is a chance that we have 25, but the last is empty

    m_op_mode = OpMode::Command;
    m_motor_state = MotorState::Stopped;
    m_acceleration = 200;
    m_deceleration = 200;
    m_max_speed = 200;

    m_current_move = 42;
    m_current_idle = 42;
    // 7 is for step-dir mode
    m_resolution =  Resolution::Full;
    m_motor_enabled = true;
    // 11 : reserved
    // reset counter when passing zero position
    m_reset_on_zero = false;
    // report when position was zeroed (zp)
    m_report_on_zero = true;
    // 13 : reserved
    // 14 : reserved
    // 15 : reserved
    // 16 : step-dir mode
    // motor direction setting in step-dir mode (1 ccw, 0 cw)
    //int dir_step_dir = std::stol(tokens.at(16));
    // 17 : step-dir mode
    // enabled in step-dir mode (1 enabled, 0 disabled)
    //int enabled_step_dir = std::stol(tokens.at(17));
    // 18 reserved
    // 19 : step-dir mode
    // 20 : step-dir mode
    // 21 : reserved
    // 22 : reserved
    // 23 : reserved

    // all these settings should be added into local registers


  }



  //TODO: Note that the functionality here is slightly different.
  // unlike the original python, this method does not lock waiting for a valid status
  // that should never be the responsibility of the library, but rather of the user code.
  // for instance, one could want to have separate threads checking on the position
  // and therefore would have no use for having the system locking while waiting for a status of '0'

  void AttenuatorSim::get_position(int32_t &position, uint16_t &status, bool wait)
  {

    position = m_position;
    status = 0;
  }

  void AttenuatorSim::get_position(int32_t &position, enum MotorState &status, bool wait)
  {

    uint16_t tmp_st;
    get_position(position,tmp_st,wait);
    status = static_cast<enum MotorState>(tmp_st);
  }


  void AttenuatorSim::set_transmission(const float trans, bool wait)
  {
    // first convert transmission range [0.0] to steps
    if ((trans < 0.0) || (trans > 1.00))
    {
      std::ostringstream msg;
      msg << "Attenuator::set_transmission : transmission ["<< trans << "] not within range [0.0,1.0]";
      throw std::range_error(msg.str());
    }

    int32_t steps = trans_to_steps(trans);
    int32_t p;
    go(steps,p,wait);
  }


  void AttenuatorSim::save_settings()
  {
    std::string msg = "ss";
  }

  void AttenuatorSim::reset_controller()
  {
    std::string msg = "j";
  }

  void AttenuatorSim::set_serial_number(const std::string sn)
  {
    std::string name(20,' ');
    if (sn.size() > 20)
    {
      // if it is too large, truncate it
      name = sn.substr(0,20);
    }
    else if (sn.size() < 20)
    {
      name = sn;
      name.insert(0, 20-name.size(),' ');
    }
    m_serial_number= name;
  }

  void AttenuatorSim::get_serial_number(std::string &sn)
  {
    sn = m_serial_number;
  }

  ////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////
  ///
  ///         getters: these just returned the locally cached value. Should be preceded by a refresh_status
  ///
  ////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////


  void AttenuatorSim::get_resolution(uint16_t &res)
  {
    res = static_cast<uint16_t>(m_resolution);
    if (res == 6)
    {
      res = 16;
    }
    m_resolution = static_cast<AttenuatorSim::Resolution>(res);
  }



  ////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////
  ///
  ///         private methods
  ///
  ////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////

  const int32_t AttenuatorSim::trans_to_steps(const float trans)
  {
    //TODO: Re-write this using the stepping configuration
    // the formula actually changes
    //FIXME: Cross-check the offset calculation

    float steps = -43.333333 * 180/M_PI * std::acos(std::sqrt(trans));
    int32_t res = static_cast<int32_t>(steps)+m_offset+3900; //Same as above, this assumes default microstepping resolution. See Manual for more.

    return res;

  }


  void AttenuatorSim::refresh_position()
  {
    // do not wait
    get_position(m_position,m_motor_state,false);
  }

  const float AttenuatorSim::convert_current(uint16_t val)
  {
    return 0.00835 * val;
  }

  void AttenuatorSim::write_cmd(const std::string cmd)
  {
   }

  void AttenuatorSim::read_cmd(std::string &answer)
  {

  }


} /* namespace device */
