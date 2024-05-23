/*
 * test_AD5339.cpp
 *
 *  Created on: Oct 20, 2023
 *      Author: nbarros
 */

#include <AD5339.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

using std::cout;
using std::endl;

volatile std::atomic_bool run;

void signal_handler(int signal)
{
  run = false;
}

int main(int argc, char**argv)
{
  run = true;
  cout << "Setting a signal handler" << endl;
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  cout << "Setting up DAC on bus 8 and address 0xD" << endl;
  cib::AD5339 dev(8,0xD);

  if (!dev.is_ready())
  {
    cout << "Failed to initialize the device." << endl;
    return 0;
  }

  // -- let's loop over the device and update the settings, generating a saw signal
  cout << "Looping over both channels and continuously increment the DAC values" << endl;
  int ret = 0;
  while(run)
  {
    cout << "Restarting the sweep" << endl;
    for (uint16_t val = 0; val < 0xFFF; val++)
    {

      if (val < 3)
      {
        dev.set_debug(true);
      }
      else
      {
        dev.set_debug(false);
      }
      for (uint16_t ch = 1; ch <= 2; ch++)
      {
        ret = dev.write_dac(val,ch);
        if (!ret)
        {
          cout << "Failed to write value " << val << " to channel " << ch << endl;
          return 0;
        }
        // now read back
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int rdbck = dev.read_dac(ch);
        if (rdbck != val)
        {
          cout << "Read back error on channel "
              << ch << ". Set " << val << ", read " << rdbck << endl;
          return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }
  cout << "Stopping execution" << endl;

  return 0;
}


