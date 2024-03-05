#include <iostream>

#include "config.hpp"
#include "config_parser.hpp"
#include "logger.hpp"

int main(int ac, char **av) {
  std::string config_file;
  if (ac == 1)
    config_file = ConfigParser::default_file_;
  else if (ac == 2)
    config_file = av[1];
  else {
    Logger::Error() << "Invalid argument." << std::endl;
    return 0;
  }
  Logger::Info() << "Reading " << config_file << std::endl;
  try {
    ConfigParser::Parse(config_file);
    std::vector<ServerContext> m = Config::GetServer();
    for (std::vector<ServerContext>::const_iterator it = m.begin();
         it != m.end(); ++it) {
      std::cout << *it << std::endl;
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
