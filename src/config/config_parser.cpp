#include "config_parser.hpp"

const std::string ConfigParser::default_file_ = "./conf/default.conf";

void ConfigParser::Parse(const std::string &file) {
  Config::Clear();
  ServerParser::ClearParsedPair();
  std::vector<ServerContext> server;
  std::string line;
  std::ifstream inf(file.c_str());
  if (!inf.is_open()) {
    throw std::invalid_argument("ファイルが開けませんでした: " + file);
  }
  while (std::getline(inf, line)) {
    std::stringstream ss(line);
    std::string key;
    std::string value;
    ss >> key;
    if (key.empty()) continue;
    ss >> value;
    if (key + value == "server{" && ss.get() == std::char_traits<char>::eof())
      Config::AddServer(ServerParser::ParseServer(inf));
    else {
      throw std::invalid_argument("無効なキー: " + line);
    }
  }
}
