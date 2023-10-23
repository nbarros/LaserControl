/*
 * test_laser.cc
 *
 *  Created on: Jun 2, 2023
 *      Author: nbarros
 */

// -- this test is virtually the same as the test_laser method in test_all_devices
#include <utilities.hh>

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


void test_laser(uint32_t timeout, std::string read_suffix)
{
  const char *label = "laser:: ";
  cout << log(label) << "Runnig the tests with read suffix [" << util::escape(read_suffix.c_str()) << "]"
                    << " and timeout [" << timeout << "] ms" << endl;


  const std::string laser_sn = "FTC7LJXH";
  std::string port = util::find_port(laser_sn.c_str());
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

    // -- up until now it was just the default connection
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (read_suffix.size() != 0)
    {
      cout << log(label)<< "Setting the read suffix..." << endl;
      m_laser->set_read_suffix(read_suffix);
    }
    if (timeout != 0)
    {
      cout << log(label)<< "Setting timeout..." << endl;
      m_laser->set_timeout_ms(timeout);

      // since we have a timeout, try to read whatever is already in the buffers
      // DON'T DO THIS WITHOUT A TIMEOUT or you'll hang the system
      cout << log(label) << "Attempting to read fragments from past eras ...this may take a bit!" << endl;
      std::vector<std::string> lines;
      m_laser->read_lines(lines);
      cout << log(label) << "Got " << lines.size() << "tokens. Dumping..." << endl;
      for (auto t: lines)
      {
        cout << log(label) << "[" << util::escape(t.c_str()) << "]" << endl;
      }
    }
    // now start the real work
    // -- at this point there is nothing
    cout << log(label)<< "Checking interlocks..." << endl;
    std::string code;
    std::string desc;
    m_laser->security(code);
    cout << log(label)<< "Laser responded [" << code << "]" << endl;

    //

    cout << log(label)<< "Get shot count..." << endl;
    uint32_t count;
    m_laser->get_shot_count(count);
    cout << log(label)<< "Laser responded [" << count << "]" << endl;

    // open and close the shutter for 500 ms
    cout << log(label)<< "Opening the shutter for 500ms..." << endl;
    m_laser->shutter_open();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    m_laser->shutter_close();
    cout << log(label)<< "All done." << endl;
  }
  catch(serial::PortNotOpenedException &e)
  {
    std::cout << log(label)<< "Port not open exception : " << e.what() << endl;
  }
  catch(serial::SerialException &e)
  {
    std::cout << log(label)<< "Serial exception : " << e.what() << endl;
  }
  catch(std::range_error &e)
  {
    std::cout << log(label) << "Range error : " << e.what() << endl;
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
  if (m_laser) {
    m_laser->close();
    delete m_laser;
  }
}

int main(int argc, char**argv)
{
  // this test simply tries to figure out the port of each device
  // connect to it and query something

  // to test out whether there is some trouble brewing

  ///dev/ttyUSB2, FTDI USB Serial Converter FTC7LJXH, USB VID:PID=0403:6001 SNR=FTC7LJXH

  cout <<"\n\n Testing with a 200 ms timeout and \\n read termination\n\n" << endl;
  test_laser(200,"\n");

  cout <<"\n\n Testing with NO timeout and \\n read termination\n\n" << endl;
  test_laser(0,"\n");

  cout <<"\n\n Testing with a 200 ms timeout and \\r read termination\n\n" << endl;
  test_laser(200,"\r");

  cout <<"\n\n Testing with a 200 ms timeout and \\r\\n read termination\n\n" << endl;
  test_laser(200,"\r\n");

  cout <<"\n\n Testing with NO timeout and \\r read termination\n\n" << endl;
  test_laser(0,"\r");


  return 0;
}


