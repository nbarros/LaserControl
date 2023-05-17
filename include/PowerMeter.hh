/*
 * PowerMeter.hh
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#ifndef POWERMETER_HH_
#define POWERMETER_HH_


#include <vector>
#include <map>
#include <Device.hh>

namespace device {

class PowerMeter: public Device
{
public:
  PowerMeter (const char* port = "/dev/ttyUSB0", const uint32_t baud_rate = 9600);
  virtual ~PowerMeter ();

  /**
   * Tell the meter what to feel for, in nm
   * @param wl
   */
  void set_wavelength(const uint32_t wl);
  void get_wavelength(uint32_t &wl);

  /**
   * Set the readout range of the meter
   * Resolution is from ~2% to 100% of the range
   * if you undercede or excede this, you won't trigger!
   *
   * Indices are "10J, 2J, 200mJ, 20mJ, 2mJ"
   *
   * @param range
   */
  void set_range(const uint32_t range, std::string &answer);
  void get_range(uint32_t &range);

  /**
   * We're *trying* to set the Energy Threshold to the minimum value.
   * This is a function called Energy Treshold in the guide to programmers.
   * FIXME: This doesn't work! Fix it!
   *
   * @param et
   */
  void set_e_threshold(const uint32_t et);
  void get_e_threshold(uint32_t &et);

  /**
   * Query the meter for a reading
   */
  std::string get_energy();


private:

  uint32_t m_range;
  uint32_t m_wavelength;

  uint32_t m_e_threshold;

  std::map<uint32_t,std::string> m_ranges;
};

}
#endif /* POWERMETER_HH_ */
