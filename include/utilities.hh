/*
 * utilities.hh
 *
 *  Created on: May 17, 2023
 *      Author: nbarros
 */

#ifndef UTILITIES_HH_
#define UTILITIES_HH_

#include <string>
#include <vector>

namespace util {

void tokenize_string(std::string &str, std::vector<std::string> &tokens, std::string sep = ";");

int char2int(const char c);

void enumerate_ports();

// FIXME: Implement this
std::string find_port(std::string param);

}

#endif /* UTILITIES_HH_ */
