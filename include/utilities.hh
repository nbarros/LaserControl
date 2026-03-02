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
#include <cstdint>
#include <exception>
#include <stdexcept>

namespace util {

const std::string WHITESPACE = " \n\r\t\f\v";

void tokenize_string(std::string &str, std::vector<std::string> &tokens, std::string sep = ";");

std::string ltrim(const std::string &s);

std::string rtrim(const std::string &s);

std::string trim(const std::string &s);

std::string escape(const char* src);
std::string escape(const std::string src);

std::string escape(const char* src, const std::set<char> escapee, const char marker);

int char2int(const char c);

void enumerate_ports();

std::string find_port(std::string param);

[[noreturn]] inline void throw_parse_error(const std::string& method, const std::string& detail)
{
	throw std::runtime_error(method + " parse error: " + detail);
}

[[noreturn]] inline void throw_parse_error(const std::string& method, const std::exception& ex)
{
	throw_parse_error(method, ex.what());
}


template <typename T>
std::string serial_map(const std::map<T,std::string> m);


std::string serialize_map(const std::map<uint16_t,std::string> m);

std::string serialize_map(const std::map<int16_t,std::string> m);

}

#endif /* UTILITIES_HH_ */
