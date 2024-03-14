#include "read_request_from_client.hpp"

#include "cgi_handler.hpp"

ReadRequestFromClient::ReadRequestFromClient(int fd, const std::string &port,
                                             const std::string &ip,
                                             const IConfig &config)
    : AIOTask(fd, POLLIN),
      port_(port),
      ip_(ip),
      config_(config),
      parser_(HTTPRequestParser()) {}

ReadRequestFromClient::~ReadRequestFromClient() {}

Result<int, std::string> ReadRequestFromClient::Execute() {
  char buf[buf_size_];
  int len = read(fd_, buf, buf_size_);
  if (len == -1) {
    Logger::Error() << "read エラー" << std::endl;
    return Err("read error");
  }
  if (len == 0) return Ok(kReadDelete);
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
  } else {
    Logger::Info() << port_ << " : "
                   << "リクエストをパースしました : " << buf << len
                   << std::endl;
    if (result.Unwrap()->GetMethod() == "POST") {
      CgiHandler cgi_handler;
      cgi_handler.Handle(result.Unwrap(), fd_, config_, port_, ip_);
      delete result.Unwrap();
    } else {
      HTTPResponse *response = new HTTPResponse();
      response->SetHTTPVersion("HTTP/1.1");
      response->SetStatusCode(http::kOk);
      response->AddHeader("Content-Type", "text/html");
      response->SetBody(
          "<html><head><title>サーバーテスト</title><meta "
          "http-equiv=\"content-type\" charset=\"utf-8\"></head><body><form "
          "action=\"/cgi-bin/cgi_test.py\" method=\"POST\"><div><label "
          "for=\"name\">好きな食べ物</label><input type=\"text\" name=\"food\" "
          "value=\"りんご\"><label for=\"season\">好きな季節</label><input "
          "type=\"text\" name=\"season\" "
          "value=\"冬\"><button>送信</button></div> </form></body></html>");
      // RequestHandler::Handle(config_, result.Unwrap(), port_, ip_);
      IOTaskManager::AddTask(new WriteResponseToClient(fd_, response));
      delete result.Unwrap();
    }
  }
  Logger::Info() << port_ << " : "
                 << "レスポンスのタスクを追加しました" << std::endl;
  return Ok(0);
}

const std::string &ReadRequestFromClient::GetPort() const { return port_; }
const std::string &ReadRequestFromClient::GetIp() const { return ip_; }
