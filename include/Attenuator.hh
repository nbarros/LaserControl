/*
 * Attenuator.h
 *
 *  Created on: May 16, 2023
 *      Author: nbarros
 */

#ifndef ATTENUATOR_H_
#define ATTENUATOR_H_

#include <Device.hh>
#include <vector>

namespace device
{

class Attenuator : public Device
{
public:
  enum Status {Success=0x0,Fail=0x1,NotReady=0x2};
  Attenuator (const char* port = "/dev/ttyUSB0", const uint32_t baud_rate = 38400 );
  virtual ~Attenuator ();

  void get_position(int32_t &position, char &status, bool wait= true);
  /**
   *             Move motor by x steps. Return final pos as string
            x: integer in [-2147483646, 2147483646]
   *
   * @param steps
   */
  void move(const int32_t steps);
  /**
   * Move motor to position 'pos'. Return final position as string
            pos: integer in [-2147483646, 2147483646]
   *
   * @param position
   */
  void go(const int32_t position);

  void set_speed(const uint32_t speed);
  void get_speed(uint32_t &speed);

  void set_resolution(const uint32_t res);
  /**
   *    #Set Motor Microstepping resolution
        #1,2,4,8,6 = full, half, quarter, eighth, sixteenth step mode
        #Yes, "6" for 16. Not a typo.

        '''
        From the Manual:
        > Higher Microsteppings modes demonstrate better position accuracy and no motor resonance
        > It is Advisible to use Half-Stepping operation mode
        '''
   *
   * @param res
   */
  /**
   *         #Get the Motor's current microstepping resolution.
        #Returns an INT with the current resolution.
   *
   * @param res
   */
  void get_resolution(uint32_t &res);
  /**
   * Get Attenuator Error status
   * TODO: write error code parser
   * @return
   */
  const std::string get_status();
  /**
   * Move the atten s.t. transmission coefficient = trans (elem of [0,1])
   * @param trans
   */
  void set_transmission(const float trans);
  /**
   * Got to hardware, zero position and reset the step counter
   */
  void go_home();
  /**
   * Set current position to desired value
            pos: integer in [-2147483646, 2147483646]
   *
   * @param pos
   */
  void set_current_position(const int32_t pos);
  /**
   * Set current position as the zero position. This will be lost after a power cycle
            Unless settings are saved
   *
   */
  void set_zero();
  /**
   * save current settings to controller memory. Config stored this way will be restored after cycling the power
   */
  void save_settings();

private:
  /// private methods
//  void write_cmd(const std::string &cmd);
//  void close_port() {m_serial.close();}

//  bool is_open() {return m_serial.isOpen();}

  /**
   * Defining a function which takes transmission and returns an integer number of steps

    Wattpilot manual has a definition of this function on page 40
   *
   * @param trans
   * @return
   */
  const int32_t trans_to_steps(const float trans);

  /// local variables
//  std::string m_port;
//  uint32_t m_baud;
//  std::string m_com_pre;
//  std::string m_com_post;
//  serial::Serial m_serial;
//  uint32_t m_timeout;
  uint32_t m_offset;
  bool m_is_moving;
  uint32_t m_speed;
  uint32_t m_resolution;
  float m_trans;
};
}

#endif /* ATTENUATOR_H_ */
