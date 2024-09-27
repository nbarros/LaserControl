/*
 * test_all_devices.cc
 *
 *  Created on: May 31, 2023
 *      Author: nbarros
 */

#include <utilities.hh>
#include <Attenuator.hh>
#include <PowerMeter.hh>
#include <Laser.hh>

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using std::cout;
using std::endl;
using std::string;

#define log_msg(s,met,msg) "[" << s << "]::" << met << " : " << msg

#define log_e(m,s) log_msg("ERROR",m,s)
#define log_w(m,s) log_msg("WARN",m,s)
#define log_i(m,s) log_msg("INFO",m,s)
#define log(m) log_msg("INFO",m,"")

/* info from David
Laser cable serial info: (/dev/ttyUSB2, FTDI USB Serial Converter A9J0G3SV, USB VID:PID=0403:6001 SNR=A9J0G3SV;
Attenuator cable info: (/dev/ttyUSB0, Silicon Labs CP2102 USB to UART Bridge Controller 6ATT1538D, USB VID:PID=10c4:ea60 SNR=6ATT1538D)
Energy meter cable info: (/dev/ttyUSB1, FTDI FT232R USB UART A9JR8MJT, USB VID:PID=0403:6001 SNR=A9JR8MJT)

 */


///dev/ttyUSB2, FTDI USB Serial Converter FTC7LJXH, USB VID:PID=0403:6001 SNR=FTC7LJXH
void test_laser(const char*sn)
{
  const char*label = "laser:: ";
  cout << log(label) <<"Testing laser." << endl;
  std::string port = util::find_port(sn);
  if (port.size() == 0)
  {
    cout << log(label)<< "Failed to find the port" << endl;
    util::enumerate_ports();
  }
  else
  {
    cout << log(label)<< "Found port " << port << endl;
  }

  device::Laser *m_laser = nullptr;
  uint32_t baud_rate = 9600;
  // now do the real test
  try
  {
    m_laser = new device::Laser(port.c_str(),baud_rate);
    m_laser->set_timeout_ms(100);

    // this laser sucks. How are we going to figure out that we are operating properly?
    // we have access to nothing in its processor.
    cout << log(label)<< "Check interlocks..." << endl;
    std::string code;
    std::string desc;
    m_laser->security(code, desc);
    cout << log(label)<< "Laser responded [" << code << " : " << desc << "]" << endl;

    cout << log(label)<< "Get shot count..." << endl;
    uint32_t count;
    m_laser->get_shot_count(count);
    cout << log(label)<< "Laser responded [" << count << "]" << endl;

    // open and close the shutter for 500 ms
    cout << log(label)<< "Opening the shutter for 500ms..." << endl;
    m_laser->shutter_open();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    m_laser->shutter_close();
    cout << log(label)<< "All done. Can't test anything else without actually firing the laser" << endl;
  }
  catch(serial::PortNotOpenedException &e)
  {
    std::cout << log(label)<< "Port not open exception : " << e.what() << endl;
  }
  catch(serial::SerialException &e)
  {
    std::cout << log(label)<< "Serial exception : " << e.what() << endl;
  }
  catch(std::exception &e)
  {
    std::cout << log(label)<< "STL exception : " << e.what() << endl;
  }
  catch(...)
  {
    cout << log(label)<< "test_laser : Caught an unexpected exception" << endl;
  }
  // try to leave cleanly
  if (m_laser) delete m_laser;
}


///Attenuator cable info: (/dev/ttyUSB0, Silicon Labs CP2102 USB to UART Bridge Controller 6ATT1538D, USB VID:PID=10c4:ea60 SNR=6ATT1538D)
void test_attenuator(const char*sn)
{
  const char*label = "attenuator";
  cout << log(label) <<"Testing attenuator. Expect it to be in /dev/ttyUSB0" << endl;
  std::string port = util::find_port(sn);
  if (port.size() == 0)
  {
    cout << log(label)<< "Failed to find the port" << endl;
    util::enumerate_ports();
  }
  else
  {
    cout << log(label)<< "Found port " << port << endl;
  }
  if (port != "/dev/ttyUSB0")
  {
    cout << log(label) << "Was expecting a different port (/dev/ttyUSB0 <>" << port << ")" << endl;
  }

  device::Attenuator *m_att = nullptr;
  uint32_t baud_rate = 38400;
  // now do the real test
  try
  {
    m_att = new device::Attenuator(port.c_str(),baud_rate);

    // -- this sets the cache variables so that we can get a whole ton of stuff out of it
    cout << log(label)<< "Refreshing status" << endl;
    m_att->refresh_status();

    // the attenuator is actually pretty cool in terms of interface
    cout << log(label)<< "Querying serial number..." << endl;
    std::string sn;
    m_att->get_serial_number(sn);
    cout << log(label)<< "Response [" << sn << "]" << endl;
    // -- if this somehow fails,it may mean that the string configuration is wrong

    cout << log(label)<< "Querying status in human form..." << endl;
    std::string st = m_att->get_status_raw();
    cout << log(label)<< "Response [" << st << "]" << endl;

    // grab all the settings separately:
    cout << log(label) << "Querying individual settings (to see they were properly parsed):" << endl;
    int32_t pos;
    uint16_t status;
    m_att->get_position(pos, status, false);
    cout << log(label)<< "Position     : " << pos << endl;
    cout << log(label)<< "Status       : " << status << endl;
    int32_t offset;
    m_att->get_offset(offset);
    cout << log(label)<< "Offset       : " << offset << endl;
    uint32_t speed;
    m_att->get_max_speed(speed);
    cout << log(label)<< "Max speed    : " << speed << endl;
    device::Attenuator::OpMode mode;
    m_att->get_op_mode(mode);
    cout << log(label)<< "Op mode      : " << mode << endl;
    uint16_t acc, dec, res, cur_idle, cur_mov;
    m_att->get_acceleration(acc);
    cout << log(label)<< "Acceleration : " << acc << endl;
    m_att->get_deceleration(dec);
    cout << log(label)<< "Deceleration : " << dec << endl;
    m_att->get_resolution(res);
    cout << log(label)<< "Resolution   : " << res << endl;
    m_att->get_current_idle(cur_idle);
    cout << log(label)<< "Idle current : " << cur_idle << endl;
    m_att->get_current_move(cur_mov);
    cout << log(label)<< "Move current : " << cur_mov << endl;

    cout << log(label)<< "All done. Can't test anything else without actually changing settings.\n\n\n\n" << endl;
  }
  catch(serial::PortNotOpenedException &e)
  {
    std::cout << log(label)<< "Port not open exception : " << e.what() << endl;
  }
  catch(serial::SerialException &e)
  {
    std::cout << log(label)<< "Serial exception : " << e.what() << endl;
  }
  catch(std::exception &e)
  {
    std::cout << log(label)<< "STL exception : " << e.what() << endl;
  }
  catch(...)
  {
    cout << log(label)<< "test_laser : Caught an unexpected exception" << endl;
  }
  // try to leave cleanly
  if (m_att) delete m_att;
}

///Energy meter cable info: (/dev/ttyUSB1, FTDI FT232R USB UART A9JR8MJT, USB VID:PID=0403:6001 SNR=A9JR8MJT)
///
void test_power_meter(const char*sn)
{
  const char*label = "power_meter";
  cout << log(label) <<"Testing powermeter." << endl;
  std::string port = util::find_port(sn);
  if (port.size() == 0)
  {
    cout << log(label)<< "Failed to find the port" << endl;
    util::enumerate_ports();
  }
  else
  {
    cout << log(label)<< "Found port " << port << endl;
  }

  device::PowerMeter *m_pm = nullptr;
  uint32_t baud_rate = 9600;
  // now do the real test
  try
  {
    m_pm = new device::PowerMeter(port.c_str(),baud_rate);

    // -- this sets the cache variables so that we can get a whole ton of stuff out of it
    cout << log(label)<< "Querying head information" << endl;
    std::string tp, sn,name;
    bool pw,en,fq;
    m_pm->head_info(tp,sn,name,pw,en,fq);
    cout <<log(label) << "HEAD INFO :\n"
        << "Type           : " << tp << "\n"
        << "Serial number  : " << sn << "\n"
        << "Name           : " << name << "\n"
        << "Power capable  : " << pw << "\n"
        << "Energy capable  : " << en << "\n"
        << "Frequency capable  : " << fq << "\n" << endl;

    // now do the same for the instrument itself
    cout << log(label) << "Now querying the instrument" << endl;

    m_pm->inst_info(tp, sn, name);
    cout <<log(label) << "INSTRUMENT INFO :\n"
        << "Id             : " << tp << "\n"
        << "Serial number  : " << sn << "\n"
        << "Name           : " << name << "\n" << endl;


    // ranges
    cout << log(label)<< "Querying ranges..." << endl;
    int16_t cur;
    std::map<int16_t,std::string> rmap;
    m_pm->get_all_ranges(cur);
    m_pm->get_range_map(rmap);
    cout << log(label)<< "Current range [" << cur << "]" << endl;
    cout <<log(label) << "RANGES :" << endl;
    for (auto item : rmap)
    {
      cout << item.first<< " : [" << item.second << "]" << endl;
    }

    // now query the user threshold
    uint16_t current,min,max;
    cout << log(label) << "Checking user threshold" << endl;
    m_pm->query_user_threshold(current, min, max);
    cout <<log(label) <<  "Response : " << current << " : [" << min << "," << max << "]"<<endl;

    // get pulse widths
    std::map<uint16_t,std::string> umap;
    cout<< log(label) << "Checking pulse options" << endl;
    m_pm->pulse_length(0, current);
    m_pm->get_pulse_map(umap);
    cout << log(label)<< "Current pulse width [" << current << "]" << endl;
    cout <<log(label) << "OPTIONS :" << endl;
    for (auto item : umap)
    {
      cout << item.first<< " : [" << item.second << "]" << endl;
    }

    // get wavelengths
    cout<< log(label) << "Querying wavelength capabilities (method 1)" << endl;
    m_pm->get_all_wavelengths(name);
    cout << log(label) << "Raw answer [" << name << "]" << endl;

    // Not implemented
    //     cout<< log(label) << "Querying wavelength capabilities (method 2)" << endl;
    //     std::string raw_answer;
    //     m_pm->get_all_wavelengths(raw_answer);
    //     cout << log(label) << "Current selection : " << cur << endl;


    // averaging settings
    cout << log(label) << "Querying averaging settings" << endl;
    m_pm->average_query(0, current);
    // -- this actually also fills the map on the first execution
    m_pm->get_averages_map(umap);
    cout << log(label)<< "Current average setting [" << current << "]" << endl;
    cout <<log(label) << "OPTIONS :" << endl;
    for (auto item : umap)
    {
      cout << item.first<< " : [" << item.second << "]" << endl;
    }

    // max frequency
    cout << log(label) << "Querying for max frequency" << endl;
    uint32_t u32;
    m_pm->max_freq(u32);
    cout << log(label) << "Answer : " << u32 << endl;


    cout << log(label)<< "All done. Can't test anything else without actually changing settings.\n\n\n\n" << endl;
  }
  catch(serial::PortNotOpenedException &e)
  {
    std::cout << log(label)<< "Port not open exception : " << e.what() << endl;
  }
  catch(serial::SerialException &e)
  {
    std::cout << log(label)<< "Serial exception : " << e.what() << endl;
  }
  catch(std::exception &e)
  {
    std::cout << log(label)<< "STL exception : " << e.what() << endl;
  }
  catch(...)
  {
    cout << log(label)<< "test_laser : Caught an unexpected exception" << endl;
  }
  // try to leave cleanly
  if (m_pm) delete m_pm;
}


int main(int argc, char**argv)
{
  // this test simply tries to figure out the port of each device
  // connect to it and query something

  const std::string laser_sn = "A9J0G3SV";
  //test_laser(laser_sn.c_str());

  const std::string attenuator_sn = "6ATT1788D";
  test_attenuator(attenuator_sn.c_str());

  const std::string pm_sn = "A9CQTZ05";
  test_power_meter(pm_sn.c_str());

  return 0;
}
