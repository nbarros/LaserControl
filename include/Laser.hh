/*
 * Laser.hh
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 *
 *      This class is a rather simplistic and direct port from Eric Deck's Laser python class
 *      to control the laser unit through a serial connection
 */

#ifndef SRC_LASER_HH_
#define SRC_LASER_HH_

#include <Device.hh>
#include <map>
#include <string>

namespace device
{

  /**
   * Available commands:
    Single Shot           : SS  : PD must be in 000 mode
    Security              : SE  : Read only
    Shutter               : SH [0/1] (off/on)
    Start/Stop            : ST [0/1] (off/on)
    Q-switch delay        : QS [XXX] Needs three digits
    Repetition Rate       : RR [XX.X]
    Power supply voltage  : VA [X.XX]
    Pulse division        : PD [XXX] 001 Single shot
    Shot counter          : SC


    Serial connection settings
    9600 BAUD, 8 BIT, NO PARITY, 1 STOP BIT (8N1)
   */
class Laser : public Device
{
public:

  enum Shutter{Open=0x1,Closed=0x0};
  enum Fire{Start=0x1,Stop=0x0};
  enum Mode{Commissioning=0,Calibration=1};
  enum Security{Normal=0,NoSerial=1,BadFlow=2,OverTemp=3,NotUsed=4,LaserHead=5,ExtInterlock=6,ChargePileUp=7,SimmerFail=8,FlowSwitch=9};

  Laser (const char* port = "auto", const uint32_t baud_rate = 9600);
  virtual ~Laser ();

  /**
   * Open or close the shutter with a binary command
   * @param s
   */
  void shutter(Shutter s);
  void shutter_open() { shutter(Shutter::Open); }
  void shutter_close() { shutter(Shutter::Closed); }

  /**
   * Start/Stop firing
   */
  //FIXME: This command should only be called when in Commissioning mode
  // in calibration mode, the firing control comes from the CIB
  void fire(enum Fire s);
  void fire_start() {fire(Fire::Start);}
  void fire_stop() {fire(Fire::Stop);}

  /**
   * Set a prescale for the shots extracted.
   * e.g. a prescale of 2 means that every other
   * shot will be extracted
   *
   * Max value is 99 (see pg. 29 of user manual)
   *
   * TODO: What does "extracted" mean? Allowed to leave the laser box?
   */
  void set_prescale(const uint32_t pre);
  void set_pulse_division(const uint32_t pd) {set_prescale(pd);}

  /**
   * Set the pump voltage for the laser lamp.
   * @param hv
   */
  void set_pump_voltage(float hv);

  /**
   * Set laser to single shot mode. Only valid if prescale is 000
   *
   * @param force forces the prescale to 000
   */
  void single_shot(bool force = false);

  /**
   * Read the Shot Count
   *
   * @param count
   */
  void get_shot_count(uint32_t &count);

  /**
   * read out error messages from the SureLite
   *
   * @param msgs
   */
  void security(std::string &code,std::string &msg);
  void security(Security &code,std::string &msg);
  void security(uint16_t &code,std::string &msg);

  void security(std::string &full_desc);
  /**
   * Set the repetition rate of the laser
   * # -- Should not be changed..
   * The rate is 10 or 20Hz and specific to the laser according to manual
   *
   * TODO: Do error check on the input "rate" is a number, as a string
   *       MUST BE FORMATTED AS "XX.X"
   *       ANYTHING ELSE WILL THROW AN ERROR
   *
   * @param rate
   */
  void set_repetition_rate(float rate);

  /**
   * Set qswitch. It must be a 3 digit value
   * units are us
   *
   * @param qs
   */
  void set_qswitch(uint32_t qs);

private:
  // disallow any kind of copy constructor or assignment operators
  Laser (Laser &&other) = delete;
  Laser (const Laser &other) = delete;
  Laser& operator= (const Laser &other) = delete;
  Laser& operator= (Laser &&other) = delete;

  /// Other private methods that may be useful
  ///



  bool m_is_firing;

  uint32_t m_prescale;
  float m_pump_hv;
  float m_rate;
  uint32_t m_qswitch;

  std::map<std::string,std::string> m_sec_map;

};
}
#endif /* SRC_LASER_HH_ */
