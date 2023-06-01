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
#include <map>
#include <set>

namespace util {

void tokenize_string(std::string &str, std::vector<std::string> &tokens, std::string sep = ";");

std::string escape(const char* src);

std::string escape(const char* src, const std::set<char> escapee, const char marker);

int char2int(const char c);

void enumerate_ports();

std::string find_port(std::string param);


template <typename T>
std::string serial_map(const std::map<T,std::string> m);


std::string serialize_map(const std::map<uint16_t,std::string> m);

std::string serialize_map(const std::map<int16_t,std::string> m);

}

#endif /* UTILITIES_HH_ */
