#include "request_handler.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.hpp"
#include "file_utils.hpp"

Option<HTTPResponse *> RequestHandler::Handle(const IConfig &config,
                                              RequestContext req_ctx) {
  HTTPRequest *request = req_ctx.request;
  if (!request) {
    return Some(HTTPResponse::Builder()
                    .SetStatusCode(http::kInternalServerError)
                    .Build());
  }
  if (request->GetUri().length() >= kMaxUriLength) {
    return Some(GenerateErrorResponse(http::kUriTooLong, config));
  }
  if (request->GetMethod() == "GET") {
    return Get(config, req_ctx);
  }
  if (request->GetMethod() == "POST") {
    return Post(config, req_ctx);
  }
  if (request->GetMethod() == "DELETE") {
    return Delete(config, req_ctx);
  }
  return Some(GenerateErrorResponse(http::kNotImplemented, config));
}

Option<HTTPResponse *> RequestHandler::Get(const IConfig &config,
                                           RequestContext req_ctx) {
  const HTTPRequest *request = req_ctx.request;
  const IServerContext &server_ctx =
      config.SearchServer(req_ctx.port, req_ctx.ip, request->GetHostHeader());
  const Result<LocationContext, std::string> location_ctx_result =
      server_ctx.SearchLocation(request->GetUri());

  bool need_autoindex = false;

  std::string request_file_path = ResolveRequestTargetPath(config, req_ctx);
  if (file_utils::IsDirectory(request_file_path)) {
    // リクエストターゲットのディレクトリが/で終わっていない場合には301を返す
    if (request_file_path.at(request_file_path.size() - 1) != '/') {
      return Some(HTTPResponse::Builder()
                      .SetStatusCode(http::kMovedPermanently)
                      .AddHeader("Location", request->GetUri() + "/")
                      .Build());
    }
    // AutoIndexが有効な場合にはディレクトリの中身を表示する
    if (location_ctx_result.IsOk() &&
        location_ctx_result.Unwrap().GetCanAutoIndex()) {
      // パーミッションを確認してからAutoIndexを表示する
      need_autoindex = true;
    }
  }

  if (need_autoindex) {
    struct stat file_stat;
    // ファイルが存在しない場合には404を返す
    if (stat(request_file_path.c_str(), &file_stat) == -1) {
      return Some(
          HTTPResponse::Builder().SetStatusCode(http::kNotFound).Build());
    }
    // パーミッションがない場合には403を返す
    if (!(file_stat.st_mode & S_IRUSR)) {
      return Some(
          HTTPResponse::Builder().SetStatusCode(http::kForbidden).Build());
    }
    return Some(GenerateAutoIndexPage(config, request, request_file_path));
  }

  // リクエストされたファイルのパスがディレクトリの場合には、indexファイルが存在する場合にはそれを返す
  if (file_utils::IsDirectory(request_file_path)) {
    if (location_ctx_result.IsOk() &&
        !location_ctx_result.Unwrap().GetIndex().empty()) {
      request_file_path += location_ctx_result.Unwrap().GetIndex();
    } else if (!server_ctx.GetIndex().empty()) {
      request_file_path += server_ctx.GetIndex();
    }
  }
  struct stat file_stat;
  // ファイルが存在しない場合には404を返す
  if (stat(request_file_path.c_str(), &file_stat) == -1) {
    return Some(HTTPResponse::Builder().SetStatusCode(http::kNotFound).Build());
  }
  // パーミッションがない場合には403を返す
  if (!(file_stat.st_mode & S_IRUSR)) {
    return Some(
        HTTPResponse::Builder().SetStatusCode(http::kForbidden).Build());
  }

  return Some(HTTPResponse::Builder()
                  .SetStatusCode(http::kOk)
                  .SetBody(file_utils::ReadFile(request_file_path))
                  .Build());
}

Option<HTTPResponse *> RequestHandler::Post(const IConfig &config,
                                            RequestContext req_ctx) {
  const HTTPRequest *request = req_ctx.request;
  const IServerContext &server_ctx =
      config.SearchServer(req_ctx.port, req_ctx.ip, request->GetHostHeader());
  const std::string &uri = request->GetUri();
  const Result<LocationContext, std::string> location_ctx_result =
      server_ctx.SearchLocation(uri);

  const std::string request_file_path =
      ResolveRequestTargetPath(config, req_ctx);
  // リクエストターゲットがディレクトリの場合には400を返す
  if (request_file_path.at(request_file_path.size() - 1) == '/') {
    return Some(
        HTTPResponse::Builder().SetStatusCode(http::kBadRequest).Build());
  }

  const std::string parent_dir =
      request_file_path.substr(0, request_file_path.find_last_of('/'));
  struct stat parent_dir_stat;
  // 親ディレクトリが存在しない場合には404を返す
  if (stat(parent_dir.c_str(), &parent_dir_stat) == -1) {
    return Some(HTTPResponse::Builder().SetStatusCode(http::kNotFound).Build());
  }
  // 親ディレクトリに書き込み権限がない場合には403を返す
  if (!(parent_dir_stat.st_mode & S_IWUSR)) {
    return Some(
        HTTPResponse::Builder().SetStatusCode(http::kForbidden).Build());
  }

  std::ofstream ofs(request_file_path.c_str());
  if (!ofs) {
    return Some(HTTPResponse::Builder()
                    .SetStatusCode(http::kInternalServerError)
                    .Build());
  }
  ofs << request->GetBody();
  ofs.close();

  return Some(HTTPResponse::Builder()
                  .SetStatusCode(http::kCreated)
                  .AddHeader("Location", uri)
                  .Build());
}

Option<HTTPResponse *> RequestHandler::Delete(const IConfig &config,
                                              RequestContext req_ctx) {
  const HTTPRequest *request = req_ctx.request;
  const IServerContext &server_ctx =
      config.SearchServer(req_ctx.port, req_ctx.ip, request->GetHostHeader());
  const std::string &uri = request->GetUri();
  const Result<LocationContext, std::string> location_ctx_result =
      server_ctx.SearchLocation(uri);

  const std::string request_file_path =
      ResolveRequestTargetPath(config, req_ctx);

  // リクエストターゲットがディレクトリの場合には400を返す
  if (uri.at(uri.size() - 1) == '/' ||
      file_utils::IsDirectory(request_file_path)) {
    return Some(
        HTTPResponse::Builder().SetStatusCode(http::kBadRequest).Build());
  }

  // ファイルが存在しない場合には404を返す
  struct stat file_stat;
  if (stat(request_file_path.c_str(), &file_stat) == -1) {
    return Some(HTTPResponse::Builder().SetStatusCode(http::kNotFound).Build());
  }

  // パーミッションがない場合には403を返す
  if (!(file_stat.st_mode & S_IWUSR) && !(file_stat.st_mode & S_IXUSR)) {
    return Some(
        HTTPResponse::Builder().SetStatusCode(http::kForbidden).Build());
  }

  // ファイルを削除する
  if (unlink(request_file_path.c_str()) == -1) {
    return Some(HTTPResponse::Builder()
                    .SetStatusCode(http::kInternalServerError)
                    .Build());
  }

  return Some(HTTPResponse::Builder().SetStatusCode(http::kOk).Build());
}

std::string RequestHandler::ResolveAbsoluteRootPath(const IConfig &config,
                                                    RequestContext req_ctx) {
  const HTTPRequest *request = req_ctx.request;
  const IServerContext &server_ctx =
      config.SearchServer(req_ctx.port, req_ctx.ip, request->GetHostHeader());
  const std::string &uri = request->GetUri();
  const Result<LocationContext, std::string> location_ctx_result =
      server_ctx.SearchLocation(uri);

  std::string root = server_ctx.GetRoot();
  if (location_ctx_result.IsOk() &&
      !location_ctx_result.Unwrap().GetRoot().empty()) {
    root = location_ctx_result.Unwrap().GetRoot();
  }

  return root;
}

std::string RequestHandler::ResolveRequestTargetPath(
    const IConfig &config, const RequestContext req_ctx) {
  // rootを取得する
  std::string root = ResolveAbsoluteRootPath(config, req_ctx);

  // RFC9112によれば、OPTIONSとCONNECT以外のリクエストはパスが以下の形式になる
  // origin-form = absolute-path [ "?" query ]
  // rootが/で終わっている場合には/が重複してしまうので削除する
  if (root.at(root.size() - 1) == '/') {
    root.erase(root.size() - 1, 1);
  }
  return root + req_ctx.request->GetUri();
}

std::string RequestHandler::GetAbsoluteCGIScriptPath(const IConfig &config,
                                                     RequestContext req_ctx) {
  std::string request_file_path = ResolveRequestTargetPath(config, req_ctx);
  // 拡張子以降のパスセグメントは除外する
  size_t pos_period = request_file_path.find('.');
  size_t pos_separator = request_file_path.substr(pos_period).find('/');
  // '/'が見つからない場合には拡張子以降のパスセグメントがないのでそのまま返す
  if (pos_separator == std::string::npos) {
    return request_file_path;
  }
  return request_file_path.substr(0, pos_period + pos_separator);
}

// 存在しない場合は空文字を返す
std::string RequestHandler::GetAbsolutePathForPathSegment(
    const IConfig &config, RequestContext req_ctx) {
  std::string request_file_path = ResolveRequestTargetPath(config, req_ctx);

  // CGIスクリプトの絶対パス以降がパスセグメントになる
  std::string path_segment = request_file_path.substr(
      GetAbsoluteCGIScriptPath(config, req_ctx).size());
  if (path_segment.empty()) {
    return "";
  }
  return ResolveAbsoluteRootPath(config, req_ctx) + path_segment;
}

HTTPResponse *RequestHandler::GenerateAutoIndexPage(
    const IConfig &config, const HTTPRequest *request,
    const std::string &abs_path) {
  DIR *dir = opendir(abs_path.c_str());
  if (!dir) {
    return GenerateErrorResponse(http::kInternalServerError, config);
  }

  dirent *dp;
  std::string body =
      "<html><head><title>AutoIndex</title></head><body><h1>AutoIndex</h1><ul>";
  errno = 0;
  while ((dp = readdir(dir)) != NULL) {
    body += "<li><a href=\"" + request->GetUri() + "/" + dp->d_name + "\">" +
            dp->d_name + "</a></li>";
  }
  if (errno != 0) {
    closedir(dir);
    return GenerateErrorResponse(http::kInternalServerError, config);
  }
  body += "</ul></body></html>";
  closedir(dir);

  return HTTPResponse::Builder().SetStatusCode(http::kOk).SetBody(body).Build();
}
// ex.) script_name = /cgi-bin/default.py
// path_translated:パスセグメントの絶対パス
// TODO: 実行権限の確認
http::StatusCode RequestHandler::CGIExe(const IConfig &config,
                                        RequestContext req_ctx,
                                        const std::string &script_name,
                                        const std::string &path_translated) {
  // TODO:　Locationでallow_methodがあることがあるので呼び出しもとでこのチェックはしたい
  if (req_ctx.request->GetMethod() != "GET" &&
      req_ctx.request->GetMethod() != "POST") {
    return http::kMethodNotAllowed;
  }
  int redirect_fd[2], cgi_fd[2];
  pid_t pid;
  if (pipe(cgi_fd) == -1) {
    Logger::Error() << "pipe エラー" << std::endl;
    return http::kInternalServerError;
  }
  if (fcntl(cgi_fd[0], F_SETFL, O_NONBLOCK) == -1) {
    Logger::Error() << "fcntl エラー" << std::endl;
    close(cgi_fd[0]);
    close(cgi_fd[1]);
    return http::kInternalServerError;
  }
  if (req_ctx.request->GetMethod() == "POST") {
    if (pipe(redirect_fd) == -1) {
      Logger::Error() << "pipe エラー" << std::endl;
      close(cgi_fd[0]);
      close(cgi_fd[1]);
      return http::kInternalServerError;
    }
    if (fcntl(redirect_fd[1], F_SETFL, O_NONBLOCK) == -1) {
      Logger::Error() << "fcntl エラー" << std::endl;
      close(cgi_fd[0]);
      close(cgi_fd[1]);
      close(redirect_fd[0]);
      close(redirect_fd[1]);
      return http::kInternalServerError;
    }
    IOTaskManager::AddTask(
        new WriteToCGI(redirect_fd[1], req_ctx.request->GetBody()));
    Logger::Info() << "WriteToCGIを追加" << std::endl;
  }
  pid = fork();
  if (pid == -1) {
    Logger::Error() << "fork エラー" << std::endl;
    if (req_ctx.request->GetMethod() == "POST") {
      close(redirect_fd[0]);
      close(redirect_fd[1]);
    }
    close(cgi_fd[0]);
    close(cgi_fd[1]);
    return http::kInternalServerError;
  }
  if (pid == 0) {
    close(cgi_fd[0]);
    dup2(cgi_fd[1], 1);
    close(cgi_fd[1]);
    std::map<std::string, std::string> env_map =
        GetEnv(config, req_ctx, script_name, path_translated);
    char **env = DupEnv(env_map);
    if (env_map["REQUEST_METHOD"] == "POST") {
      close(redirect_fd[1]);
      dup2(redirect_fd[0], 0);
      close(redirect_fd[0]);
    }
    std::ifstream inf(script_name.c_str());
    if (inf.is_open() == false) std::exit(1);
    std::string first_line;
    std::getline(inf, first_line);
    const char **argv = MakeArgv(script_name, first_line);
    Logger::Info() << "CGI実行: " << argv[0] << std::endl;
    execve(argv[0], const_cast<char *const *>(argv), env);
    Logger::Error() << "execve エラー" << std::endl;
    std::exit(1);
  }
  close(cgi_fd[1]);
  if (req_ctx.request->GetMethod() == "POST") close(redirect_fd[0]);
  IOTaskManager::AddTask(new ReadFromCGI(pid, cgi_fd[0], req_ctx, config));
  Logger::Info() << "ReadFromCGIを追加" << std::endl;
  return http::kOk;
}

std::map<std::string, std::string> RequestHandler::GetEnv(
    const IConfig &config, const RequestContext &req_ctx,
    const std::string &script_name, const std::string &path_translated) {
  std::map<std::string, std::string> env_map;
  (void)config;
  const HTTPRequest *req = req_ctx.request;
  const std::map<std::string, std::string> &headers = req->GetHeaders();
  env_map["REQUEST_METHOD"] = req->GetMethod();
  if (headers.find("Authorization") != headers.end())
    env_map["AUTH_TYPE"] = headers.find("Authorization")->second;
  else
    env_map["AUTH_TYPE"] = "";
  if (env_map["REQUEST_METHOD"] == "POST") {
    unsigned int size = req->GetBody().size();
    std::stringstream ss;
    ss << size;
    env_map["CONTENT_LENGTH"] = ss.str();
  }
  if (headers.find("CONTENT-TYPE") != headers.end())
    env_map["CONTENT_TYPE"] = headers.find("CONTENT-TYPE")->second;
  else
    env_map["CONTENT_TYPE"] = "";
  env_map["GATEWAY_INTERFACE"] = "CGI/1.1";
  env_map["PATH_INFO"] = req->GetUri();
  env_map["PATH_TRANSLATED"] = path_translated;
  env_map["QUERY_STRING"] = req->GetQuery();
  // TODO: inet_ntoa使用禁止なので自作
  env_map["REMOTE_ADDR"] = inet_ntoa(req_ctx.client_addr.sin_addr);
  // REMOTE_HOSTは使用可能関数ではわからない
  env_map["REMOTE_HOST"] = "";
  // TODO: userの取得
  if (env_map["AUTH_TYPE"] == "")
    env_map["REMOTE_USER"] = "";
  else
    env_map["REMOTE_USER"] = "user";
  env_map["SCRIPT_NAME"] = script_name;
  // TODO: URIからサーバ名の取得
  env_map["SERVER_NAME"] = "localhost";
  env_map["SERVER_PORT"] = req_ctx.port;
  env_map["SERVER_PROTOCOL"] = "HTTP/1.1";
  env_map["SERVER_SOFTWARE"] = "webserv";
  return env_map;
}

char **RequestHandler::DupEnv(
    const std::map<std::string, std::string> &env_map) {
  char **env = new char *[env_map.size() + 1];
  Logger::Info() << "-------env_map--------" << env_map.size() << std::endl;
  std::map<std::string, std::string>::const_iterator it = env_map.begin();
  for (unsigned int i = 0; it != env_map.end(); ++it, ++i) {
    std::string tmp = it->first + "=" + it->second;
    env[i] = new char[tmp.size() + 1];
    std::strcpy(env[i], tmp.c_str());
    Logger::Info() << env[i] << std::endl;
  }
  return env;
}

void RequestHandler::DeleteEnv(char **env) {
  if (env == NULL) return;
  for (unsigned int i = 0; env[i] != NULL; ++i) {
    delete[] env[i];
  }
  delete[] env;
}

const char **RequestHandler::MakeArgv(const std::string &script_name,
                                      std::string &first_line) {
  const char **ret;
  if (first_line[0] == '#' && first_line[1] == '!') {
    first_line = first_line.substr(2);
    ret = const_cast<const char **>(new char *[3]);
    ret[0] = first_line.c_str();
    ret[1] = script_name.c_str();
  } else {
    ret = const_cast<const char **>(new char *[2]);
    ret[0] = script_name.c_str();
  }
  return ret;
}

RequestHandler::RequestHandler() {}

RequestHandler::RequestHandler(const RequestHandler &other) { (void)other; }

RequestHandler &RequestHandler::operator=(const RequestHandler &other) {
  (void)other;
  return *this;
}

RequestHandler::~RequestHandler() {}
