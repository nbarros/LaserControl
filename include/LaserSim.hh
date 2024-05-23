/*
 * LaserSim.hh
 *
 *  Created on: Apr 3, 2024
 *      Author: Nuno Barros
 */

#ifndef LASERCONTROL_INCLUDE_LASERSIM_HH_
#define LASERCONTROL_INCLUDE_LASERSIM_HH_

#include "Laser.hh"
#include <Device.hh>
#include <map>
#include <string>

namespace device
{

  /*
   *
   */
  class LaserSim final : public Laser
  {
  public:
    LaserSim ();
    virtual ~LaserSim ();
    LaserSim (const LaserSim &other) = delete;
    LaserSim (LaserSim &&other) = delete;
    LaserSim& operator= (const LaserSim &other) = delete;
    LaserSim& operator= (LaserSim &&other) = delete;


    enum Shutter{Open=0x1,Closed=0x0};
    enum Fire{Start=0x1,Stop=0x0};
    enum Mode{Commissioning=0,Calibration=1};
    enum Security{Normal=0,NoSerial=1,BadFlow=2,OverTemp=3,NotUsed=4,LaserHead=5,ExtInterlock=6,ChargePileUp=7,SimmerFail=8,FlowSwitch=9};


    void shutter(Shutter s);
    void shutter_open() { shutter(Shutter::Open); }
    void shutter_close() { shutter(Shutter::Closed); }


    void fire(enum Fire s);
    void fire_start() {fire(Fire::Start);}
    void fire_stop() {fire(Fire::Stop);}


    void set_prescale(const uint32_t pre);
    void set_pulse_division(const uint32_t pd) {set_prescale(pd);}


    void set_pump_voltage(float hv);


    void single_shot();

    void get_shot_count(uint32_t &count);


    void security(std::string &code,std::string &msg);
    void security(Security &code,std::string &msg);
    void security(uint16_t &code,std::string &msg);

    void security(std::string &code);

    void set_repetition_rate(float rate);

    void set_qswitch(uint32_t qs);


  private:



    bool m_is_firing;

    uint32_t m_prescale;
    float m_pump_hv;
    float m_rate;
    uint32_t m_qswitch;

    std::map<std::string,std::string> m_sec_map;
    //std::string m_read_sfx;
    bool m_wait_read;
    uint32_t m_shot_count = 0;
    uint32_t m_shutter_state = 0;
  };

} /* namespace device */

#endif /* LASERCONTROL_INCLUDE_LASERSIM_HH_ */
