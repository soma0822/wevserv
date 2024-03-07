#include "server.hpp"

Server::Server() {}

Server::~Server() {}

Server &Server::operator=(const Server &other) {
  (void)other;
  return *this;
}

bool Server::Run(const IConfig &config) {
  const std::vector<ServerContext> &servers = config.GetServer();
  std::vector<ServerContext>::const_iterator server_it = servers.begin();
  std::map<std::string, std::set<std::string> > listen_port;
  for (; server_it != servers.end(); ++server_it) {
    if (listen_port[server_it->GetPort()].insert(server_it->GetIp()).second) {
      Result<int, int> result =
          Listen(server_it->GetPort(), server_it->GetIp());
      if (result.IsOk()) {
        IOTaskManager::AddTask(new Accept(result.Unwrap(), server_it->GetPort(),
        server_it->GetIp(), config));
      } else {
        Logger::Error() << "リッスンに失敗しました" << std::endl;
        return false;
      }
    }
  }
  // IOTaskManager::ExecuteTasks();
  return true;
}

// bind, listenのエラーハンドリングするためにAcceptのコンストラクタから移行する
Result<int, int> Server::Listen(const std::string &port,
                                const std::string &ip) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  Result<int, std::string> result = string_utils::StrToI(port);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(result.Unwrap());
  // TODO:
  // inet_addrは使用可能ではないため、自作の必要あり　空文字列の場合INADDR_ANYを返してくる
  addr.sin_addr.s_addr = inet_addr(ip.c_str());

  // TODO：errornoを見て処理を変える
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    return Err(kBindError);
  if (listen(sock, SOMAXCONN) == -1) return Err(kListenError);
  Logger::Info() << port << " : リッスン開始" << std::endl;
  return Ok(sock);
}
