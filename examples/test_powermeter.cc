/*
 * test_laser.cc
 *
 *  Created on: Jun 2, 2023
 *      Author: nbarros
 */

// -- this test is virtually the same as the test_laser method in test_all_devices
#include <utilities.hh>

#include <PowerMeter.hh>

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


void test_powermeter(uint32_t timeout, std::string read_suffix, std::string write_suffix)
{
  const char *label = "power:: ";
  cout << log(label) << "Runnig the tests with read suffix [" << util::escape(read_suffix.c_str()) << "]"
      << ", write suffix [" <<     util::escape(write_suffix.c_str())  << "] "
      << " and timeout [" << timeout << "] ms" << endl;


  const char*sn = "A9JR8MJT";
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

    // -- up until now it was just the default connection
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (read_suffix.size() != 0)
    {
      cout << log(label)<< "Setting the read suffix..." << endl;
      m_pm->set_read_suffix(read_suffix);
    }
    if (write_suffix.size() != 0)
    {
      cout << log(label)<< "Setting the write suffix..." << endl;
      m_pm->set_com_suffix(write_suffix);
    }
    if (timeout != 0)
    {
      cout << log(label)<< "Setting timeout..." << endl;
      m_pm->set_timeout_ms(timeout);

      // since we have a timeout, try to read whatever is already in the buffers
      // DON'T DO THIS WITHOUT A TIMEOUT or you'll hang the system
      cout << log(label) << "Attempting to read fragments from past eras ...this may take a bit!" << endl;
      std::vector<std::string> lines;
      m_pm->read_lines(lines);
      cout << log(label) << "Got " << lines.size() << "tokens. Dumping..." << endl;
      for (auto t: lines)
      {
        cout << log(label) << "[" << util::escape(t.c_str()) << "]" << endl;
      }
    }

    uint16_t qstate;
    m_pm->average_query(0,qstate);
    cout << log(label) << "Average query state : " << qstate << endl;

    std::string wls;
    m_pm->get_all_wavelengths(wls);
        std::cout << log(label) << ": WL : [" << wls << "]" << std::endl;



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
      cout<< log(label) << "Checking pulse width options" << endl;
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


      cout << "==========================================================" << endl;
      cout << "Dumping the collected maps :\n" << endl;
      std::map<int16_t,std::string> ranges;
      m_pm->get_range_map(ranges);
      cout << "RANGE : " << util::serialize_map(ranges) << endl;

      std::map<uint16_t,std::string> pulses;
      m_pm->get_pulse_map(pulses);
      cout << "PULSE WIDTHS : " << util::serialize_map(pulses) << endl;

      m_pm->get_averages_map(pulses);
      cout << "AVERAGES : " << util::serialize_map(pulses) << endl;

      std::pair<uint16_t, uint16_t> th = m_pm->get_threshold_ranges();
      cout << "TRESHOLDS : [ " << th.first << "," << th.second << "]" << endl;


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
    if (m_pm)
    {
      m_pm->close();
      delete m_pm;
    }

}

int main(int argc, char**argv)
{
  // this test simply tries to figure out the port of each device
  // connect to it and query something


//  cout <<"\n\n Testing with a 500 ms timeout and (\\n\\r,\\n\\r)  termination\n\n" << endl;
//  test_powermeter(500,"\n\r","\n\r");
//
//  cout <<"\n\n Testing with a 500 ms timeout and (\\n\\r,\\r\\n)  termination\n\n" << endl;
//  test_powermeter(500,"\n\r","\r\n");

  cout <<"\n\n Testing with a 100 ms timeout and (\\r\\n,\\r\\n)  termination\n\n" << endl;
  test_powermeter(100,"\r\n","\r\n");
//
//  cout <<"\n\n Testing with a 500 ms timeout and (\\r\\n,\\n\\r)  termination\n\n" << endl;
//  test_powermeter(500,"\r\n","\n\r");


  return 0;
}


