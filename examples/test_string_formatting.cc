/*
 * test_string_formatting.cc
 *
 *  Created on: May 31, 2023
 *      Author: nbarros
 */

#include <Laser.hh>
#include <PowerMeter.hh>
#include <Attenuator.hh>
#include <utilities.hh>

#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;
/*
void test_laser(const std::string port)
{

  std::string str;
  try
  {
    device::Laser *m_dev = new device::Laser(port.c_str(),9600);
    m_dev->set_timeout(1000);

    // prefix "" suffix:"\n"
    cout << "Testing prefix [] suffix [\\n]" << endl;
    m_dev->set_com_prefix("");
    m_dev->set_com_suffix("\n");
    // -- set a large timeout
    m_dev->security(str);
    cout << "Received answer [" << str << "]" << endl;
  }
  catch(serial::SerialException &e)
  {
    cout << "Caught serial exception " << e.what() << endl;
  }
  catch(std::exception &e)
  {
    cout << "Caught STL exception " << e.what() << endl;
  }
  catch(...)
  {
    cout << "Caught unknown exception " << endl;
  }
}
*/
int main(int argc, char**argv)
{

  std::string str = "o0;4000\n\r";
  std::string escaped = util::escape(str.c_str(), {'"','\n','\t','\r'}, '\\');

  cout <<"Original string :[" << str << "]" << endl;
  cout <<"Escaped string : [" << escaped << "]" << endl;

  str = str.substr(1); // drop the leading echo
  cout << "[" << str << "]" << endl;
  str.erase(str.size()-2); // drpo the trailing chars
  cout << "[" << str << "]" << endl;
  std::vector<std::string> tokens;

  util::tokenize_string(str,tokens);

  for (auto e : tokens)
  {
    cout << "[" << e << "]" << endl;
    cout << "[" << std::stoul(e) << "]" << endl;
  }


  return 0;

  if (argc != 3)
  {
    cout << "Usage: " << argv[0] << " <device type> <device port> \n\n"
        << "<device type> is one of \n"
        << "0 : laser\n1 : attenuator\n2 : power meter" << endl;
    return 0;
  }

  uint32_t dev = std::stoul(string(argv[1]));
  std::string port(argv[2]);

  device::Device *m_dev;
  switch(dev)
  {
    case 0: // laser
      cout << "Running the test on the laser" << endl;
      //test_laser(port);

      break;
    case 1: // attenuator
      cout << "Running the test on the attenuator" << endl;
      m_dev = new device::Attenuator(port.c_str(),38400);
      break;
    case 2:
      cout << "Running the test on the attenuator" << endl;
      m_dev = new device::PowerMeter(port.c_str(),9600);
      break;
    default:
      cout << "Unknown device type" << endl;
      return 0;
  }




  return 0;
}
