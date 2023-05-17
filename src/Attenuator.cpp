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
  m_resolution(1), // half-step mode
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
      std::this_thread::sleep_for (std::chrono::milliseconds(500));

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

const std::string Attenuator::get_status()
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
