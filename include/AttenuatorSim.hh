/*
 * AttenuatorSim.hh
 *
 *  Created on: Apr 3, 2024
 *      Author: Nuno Barros
 */

#ifndef LASERCONTROL_INCLUDE_ATTENUATORSIM_HH_
#define LASERCONTROL_INCLUDE_ATTENUATORSIM_HH_

#include "Attenuator.hh"

namespace device
{

  /*
   * This class mimics all the methods of device::Attenuator without actually
   * connecting to the device itself
   * All relevant methods are overloaded here
   */
  class AttenuatorSim final : public Attenuator
  {
  public:

    enum OpMode {StepDir=0,Command=1};

    enum MovementStatus {Success=0x0,Fail=0x1,NotReady=0x2};

    enum Resolution {Full=0x1,Half=0x2,Quarter=4,Eighth=8,Sixteeth=6};

    enum MotorState {Stopped=0,Accelerating=1,Decelerating=2,Running=3, Unknown=0x4};

    AttenuatorSim ();
    virtual ~AttenuatorSim ();
    AttenuatorSim (const AttenuatorSim &other) = delete;
    AttenuatorSim (AttenuatorSim &&other) = delete;
    AttenuatorSim& operator= (const AttenuatorSim &other) = delete;
    AttenuatorSim& operator= (AttenuatorSim &&other) = delete;

    void move(const int32_t steps, int32_t &position, bool wait = false);
    void go(const int32_t target, int32_t &position, bool wait = false );
    void set_current_position(const int32_t pos);
    void set_zero();
    void stop(bool force = false);
    void go_home();
    void set_resolution(const uint16_t res) {set_resolution(static_cast<enum Resolution>(res));}
    void set_resolution(const enum Resolution res);

    void set_idle_current(const uint16_t val);
    void set_moving_current(const uint16_t val);
    void set_acceleration(const uint16_t val);
    void set_deceleration(const uint16_t val);
    void set_max_speed(const uint32_t speed);
    const std::string get_status_raw();
    void refresh_status();
    void get_position(int32_t &position, uint16_t &status, bool wait= false);
    void get_position(int32_t &position, enum MotorState &status, bool wait);

    void set_transmission(const float trans, bool wait = false);
    void save_settings();
    void reset_controller();
    void set_serial_number(const std::string sn);
    void get_serial_number(std::string &sn);


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

   void get_resolution(uint16_t &res);

   void get_current_idle(uint16_t &c) {c = m_current_idle;}
   void get_current_move(uint16_t &c) {c = m_current_move;}

   void get_reset_on_zero(bool &r) {r = m_reset_on_zero;}
   void get_report_on_zero(bool &r) { r = m_report_on_zero;}

  private:

    void refresh_position();
    const int32_t trans_to_steps(const float trans);
    const float convert_current(uint16_t val);
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
//    uint16_t m_resolution;

  };

} /* namespace device */

#endif /* LASERCONTROL_INCLUDE_ATTENUATORSIM_HH_ */
