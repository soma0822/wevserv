#include "config.hpp"

Config::Config():file_(DEFAULTCONF) {}

Config::Config(const std::string &file):file_(file) {}

Config::Config(const Config &other):file_(other.file_), server_(other.server_){}

Config::~Config(){}

Config &Config::operator=(const Config& other){
    if (this == &other) {
      return *this;
    }
    file_ = other.file_;
	server_ = other.server_;
	return *this;
}

void Config::parse_file(){
	server_ = ConfigParser::parse(file_);
}