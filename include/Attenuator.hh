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
/**
 * Watt-Pilot pp. 23 for refence
 * Available commands (pp. 31)
 * -------------------------
 *
 * ++ Important note: the controller always echoes back
 * the commands that are sent. So, parsing requires the command do be
 * stripped first
 *
 *
 * ent 0/1/off (enable motor : only in step-dir mode) <-- not implemented
 * dir [cc/ccw/off] : only in step-dir mode <-- not implemented
 * m x  : move x steps
 * g x  : go to absolute coordinate
 * i x  : set counter to value
 * h    : reset counter to zero
 * st   : stop smoothly. counter accuracy is maintained
 * b    : stop immediately. counter accuracy can degrade
 * zp   : go to hardware zero position and reset counter
 * r [1/2/3/4/6/8] : set resolution. Doc recommends half stepping
 * ws x : set motor current when it is idle (heat control)
 *        x in [0,255] I = 0.00835 * x (A)
 * wm x : set motor current when moving
 *        x in [0,255] I = 0.00835 * x (A)
 * wt x : set motor current in step-dir mode <-- not implemented
 * a x  : set motor acceleration
 *        x in [0,255]
 *        0 - no acceleration
 *        ++ setting acceleration helps to increase position repeatability (?)
 * d x  : set motor deceleration (same logic as acceleration)
 * s x  : set maximal motor speed
 *        x in [0, 65000]
 * p    : shows controller settings (direct terminal usage) <-- not implemented
 * pt   : controller settings in step-dir mote <-- not implemented
 * pc   : controller settings in command mode
 *        returns string in form (terminated with '\n\r'
 *
 *
 */
class Attenuator : public Device
{
public:

  enum OpMode {StepDir=0,Command=1};

  enum MovementStatus {Success=0x0,Fail=0x1,NotReady=0x2};

  enum Resolution {Full=0x1,Half=0x2,Quarter=4,Eighth=8,Sixteeth=6};

  enum MotorState {Stopped=0,Accelerating=1,Decelerating=2,Running=3, Unknown=0x4};

  /**
   * Serial connection parameters for the attenuator
   * baud 38400
   * 8 data bits
   * 1 stop bit
   * no parity
   *
   * @param port
   * @param baud_rate
   */
  Attenuator () { };
  Attenuator (const char* port, const uint32_t baud_rate = 38400 );
  virtual ~Attenuator ();

  /**
   *  Move motor by x steps.
   *  If wait is set to true, wait for the motor to finish movement before
   *  returning, and return the final position in position
   *
   * x: integer in
   *
   * @param steps number of steps to move. Can be an integer in the range [-2147483646, 2147483646]
   * @param wait [default false] wait for the movement to complete (status returning to 0)
   * @param position final position. Only returns a valid value if wait is true, otherwise returns
   *  	    the current position
   */
  void move(const int32_t steps, int32_t &position, bool wait = false);
  /**
   * Move motor to position 'pos'.
   *
   * If 'wait' is set to true, wait for the motor to reach position before returning
   * Return final position, if wait is true, current position is wait is false
            pos:
   *
   * @param target integer in [-2147483646, 2147483646]
   * @param position last known position
   * @param wait (default false) wait for the motor to reach destination
   */
  void go(const int32_t target, int32_t &position, bool wait = false );
  /**
   * Set current position to desired value.

   *
   * @param pos integer in [-2147483646, 2147483646]
   */
  void set_current_position(const int32_t pos);

  /**
   * Set current position as the zero position. This will be lost after a power cycle
   * Unless settings are saved
   *
   */
  void set_zero();
  /**
   * Stop the motor. If force is set to true, the stop will be much more aggressive
   * but the position register may become less precise.
   */
  void stop(bool force = false);
  /**
   * Go to hardware zero position and reset counter
   */
  void go_home();
  /**
   * Set Motor Microstepping resolution
   *
   * Valid values are:
   * 1,2,4,8,6 = full, half, quarter, eighth, sixteenth step mode
   *
   * Attenuator documentation recommends this to be set to 2 (half-stepping)
   *
   *     From the Manual:
   *     > Higher Microsteppings modes demonstrate better position accuracy and no motor resonance
   *     > It is Advisible to use Half-Stepping operation mode
   *
   * @param res resolution setting
   */
  void set_resolution(const enum Resolution res);
  void set_resolution(const uint16_t res) {set_resolution(static_cast<enum Resolution>(res));}

  /**
   * Set the current for idle.
   * The value must be in the range [0,255]
   *
   * The value converts to current as I = 0.00835 * val (Amps)
   *
   * Note that there is a minimum necessary to keep the settings in memory
   */
  void set_idle_current(const uint16_t val);

  /**
   * Set the current for moving state.
   * The value must be in the range [0,255]
   *
   * The value converts to current as I = 0.00835 * val (Amps)
   *
   */
  void set_moving_current(const uint16_t val);

  /**
   *
   * Set motor acceleration
   *
   * Value must be in the range [0,255]
   * 0 : no acceleration
   *
   * According to manual:
   * > Setting acceleration helps to increase position repeatability
   */
  void set_acceleration(const uint16_t val);

  /**
   *
   * Set motor deceleration
   *
   * Value must be in the range [0,255]
   * 0 : no deceleration
   *
   */
  void set_deceleration(const uint16_t val);

  /**
   * Set maximal motor speed
   *
   * @param speed the maximum speed. Must be in range [0,65000]
   */
  void set_max_speed(const uint32_t speed);

  /**
   * @fn const std::string get_status_raw()
   *
   * @brief Get the status string, as if it was requested from terminal
   * using command 'p'
   * @return user-friendly string. The equivalent of the get_status method in the python module
   */
  const std::string get_status_raw();

  /**
     * @fn void refresh_status()
     *
     * Refreshes the settings from the device registers (using 'pc' command
     * and updates the local variables
     *
     * It does not return. This method is called by every 'get' method, to obtain the latest state
   */
  void refresh_status();

  /**
   * @fn get_position(int32_t &position, char &status, bool wait= true)
   *
   * Returns the current position (and well as the movement status). If wait is true, the
   * function only returns when the status is '0'/stopped
   * @param position
   * @param status
   * @param wait
   */
  void get_position(int32_t &position, uint16_t &status, bool wait= false);
  void get_position(int32_t &position, enum MotorState &status, bool wait = false);


  /**
     * @fn void set_transmission(const float)
     *
     * Auxiliary function that sets the transmissibility of the
     * attenuator by converting the transmission to steps and then
     * moving the motor to the appropriate location
     *
     *
   * @param trans transmissibility, ranging from [0.0 to 1.0]
   * @param wait (default false) wait for the attenuator to be in that setting before returning
   */
   void set_transmission(const double trans, bool &success, bool wait = false);
   void get_transmission(double &transmission);
   /**
    * Save settings to the device registers. This only affects the counter manipulations
    */
   void save_settings();
   /**
    *
    */
   void reset_controller();
   /**
    *
    * @param sn
    */
   void set_serial_number(const std::string sn);
   /**
    *
    * @param sn
    */
   void get_serial_number(std::string &sn);

   ///
   ///
   /// Getter functions.
   ///
   ///




  void get_offset(int32_t &off) {off = m_offset;}

  /**
   * @fn void get_speed(uint32_t&)
   *
   * Get max speed
   *
   * @param speed
   */
  void get_max_speed(uint32_t &speed) {speed = m_max_speed;}

  void get_op_mode(enum OpMode &m) {m = m_op_mode;}

  void get_motor_state(MotorState &s) {s = m_motor_state;}

  void get_acceleration(uint16_t &a) {a = m_acceleration;}

  void get_deceleration(uint16_t &d) {d = m_deceleration;}

 /**
  *         #Get the Motor's current microstepping resolution.
       #Returns an INT with the current resolution.
  *
  * @param res
  */
 void get_resolution(uint16_t &res);

 void get_current_idle(uint16_t &c) {c = m_current_idle;}
 void get_current_move(uint16_t &c) {c = m_current_move;}

 void get_reset_on_zero(bool &r) {r = m_reset_on_zero;}
 void get_report_on_zero(bool &r) { r = m_report_on_zero;}

 void set_cal_parameters(const int offset, const double scale) {m_cal_offset = offset; m_cal_scale = scale;}
 void get_cal_parameters(int &offset, double &scale) {offset = m_cal_offset; scale = m_cal_scale;}
private:

  /**
     * @fn void refresh_position()
     *
     * Refreshes the current position and movement status.
     * This method is similar to the previous but is faster,
     * since it only requires 2 registers to be queried
   */
  void refresh_position();
  /**
   * Defining a function which takes transmission and returns an integer number of steps
   *
   * Wattpilot manual has a definition of this function on page 40
   *
   * @param transmissibility
   * @return corresponding position in steps
   */
  const int trans_to_steps(const double trans);
  const double steps_to_trans(const int steps);
  const float convert_current(uint16_t val);

  /**
   * Overload of Device::write_cmd, as the attenuator requires that 50 ms
   * are injected between commands
   * @param cmd
   */
  void write_cmd(const std::string cmd);
  void read_cmd(std::string &answer);



  /// local variables
  int32_t m_offset;
  uint32_t m_max_speed;

  enum OpMode m_op_mode;
  enum MotorState m_motor_state;
  uint16_t m_acceleration;
  uint16_t m_deceleration;

  enum Resolution m_resolution;

  bool m_motor_enabled;
  int32_t m_position;
  uint16_t m_current_idle;
  uint16_t m_current_move;

  bool m_reset_on_zero;
  bool m_report_on_zero;
  std::string m_serial_number;
  double m_cal_scale;
  int    m_cal_offset;
};
}

#endif /* ATTENUATOR_H_ */
