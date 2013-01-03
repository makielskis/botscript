// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_CON_H_
#define HTTP_CON_H_

#include <memory>
#include <functional>

#include "./coroutine/coroutine.hpp"
#include "./url.h"
#include "./http_source.h"

namespace http {

class http_con : public std::enable_shared_from_this<http_con> {
 public:
  typedef std::function<void (std::shared_ptr<http_con>,
                              std::string,
                              boost::system::error_code
                             )> callback;

  http_con(boost::asio::io_service* io_service,
           std::string host, std::string port);

  const http_source& http_src() const { return *src_; }

  void operator()(std::string request_str, callback cb);

 private:
  void request(std::shared_ptr<http_con> self,
               std::string request_str, callback cb,
               boost::system::error_code ec);
  void resolve(std::shared_ptr<http_con> self,
               std::string request_str, callback cb);
  void connect(std::shared_ptr<http_con> self,
               std::string request_str, callback cb,
               boost::system::error_code ec,
               boost::asio::ip::tcp::resolver::iterator iterator);
  void on_connect(std::shared_ptr<http_con> self,
                  std::string request_str, callback cb,
                  boost::system::error_code ec,
                  boost::asio::ip::tcp::resolver::iterator iterator);
  void request_finish(std::shared_ptr<http_con> self, callback cb,
                      std::shared_ptr<http_source> http_src,
                      boost::system::error_code ec);

  boost::asio::io_service* io_service_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::deadline_timer req_timeout_timer_;
  std::shared_ptr<http_source> src_;
  std::string host_, port_;
  bool connected_ = false;
};

}  // namespace http

#endif  // HTTP_H_
