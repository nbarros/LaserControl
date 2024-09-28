/*
 * calibrate_attenuator.cpp
 *
 *  Created on: May 31, 2024
 *      Author: Nuno Barros
 */


#include <Attenuator.hh>
#include <PowerMeter.hh>
#include <Laser.hh>

#include <chrono>
#include <thread>
#include <cmath>

using device::PowerMeter;
using device::Laser;
using device::Attenuator;

void sample_laser(PowerMeter &m, const uint32_t num_samples, std::vector<double> &samples)
{

  for (size_t i = 0 ; i < num_samples; i++)
  {
    // just keep taking samples until you can move on
    // no point in taking samples more often than 100 ms
    double tmp;
    if (m.read_energy(tmp))
    {
      samples.push_back(tmp);
      std::this_thread::sleep_for(std::chrono::milliseconds(99.5));
    }
  }
}


void fit_fcn()
{

}

void stepsToTrans(uint32_t pos,uint32_t offset = 0)
{
  return std::pow(std::cos((M_PI/180.)*((pos-(offset+3900))/43.333)),2);
}
//    return np.cos((np.pi/180)*((pos-(offset+3900))/43.333))**2 #From Altechna's manual.
//                                                               #Offset is a constant position at which transmission =0
//                                                               #deriving offset is the purpose of calibration
//                                                               #Note: this assumes the default microstepping resolution.
//                                                               #See manual for more


void transRoSteps(uint32_t pos,uint32_t offset = 0)
{
  return std::pow(std::cos((M_PI/180.)*((pos-(offset+3900))/43.333)),2);
}

