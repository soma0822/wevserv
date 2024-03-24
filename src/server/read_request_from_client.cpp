#include "read_request_from_client.hpp"

ReadRequestFromClient::ReadRequestFromClient(int fd, const std::string &port,
                                             const std::string &ip,
                                             struct sockaddr_in client_addr,
                                             const IConfig &config)
    : AIOTask(fd, POLLIN),
      port_(port),
      ip_(ip),
      client_addr_(client_addr),
      config_(config),
      parser_(HTTPRequestParser()) {}

ReadRequestFromClient::~ReadRequestFromClient() {}

Result<int, std::string> ReadRequestFromClient::Execute() {
  char buf[buf_size_ + 1];
  int len = read(fd_, buf, buf_size_);
  if (len == -1) {
    Logger::Error() << "read エラー" << std::endl;
    return Ok(kFdDelete);
  }
  if (len == 0) return Ok(kFdDelete);
  buf[len] = '\0';
  Result<HTTPRequest *, int> result = parser_.Parser(buf);
  if (result.IsErr() && result.UnwrapErr() == HTTPRequestParser::kNotEnough) {
    return Ok(0);
    Logger::Info() << port_ << " : "
                   << "リクエストをパース中です : " << buf << len << std::endl;
  } else if (result.IsErr() &&
             result.UnwrapErr() == HTTPRequestParser::kBadRequest) {
    std::cout << "bad request" << std::endl;
    // TODO: badrequestの処理
    //  IOTaskManager::AddTask(new WriteResponseToClient write_response(fd_,
    //  badrequest_response));
  } else if (result.IsErr() &&
             result.UnwrapErr() ==
                 HTTPRequestParser::kHttpVersionNotSupported) {
    std::cout << "HttpVersion Not Supported" << std::endl;
    // TODO: HttpVersionNotSupportedの処理
  } else {
    Logger::Info() << port_ << " : "
                   << "リクエストをパースしました : " << buf << len
                   << std::endl;
    RequestContext req_ctx = {result.Unwrap(), port_, ip_,
                              client_addr_,    fd_,   0};
    HTTPResponse *response = RequestHandler::Handle(config_, req_ctx);
    IOTaskManager::AddTask(
        new WriteResponseToClient(fd_, response, result.Unwrap()));
    // }
  }
  Logger::Info() << port_ << " : "
                 << "レスポンスのタスクを追加しました" << std::endl;
  return Ok(0);
}

const std::string &ReadRequestFromClient::GetPort() const { return port_; }
const std::string &ReadRequestFromClient::GetIp() const { return ip_; }
