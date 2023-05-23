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
  return "";
}

}

