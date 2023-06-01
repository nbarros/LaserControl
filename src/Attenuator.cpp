/*
 * Attenuator.cpp
 *
 *  Created on: May 16, 2023
 *      Author: nbarros
 */

#include <Attenuator.hh>
#include <utilities.hh>
#include <iostream>
#include <sstream>
#include <cmath>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

namespace device
{
Attenuator::Attenuator (const char* port, const uint32_t baud_rate)
: Device(port,baud_rate),
  m_offset(0),
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
  // -- the attenuator is weird, as the termination of the
  // answers/reads is not the same as the
  // termination of the writes. Had to overload the functions

  // open the serial port with a 1s timeout by default
  m_serial.setBaudrate(m_baud);
  m_serial.setPort(m_comport);
  m_serial.setBytesize(serial::eightbits);
  m_serial.setParity(serial::parity_none);
  m_timeout_ms = 50;
  serial::Timeout t = serial::Timeout::simpleTimeout(m_timeout_ms);
  m_serial.setTimeout(t);
  m_serial.setStopbits(serial::stopbits_one);
  m_serial.open();

  if (!m_serial.isOpen())
  {
#ifdef DEBUG
    std::cerr << "Failed to open the port ["<< m_comport << ":" << m_baud << "]" << std::endl;
#endif
    throw serial::PortNotOpenedException("initial communication");
  }

  // make a call to refresh_status and refresh_position to read out current register settings
  refresh_status();
  refresh_position();
}

Attenuator::~Attenuator ()
{
  if (m_serial.isOpen())
  {
    m_serial.close();
  }
}


void Attenuator::move(const int32_t steps, int32_t &position, bool wait)
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
  std::ostringstream msg;
  msg  << "m "  << steps;
  write_cmd(msg.str());
  // don't do anything here. This is just to set the motor in motion
  if (wait)
  {
    enum MotorState s;
    get_position(position,s,wait);
  }
}

void Attenuator::go(const int32_t target,int32_t &position, bool wait )
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
  std::ostringstream msg;
  msg << "g " << target;
  write_cmd(msg.str());
  if (wait)
  {
    enum MotorState s;
    get_position(position,s,wait);
  }
}

void Attenuator::set_current_position(const int32_t pos)
{
  refresh_position();
  // check that state is '0'
  // if not throw an exception
  if (m_motor_state != Stopped)
  {
    std::ostringstream msg;
    msg << "Can't reconfigure motor until it is in a stopped (state=" << m_motor_state << ")";
#ifdef DEBUG
  std::cout << msg.str() << std::endl;
#endif
    throw std::runtime_error(msg.str());
  }

  std::ostringstream msg;
  msg << "i "<< pos;
#ifdef DEBUG
  std::cout << "Attenuator::set_current_position : Current position ["
      << m_position << "] new position [" << pos << "]" << std::endl;
#endif
  write_cmd(msg.str());

  // but now we should set the offset to the difference
  m_offset += (pos - m_position);
}

void Attenuator::set_zero()
{
  refresh_position();
  // check that state is '0'
  // if not throw an exception
  if (m_motor_state != Stopped)
  {
    std::ostringstream msg;
    msg << "Can't reconfigure motor until it is in a stopped (state=" << m_motor_state << ")";
#ifdef DEBUG
  std::cout << msg.str() << std::endl;
#endif
    throw std::runtime_error(msg.str());
  }

  std::string cmd = "h";
#ifdef DEBUG
  std::cout << "Attenuator::set_zero : Current position ["
      << m_position << "] " << std::endl;
#endif
  m_offset += m_position;
  write_cmd(cmd);

}


void Attenuator::stop(bool force)
{
  std::string cmd = "st";
  if (force)
  {
#ifdef DEBUG
    std::cout << "Attenuator::stop : WARNING: Issuing a forced stop" << std::endl;
#endif
    cmd = "b";
  }
  write_cmd(cmd);
  // this command should always be followed by a get_position
}


void Attenuator::go_home()
{
  refresh_position();
  // check that state is '0'
  // if not throw an exception
  if (m_motor_state != Stopped)
  {
    std::ostringstream msg;
    msg << "Can't reconfigure motor until it is in a stopped (state=" << m_motor_state << ")";
#ifdef DEBUG
  std::cout << msg.str() << std::endl;
#endif
    throw std::runtime_error(msg.str());
  }

  std::string msg = "zp";
  write_cmd(msg);
  m_offset = 0;
}

void Attenuator::set_resolution(const enum Resolution res)
{
  uint32_t r = static_cast<uint32_t>(res);
  if (r == 16)
  {
    r = 6;
  }
  m_resolution = res;
  std::ostringstream msg;
  msg << "r "  << r;
  write_cmd(msg.str());
}

void Attenuator::set_idle_current(const uint16_t val)
{
  std::ostringstream msg;
  // note, only values up to 255 are accepted
  msg << "ws "<< (val & 0xFF);
#ifdef DEBUG
  std::cout << "Attenuator::set_idle_current : Setting idle current to ["
      << msg.str() << "] (" << convert_current(val)<< "]" << std::endl;
#endif
  write_cmd(msg.str());
  m_current_idle = val;
}

void Attenuator::set_moving_current(const uint16_t val)
{
  std::ostringstream msg;
  msg << "wm "<< (val & 0xFF);
#ifdef DEBUG
  std::cout << "Attenuator::set_moving_current : Setting moving current to ["
      << msg.str() << "] (" << convert_current(val)<< "]" << std::endl;
#endif
  write_cmd(msg.str());
  m_current_move = val;
}

void Attenuator::set_acceleration(const uint16_t val)
{
  std::ostringstream msg;
  msg << "a "<< (val & 0xFF);
#ifdef DEBUG
  std::cout << "Attenuator::set_acceleration : Setting acceleration to ["
      << msg.str() << "]" << std::endl;
#endif
  write_cmd(msg.str());
  m_acceleration = val;
}

void Attenuator::set_deceleration(const uint16_t val)
{
  std::ostringstream msg;
  msg << "d "<< (val & 0xFF);
#ifdef DEBUG
  std::cout << "Attenuator::set_deceleration : Setting deceleration to ["
      << msg.str() << "]" << std::endl;
#endif
  write_cmd(msg.str());
  m_deceleration = val;
}

void Attenuator::set_max_speed(const uint32_t speed)
{
  std::ostringstream msg;
  msg << "s "  << speed ;

#ifdef DEBUG
  std::cout << "Attenuator::set_max_speed : Setting max speed to ["
      << msg.str() << "]" << std::endl;
#endif
  write_cmd(msg.str());
  m_max_speed = speed;
}

const std::string Attenuator::get_status_raw()
{
  std::string msg = "p";
  write_cmd(msg);
  std::string resp;
  read_cmd(resp);
#ifdef DEBUG
  std::cout << "Attenuator::get_status_raw : Resp ["<< resp << "]" << std::endl;
#endif
  // drop the echoed 'p'
  return resp.substr(1);
}

/// This command returns a string finished with 0x0A followed by 0x0D (\r\n)
void Attenuator::refresh_status()
{
  std::string msg= "pc";
  write_cmd(msg);

  std::string resp;
  read_cmd(resp);
#ifdef DEBUG
  std::cout << "Attenuator::refresh_status : Resp ["<< resp << "]" << std::endl;
#endif

  // the first 2 bytes are the echo of the command that was sent
  // so, lets just get rid of them
  resp = resp.substr(2);
  // now let's tokenize the remaining string
  std::vector<std::string> tokens;
  util::tokenize_string(resp, tokens);

  // we should have 24 tokens
  // actually, there is a chance that we have 25, but the last is empty
#ifdef DEBUG
  std::cout << "Attenuator::refresh_status : Have ["<< tokens.size() << "] tokens" << std::endl;
#endif

  m_op_mode = static_cast<enum OpMode>(std::stol(tokens.at(0)));
  m_motor_state = static_cast<enum MotorState>(std::stol(tokens.at(1)));
  m_acceleration = std::stoul(tokens.at(2)) & 0xFF;
  m_deceleration = std::stoul(tokens.at(3)) & 0xFF;
  m_max_speed = std::stoul(tokens.at(4));

  m_current_move = std::stoul(tokens.at(5)) & 0xFF;
  m_current_idle = std::stoul(tokens.at(6)) & 0xFF;
  // 7 is for step-dir mode
  m_resolution =  static_cast<enum Resolution>(std::stoul(tokens.at(8)));
  m_motor_enabled = std::stol(tokens.at(9));
  // 11 : reserved
  // reset counter when passing zero position
  m_reset_on_zero = (std::stol(tokens.at(11))?true:false);
  // report when position was zeroed (zp)
  m_report_on_zero = (std::stol(tokens.at(12))?true:false);
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

void Attenuator::get_position(int32_t &position, uint16_t &status, bool wait)
{
  // query status and position of the attenuator motor
  std::string msg("o");
  write_cmd(msg);
  std::string resp;
  read_cmd(resp);
  // the answer already comes stripped from the carriage return '\r'
#ifdef DEBUG
  std::cout << "Attenuator::get_position : Resp ["<< resp << "]" << std::endl;
#endif
  // drop the first byte, as it is the echoed command 'o'
  resp = resp.substr(1);
  // tokenize the response
  std::vector<std::string> tokens;
  util::tokenize_string(resp,tokens);

  position = std::stol(tokens.at(1));
  // check that the status is indeed just one char long
  if (tokens.at(0).size() != 1)
  {
    std::ostringstream msg;
    msg << "Expected only 1 char as status. Got " << tokens.at(0).size() << "(" << tokens.at(0) << ")";
    throw std::runtime_error(msg.str());
  }
  status = std::stoul(tokens.at(0));

  // -- if wait is set to true, we need to do the extra length of looping until status is 0
  if (wait)
  {
    while(status != 0)
    {
#ifdef DEBUG
      std::cout << "Attenuator::get_position : status ["<< status << "] pos [" << position << "]" << std::endl;
#endif
      // call the function again
      get_position(position,status,false);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

void Attenuator::get_position(int32_t &position, enum MotorState &status, bool wait)
{

  uint16_t tmp_st;
  get_position(position,tmp_st,wait);
  status = static_cast<enum MotorState>(tmp_st);
}


void Attenuator::set_transmission(const float trans, bool wait)
{
  // first convert transmission range [0.0] to steps
  if ((trans < 0.0) || (trans > 1.00))
  {
    std::ostringstream msg;
    msg << "Attenuator::set_transmission : transmission ["<< trans << "] not within range [0.0,1.0]";
#ifdef DEBUG
    std::cout << msg.str() << std::endl;
#endif
    throw std::range_error(msg.str());
  }

  int32_t steps = trans_to_steps(trans);
#ifdef DEBUG
    std::cout << "Attenuator::set_transmission : Setting target to " << steps << std::endl;
#endif

    int32_t p;
    go(steps,p,wait);
}


void Attenuator::save_settings()
{
  std::string msg = "ss";
  write_cmd(msg);
}

void Attenuator::reset_controller()
{
  std::string msg = "j";
  write_cmd(msg);
}

void Attenuator::set_serial_number(const std::string sn)
{
  std::string name(20,' ');
  if (sn.size() > 20)
  {
    // if it is too large, truncate it
    name = sn.substr(0,20);
#ifdef DEBUG
    std::cout << "Attenuator::set_serial_number : Truncating argument to " << name << std::endl;
#endif
  }
  else if (sn.size() < 20)
  {
    name = sn;
    name.insert(0, 20-name.size(),' ');
  }

#ifdef DEBUG
    std::cout << "Attenuator::set_serial_number : Setting serial number/name to [" << name << "]" << std::endl;
#endif

    std::ostringstream cmd;
    cmd << "sn " << name;
    write_cmd(cmd.str());
}

void Attenuator::get_serial_number(std::string &sn)
{
  std::string cmd = "n";
  write_cmd(cmd);
  read_cmd(sn);
  //std::string resp = m_serial.readline(0xFFFF, std::string("\r"));
  //#ifdef DEBUG
  //  std::cout << "Attenuator::get_serial_number : Resp ["<< resp << "]" << std::endl;
  //#endif
  // get rid of the echo byte
  sn = sn.substr(1);
  m_serial_number = sn;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
///
///         getters: these just returned the locally cached value. Should be preceded by a refresh_status
///
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////


void Attenuator::get_resolution(uint16_t &res)
{
  res = static_cast<uint16_t>(m_resolution);
  if (res == 6)
  {
    res = 16;
  }
}



////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
///
///         private methods
///
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

const int32_t Attenuator::trans_to_steps(const float trans)
{
  //TODO: Re-write this using the stepping configuration
  // the formula actually changes
  //FIXME: Cross-check the offset calculation

  float steps = -43.333333 * 180/M_PI * std::acos(std::sqrt(trans));
  int32_t res = static_cast<int32_t>(steps)+m_offset+3900; //Same as above, this assumes default microstepping resolution. See Manual for more.
#ifdef DEBUG
    std::cout << "Attenuator::trans_to_steps :  trans ["
        << trans << "] --> steps [" << res << "]" << std::endl;
#endif

  return res;

}


void Attenuator::refresh_position()
{
  // do not wait
  get_position(m_position,m_motor_state,false);
}

const float Attenuator::convert_current(uint16_t val)
{
  return 0.00835 * val;
}

void Attenuator::write_cmd(const std::string cmd)
{
  Device::write_cmd(cmd);
  // attenuator instruction on page 31 say that we need to
  // add an interval of 50ms between commands
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Attenuator::read_cmd(std::string &answer)
{
  size_t nbytes = m_serial.readline(answer,0xFFFF,"\r\n");

#ifdef DEBUG
  std::cout << "Received " << nbytes << " bytes with answer [" << util::escape(answer.c_str()) << "]" << std::endl;
#endif
  answer.erase(answer.size()-2);

}


}
