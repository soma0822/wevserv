#include "server.hpp"

Server::Server() {}

Server::~Server() {}

Server &Server::operator=(const Server &other) {
  (void)other;
  return *this;
}

void Server::run() {
  std::vector<ServerContext> servers = Config::GetServer();
  std::vector<ServerContext>::iterator server_it = servers.begin();
  std::map<std::string, bool> listen_port;
  for (; server_it != servers.end(); ++server_it) {
    std::vector<std::string> ports = server_it->GetPort();
    std::vector<std::string>::iterator port_it = ports.begin();
    for (; port_it != ports.end(); ++port_it) {
      if (listen_port[*port_it] == false) {
        Result<int, int> result = Listen(*port_it, server_it->GetIp());
        if (result.IsOk()) {
          // IOTaskManager::AddTask(new Accept(result.Unwrap(), *port_it, server_it->GetIp()));
          listen_port[*port_it] = true;
        } else {
          Logger::Error() << "リッスンに失敗しました" << std::endl;
        }
      }
    }
  }
  // IOTaskManager::ExecuteTasks();
}

// bind, listenのエラーハンドリングするためにAcceptのコンストラクタから移行する
Result<int, int> Server::Listen(const std::string &port, const std::string &ip) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  Result<int, std::string> result = string_utils::StrToI(port);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(result.Unwrap());
  //TODO: inet_addrは使用可能ではないため、自作の必要あり　空文字列の場合INADDR_ANYを返してくる
  addr.sin_addr.s_addr = inet_addr(ip.c_str());

  // TODO：errornoを見て処理を変える
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    return Err(kBindError);
  if (listen(sock, SOMAXCONN) == -1) return Err(kListenError);
  Logger::Info() << port << " : リッスン開始" << std::endl;
  return Ok(sock);
}
