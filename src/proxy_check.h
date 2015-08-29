// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef PROXY_CHECK_H_
#define PROXY_CHECK_H_

#include <functional>
#include <memory>
#include <utility>

#include "boost/algorithm/string.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/bind.hpp"

#include "./http/http_con.h"

namespace botscript {

class proxy {
 public:
  explicit proxy(const std::string& proxy)
    : str_(proxy) {
    std::vector<std::string> split;
    boost::split(split, proxy, boost::is_any_of(":"));
    if (split.size() == 2) {
      host_ = split[0];
      port_ = split[1];
    }
  }

  proxy(std::string host, std::string port)
    : host_(std::move(host)),
      port_(std::move(port)) {
  }

  std::string str()  const { return str_; }
  std::string host() const { return host_; }
  std::string port() const { return port_; }

  bool operator==(const proxy& p) {
    return p.str() == str_;
  }

  bool operator<(const proxy& p) {
    return p.str() < str_;
  }

 private:
  std::string host_, port_, str_;
};

class proxy_check : public std::enable_shared_from_this<proxy_check>,
                    public boost::system::error_category {
 public:
  typedef std::function<void(std::shared_ptr<proxy_check>,
                             boost::system::error_code
                            )> callback;

  proxy_check(boost::asio::io_service* io_service, const proxy& proxy,
              std::string request,
              std::function<bool(std::string)>* check_fun = nullptr)
    : request_(std::move(request)),
      con_(std::make_shared<http::http_con>(io_service,
                                            proxy.host(), proxy.port(),
                                            boost::posix_time::seconds(30))),
      check_fun_(check_fun),
      proxy_(proxy) {
  }

  void check(callback cb) {
    con_->operator()(request_, boost::bind(&proxy_check::check_finish, this,
                                           shared_from_this(), cb, _1, _2, _3));
  }

  void check_finish(std::shared_ptr<proxy_check> self, callback cb,
                    std::shared_ptr<http::http_con> /* con */,
                    std::string response,
                    boost::system::error_code ec) {
    if (!ec && (check_fun_ == nullptr || (*check_fun_)(response))) {
      cb(self, ec);
    } else {
      cb(self, boost::system::error_code(-1, *this));
    }
  }

  const proxy& get_proxy() const {
    return proxy_;
  }

  const char* name() const noexcept { return "proxy"; }
  std::string message(int /* ev */) const {
    return "check failed";
  }

 private:
  std::string request_;
  std::shared_ptr<http::http_con> con_;
  std::function<bool(std::string)>* check_fun_;
  proxy proxy_;
};

}  // namespace botscript

#endif  // PROXY_CHECK_H_
