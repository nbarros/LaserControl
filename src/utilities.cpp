/*
 * utilities.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <utilities.hh>
#include <iostream>
#include <serial/serial.h>

using std::vector;
using std::string;
using std::cout;
using std::endl;

namespace util {
void tokenize_string(std::string &str, std::vector<std::string> &tokens, std::string sep)
{
  size_t pos = 0;
  std::string token;
  while ((pos = str.find(sep)) != std::string::npos)
  {
    tokens.push_back(str.substr(0, pos));
    str.erase(0, pos + sep.length());
  }
}

int char2int(const char c)
{
  // WARNING: There is no range check. If the char is not a digit, this will give the wrong answer
  // see https://sentry.io/answers/char-to-int-in-c-and-cpp/
  return  (c - '0');

}

void enumerate_ports()
{
  vector<serial::PortInfo> devices_found = serial::list_ports();
  cout << "enumerate_ports : Number of devices found : " << devices_found.size() << endl;
  vector<serial::PortInfo>::iterator iter = devices_found.begin();

  while( iter != devices_found.end() )
  {
    serial::PortInfo device = *iter++;
    printf( "(%s, %s, %s)\n", device.port.c_str(), device.description.c_str(),
     device.hardware_id.c_str() );
  }
}


std::string find_port(std::string param)
{
  cout << "find_port : This method is not implemented yet" << endl;

  // loop over all available ports and search if any of the fields matches the string
  // that was passed
  // if more than one port is found to match, return the best match
  vector<serial::PortInfo> devices_found = serial::list_ports();
#ifdef DEBUG
  cout << "find_port : Number of devices found : " << devices_found.size() << endl;
#endif
  vector<serial::PortInfo>::iterator iter = devices_found.begin();

  vector<serial::PortInfo> devices_match;
  while( iter != devices_found.end() )
  {
    serial::PortInfo device = *iter++;
    if (device.description.find(param) != device.description.npos)
    {
      devices_match.push_back(device);
    }
    else if (device.port.find(param) != device.port.npos)
    {
      devices_match.push_back(device);
    }
    else if (device.hardware_id.find(param) != device.hardware_id.npos)
    {
      devices_match.push_back(device);
    }
  }

  // if we found more than one, just return the first found.
  // if we found none, return empty string
  if (devices_match.size() == 0)
  {
#ifdef DEBUG
    cout << "find_port : Couldn't find any devices matching description" << endl;
#endif
    throw std::runtime_error("Couldn't find any devices matching description");
  }
  else if (devices_match.size() == 1)
  {
#ifdef DEBUG
    cout << "find_port : Found device [" << devices_match.at(0).port << " , "
        << devices_match.at(0).description << " , " << devices_match.at(0).hardware_id << "]" << endl;
#endif
    return devices_match.at(0).port;
  }
  else // found multiple entries
  {
#ifdef DEBUG
    cout << "find_port : Found multiple matching devices: "<< endl;
    vector<serial::PortInfo>::iterator iter = devices_match.begin();
    vector<serial::PortInfo> devices_match;
    while( iter != devices_match.end() )
    {
    cout << " --> [" << devices_match.at(0).port << " , "
        << devices_match.at(0).description << " , " << devices_match.at(0).hardware_id << "]" << endl;
    }
    cout << "find_port : Returning first entry " << endl;
#endif
    return devices_match.at(0).port;
  }
  return "";
}

}

