// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_WEBCLIENT_H_
#define HTTP_WEBCLIENT_H_

#include <functional>
#include <string>
#include <map>

#include "boost/asio/io_service.hpp"
#include "boost/system/error_code.hpp"
#include "boost/utility.hpp"
#include "boost/thread.hpp"

#include "pugixml.hpp"

#include "./error.h"
#include "./http_con.h"

#define MAX_REDIRECT 3

namespace http {

class webclient : boost::noncopyable {
 public:
  typedef std::function<void (std::string, boost::system::error_code)> callback;

  webclient(boost::asio::io_service* io_service);

  void proxy(std::string host, std::string port);
  std::string proxy_host() const { return proxy_host_; }
  std::string proxy_port() const { return proxy_port_; }

  void submit(const std::string& xpath,
              const std::string& page,
              std::map<std::string, std::string> input_params,
              const std::string& action,
              callback cb);

  void request(const url& u, int method, std::string body, callback cb,
               int remaining_redirects);

 private:
  void request_finish(const std::string& host,
                      int remaining_redirects, callback cb,
                      std::shared_ptr<http_con> con_ptr,
                      std::string response,
                      boost::system::error_code ec);

  void store_cookies(const std::string& new_cookies);

  static error::http_category cat_;

  boost::mutex proxy_mutex_;
  std::string proxy_host_;
  std::string proxy_port_;

  std::map<std::string, std::string> headers_;

  boost::mutex cookies_mutex_;
  std::map<std::string, std::string> cookies_;

  boost::asio::io_service* io_service_;
};

}  // namespace http

#endif  // HTTP_WEBCLIENT_H_
