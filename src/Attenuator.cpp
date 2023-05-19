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
  m_is_moving(false),
  m_speed(59000), // whatever was in the python code
  m_resolution(2), // half-step mode
  m_trans(1.0)
  {
  // open the serial port with a 1s timeout by default
  m_serial.setBaudrate(m_baud);
  m_serial.setPort(m_comport);
  m_serial.setBytesize(serial::eightbits);
  m_serial.setParity(serial::parity_none);
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

}

Attenuator::~Attenuator ()
{
  if (m_serial.isOpen())
  {
    m_serial.close();
  }
}


//TODO: Note that the functionality here is slightly different.
// unlike the original python, this method does not lock waiting for a valid status
// that should never be the responsibility of the library, but rather of the user code.
// for instance, one could want to have separate threads checking on the position
// and therefore would have no use for having the system locking while waiting for a status of '0'
void Attenuator::get_position(int32_t &position, char &status, bool wait)
{
  // query status and position of the attenuator motor
  m_serial.write(std::string("o"));
  std::string resp = m_serial.readline(0xFFFF, std::string("\r"));
  // the answer already comes stripped from the carriage return '\r'
#ifdef DEBUG
  std::cout << "Attenuator::get_position : Resp ["<< resp << "]" << std::endl;
#endif
  // tokenize the response
  std::vector<std::string> tokens;
  util::tokenize_string(resp,tokens);

  position = std::stol(tokens.at(1));
  status = tokens.at(0).at(1);

  // wait for the position status to
  // be stable
  if (wait)
  {
    while(tokens.at(0).at(1) != '0')
    {
#ifdef DEBUG
      std::cout << "Attenuator::get_position : status ["<< tokens.at(0).at(1) << "] pos [" << tokens.at(1) << "]" << std::endl;
#endif
      m_serial.write(std::string("o"));
      resp = m_serial.readline(0xFFFF, std::string("\r"));
      util::tokenize_string(resp,tokens);
      position = std::stol(tokens.at(1));
      status = tokens.at(0).at(1);
      std::this_thread::sleep_for (std::chrono::milliseconds(m_timeout_ms));

    }
  }
  // the status is the second char on the first token
  if (tokens.at(0).at(1) != '0')
  {
    // the motor is still moving
    // FIXME: Should it simply wait or just answer whatever it has in the register?
#ifdef DEBUG
    std::cout << "Attenuator::get_position : status ["<< tokens.at(0).at(1) << "] pos [" << tokens.at(1) << "]" << std::endl;
#endif
  }

}

void Attenuator::move(const int32_t steps)
{
  // there is no need to range check, as int32_t *is* the allowed range
  std::ostringstream msg;
  msg << m_com_pre << "m "  << steps << m_com_post;
  write_cmd(msg.str());
  // don't do anything here. This is just to set the motor in motion
}

void Attenuator::go(const int32_t position)
{
  std::ostringstream msg;
  msg << m_com_pre << "g " << position << m_com_post;
  write_cmd(msg.str());

}

void Attenuator::set_speed(const uint32_t speed)
{
  m_speed = speed;
  std::ostringstream msg;
  msg << m_com_pre << "s "  << speed << m_com_post;
  write_cmd(msg.str());
}
/**
 *
 * @param res
 */
void Attenuator::set_resolution(const uint32_t res)
{
  uint32_t r = res;
  if (r == 16)
  {
    r = 6;
  }
  m_resolution = r;
  std::ostringstream msg;
  msg << m_com_pre << "r "  << r << m_com_post;
  write_cmd(msg.str());
}

void Attenuator::get_resolution(uint32_t &res)
{
  m_resolution = res;
  std::ostringstream msg;
  msg << m_com_pre << "p"<< m_com_post;
  write_cmd(msg.str());
  std::string resp = m_serial.readline(0xFFFF, std::string("\r"));
#ifdef DEBUG
  std::cout << "Attenuator::get_resolution : Resp ["<< resp << "]" << std::endl;
#endif
  // the answer we want is on the second byte after 'r'
  size_t pos = resp.find('r');
  char r = resp.at(pos+2);
  if (r == '6')
  {
    res = 16;
  } else
  {
    // see https://sentry.io/answers/char-to-int-in-c-and-cpp/
    res =  r - '0';
  }

}

const std::string Attenuator::get_status_raw()
{
  std::ostringstream msg;
  msg << m_com_pre << "p"<< m_com_post;
  write_cmd(msg.str());

  std::string resp = m_serial.readline(0xFFFF, std::string("\r"));
#ifdef DEBUG
  std::cout << "Attenuator::get_status : Resp ["<< resp << "]" << std::endl;
#endif
  return resp;
}

void Attenuator::refresh_status()
{
  std::ostringstream msg;
  msg << m_com_pre << "pc"<< m_com_post;
  write_cmd(msg.str());

  std::string resp = m_serial.readline(0xFFFF, std::string("\r"));
#ifdef DEBUG
  std::cout << "Attenuator::get_status : Resp ["<< resp << "]" << std::endl;
#endif

  // the first 2 bytes are the echo of the command that was sent
  // so, lets just get rid of them
  resp = resp.substr(2);
  // now let's tokenize the remaining string
  std::vector<std::string> tokens;
  util::tokenize_string(resp, tokens);

  // we should have 24 tokens
  // actually, there is a chance that we have 25, but the last is empty

  int command_mode = std::stol(tokens.at(0));
  enum MotorState s = static_cast<enum MotorState>(std::stol(tokens.at(1)));
  uint8_t acc_val = std::stol(tokens.at(2)) & 0xFF;
  uint8_t dec_val = std::stol(tokens.at(3)) & 0xFF;
  uint16_t speed_val = std::stol(tokens.at(4));
  uint8_t mov_current_val = std::stol(tokens.at(5)) & 0xFF;
  uint8_t idle_current_val = std::stol(tokens.at(6)) & 0xFF;
  uint8_t idle_current_val = std::stol(tokens.at(6)) & 0xFF;
  // 7 is for step-dir mode
  enum Resolution res =  static_cast<enum Resolution>(std::stol(tokens.at(8)));
  int motor_enabled = std::stol(tokens.at(9));
  // 11 : reserved
  // reset counter when passing zero position
  int reset_counter_on_zero = std::stol(tokens.at(11));
  // report when position was zeroed (zp)
  int report_on_zero = std::stol(tokens.at(12));
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



}


void Attenuator::set_transmission(const float trans)
{
  // first convert transmission range [0.0] to steps
  if ((trans < 0.0) || (trans > 1.00))
  {
    throw serial::SerialException("set transmission range within [0.0, 1.0]");
  }

  int32_t steps = trans_to_steps(trans);
  go(steps);

}

const int32_t Attenuator::trans_to_steps(const float trans)
{
  float steps = -43.333333 * 180/M_PI * std::acos(std::sqrt(trans));
  int32_t res = static_cast<int32_t>(steps)+m_offset+3900; //Same as above, this assumes default microstepping resolution. See Manual for more.
#ifdef DEBUG
    std::cout << "Attenuator::trans_to_steps :  trans ["
        << trans << "] --> steps [" << res << "]" << std::endl;
#endif

  return res;

}

void Attenuator::go_home()
{
  std::ostringstream msg;
  msg << m_com_pre << "zp"<< m_com_post;
  write_cmd(msg.str());
}

void Attenuator::set_current_position(const int32_t pos)
{
  char s;
  int32_t p;
  get_position(p,s);
  if (s != '0')
  {
#ifdef DEBUG
  std::cout << "Attenuator::set_current_position : Motor not in OK state ["
      << s << "]. Doing nothing." << std::endl;
#endif
    return;
  }
  std::ostringstream msg;
  msg << m_com_pre << "i "<< pos << m_com_post;
#ifdef DEBUG
  std::cout << "Attenuator::set_current_position : Current position ["
      << p << "] new position [" << pos << "]" << std::endl;
#endif
  write_cmd(msg.str());

}

void Attenuator::set_zero()
{
  char s;
  int32_t p;
  get_position(p,s);
  if (s != '0')
  {
#ifdef DEBUG
  std::cout << "Attenuator::set_zero : Motor not in OK state ["
      << s << "]. Doing nothing." << std::endl;
#endif
    return;
  }
  std::ostringstream msg;
  msg << m_com_pre << "h" << m_com_post;
#ifdef DEBUG
  std::cout << "Attenuator::set_zero : Current position ["
      << p << "] " << std::endl;
#endif
  write_cmd(msg.str());

}

void Attenuator::save_settings()
{
  std::ostringstream msg;
  msg << m_com_pre << "ss"<< m_com_post;
  write_cmd(msg.str());

}

}
