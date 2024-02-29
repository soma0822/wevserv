#ifndef WEBSERV_SRC_CONFIG_LOCATION_PARSER_HPP
#define WEBSERV_SRC_CONFIG_LOCATION_PARSER_HPP

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "location_context.hpp"
#include "merge_container.hpp"
#include "result.hpp"
#include "string.hpp"
#include "validation.hpp"

class LocationParser {
 public:
  typedef bool (*parseFunction)(const std::vector<std::string> &,
                                LocationContext &);
  static LocationContext ParseLocation(std::ifstream &);

 private:
  static void ParseFuncInit(std::map<std::string, parseFunction> &);
  static void RemoveSemicolon(std::string &line);
  static bool ParseAutoIndex(const std::vector<std::string> &,
                             LocationContext &);
  static bool ParseLimitClientBody(const std::vector<std::string> &,
                                   LocationContext &);
  static bool ParseReturn(const std::vector<std::string> &, LocationContext &);
  static bool ParseAlias(const std::vector<std::string> &, LocationContext &);
  static bool ParseRoot(const std::vector<std::string> &, LocationContext &);
  static bool ParseIndex(const std::vector<std::string> &, LocationContext &);
  static bool ParseCgiPath(const std::vector<std::string> &, LocationContext &);
  static bool ParseCgiExtention(const std::vector<std::string> &,
                                LocationContext &);
  static bool ParseAllowMethod(const std::vector<std::string> &,
                               LocationContext &);
  static bool ParseErrorPage(const std::vector<std::string> &,
                             LocationContext &);
};

#endif
