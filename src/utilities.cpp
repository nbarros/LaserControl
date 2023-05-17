/*
 * utilities.cpp
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#include <utilities.hh>

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

}

