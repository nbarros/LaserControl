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

// Safe substring extraction with bounds checking
inline std::string safe_substr(const std::string& str, size_t pos, size_t len, 
                               const std::string& context)
{
	if (str.size() < pos + len)
	{
		throw_parse_error(context, 
			std::string("substr(") + std::to_string(pos) + "," + std::to_string(len) + 
			") on string of size " + std::to_string(str.size()) + ": [" + escape(str) + "]");
	}
	return str.substr(pos, len);
}

// Safe character access with bounds checking
inline char safe_at(const std::string& str, size_t pos, const std::string& context)
{
	if (str.size() <= pos)
	{
		throw_parse_error(context, 
			std::string("at(") + std::to_string(pos) + 
			") on string of size " + std::to_string(str.size()) + ": [" + escape(str) + "]");
	}
	return str.at(pos);
}

// Validate token count before access
inline void validate_token_count(const std::vector<std::string>& tokens, size_t required_count,
                                 const std::string& context, const std::string& response = "")
{
	if (tokens.size() < required_count)
	{
		std::string msg = std::string("Expected at least ") + std::to_string(required_count) + 
			" tokens, got " + std::to_string(tokens.size());
		if (!response.empty())
		{
			msg += ". Response: [" + escape(response) + "]";
		}
		throw_parse_error(context, msg);
	}
}

template <typename T>
std::string serial_map(const std::map<T,std::string> m);


std::string serialize_map(const std::map<uint16_t,std::string> m);

std::string serialize_map(const std::map<int16_t,std::string> m);

}

#endif /* UTILITIES_HH_ */
