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

void test_pm1()
{
  std::string answer="* PY 967321 PE50BF-DFH-C 80608003 \r\n";
  answer.erase(answer.size()-2);
  std::vector<std::string> tokens;
  util::tokenize_string(answer, tokens, " ");
  tokens.erase(tokens.begin());
  for (auto t:tokens)
  {
    cout << t << endl;
  }
  std::string type = tokens.at(0);
  std::string sn = tokens.at(1);
  std::string name = tokens.at(2);
  uint32_t word = std::stoul(tokens.at(3),0, 16);

  bool power = word & (1 << 0);
    // energy is bit 1
  bool energy = word & (1 << 0);
    // frequency is bit 31
  bool freq = word & (1 << 31);

}

int main(int argc, char**argv)
{

  test_pm1();

//  std::string stat="pc1;0;0;0;59000;114;36;114;2;1;0;0;0;0;0;0;1;1;0;0;0;0;0;0;\n\r";
//  std::string escaped = util::escape(stat.c_str(), {'"','\n','\t','\r'}, '\\');
//  stat.erase(stat.size()-2);
//  stat = stat.substr(2);
//
//  cout << "Raw [" << stat << "]" << endl;
//  cout << "Escaped [" << util::escape(stat.c_str()) << "]" << endl;
//
//  std::vector<std::string> tokens;
//  util::tokenize_string(stat, tokens);
//
//  int arg1 = std::stol(tokens.at(0));
//  int arg2 = std::stol(tokens.at(1));
//
//  for (auto e : tokens)
//  {
//    cout << "[" << e << "]" << endl;
//  }
//
//  uint16_t arg3 = std::stoul(tokens.at(2)) & 0xFF;
//  uint16_t arg4 = std::stoul(tokens.at(3)) & 0xFF;
//  uint16_t arg5 = std::stoul(tokens.at(4));
//
//  uint16_t arg6 = std::stoul(tokens.at(5)) & 0xFF;
//  uint16_t arg7 = std::stoul(tokens.at(6)) & 0xFF;
//
//  uint16_t arg9 = std::stoul(tokens.at(8)) & 0xFF;
//  uint16_t arg10 = std::stoul(tokens.at(9)) & 0xFF;
//
//  uint16_t arg12 = std::stoul(tokens.at(11)) & 0xFF;
//  uint16_t arg13 = std::stoul(tokens.at(12)) & 0xFF;
//
//
//  std::string str = "o0;4000\n\r";
//  std::string escaped = util::escape(str.c_str(), {'"','\n','\t','\r'}, '\\');
//
//  cout <<"Original string :[" << str << "]" << endl;
//  cout <<"Escaped string : [" << escaped << "]" << endl;
//
//  str = str.substr(1); // drop the leading echo
//  cout << "[" << str << "]" << endl;
//  str.erase(str.size()-2); // drpo the trailing chars
//  cout << "[" << str << "]" << endl;
//  std::vector<std::string> tokens;
//
//  util::tokenize_string(str,tokens);



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
