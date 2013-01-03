#include <iostream>
#include <functional>
#include <memory>
#include <utility>

#include "boost/asio/io_service.hpp"
#include "boost/bind.hpp"

#include "http/http_con.h"

namespace asio = boost::asio;

class proxy_check : public std::enable_shared_from_this<proxy_check>,
                    public boost::system::error_category {
 public:
  typedef std::function<void(std::shared_ptr<proxy_check>,
                             boost::system::error_code
                            )> callback;

  proxy_check(boost::asio::io_service* io_service,
              std::string host, std::string port,
              std::string request, std::function<bool(std::string)>* check_fun)
    : request_(std::move(request)),
      con_(std::make_shared<http::http_con>(io_service,
                                            std::move(host), std::move(port),
                                            boost::posix_time::seconds(15))),
      check_fun_(check_fun) {
  }

  void check(callback cb) {
    con_->operator()(request_, boost::bind(&proxy_check::check_finish, this,
                                           shared_from_this(), cb, _1, _2, _3));
  }

  void check_finish(std::shared_ptr<proxy_check> self, callback cb,
                    std::shared_ptr<http::http_con> con, std::string response,
                    boost::system::error_code ec) {
    if (ec || (*check_fun_)(response)) {
      cb(self, ec);
    } else {
      cb(self, boost::system::error_code(-1, *this));
    }
  }
  
  const char* name() const { return "proxy"; }
  std::string message(int ev) const {
    return "check failed";
  }

 private:
   std::string request_;
   std::shared_ptr<http::http_con> con_;
   std::function<bool(std::string)>* check_fun_;
};

bool check(const std::string& s) {
  return s.find("farbflut") != std::string::npos;
}

void callback(std::shared_ptr<proxy_check>, boost::system::error_code ec) {
  std::cout << ec.message() << "\n";
}

int main() {
  asio::io_service io_service;

  std::string request = "GET http://www.pennergame.de/ HTTP/1.1\r\n"\
                        "Host: www.pennergame.de\r\n\r\n";
  std::function<bool(std::string)> check_fun = check;

  std::vector<std::shared_ptr<proxy_check>> proxies;
  proxies.emplace_back(std::make_shared<proxy_check>(&io_service, "86.10.202.142", "3128", request, &check_fun));
  proxies.emplace_back(std::make_shared<proxy_check>(&io_service, "85.10.202.142", "3128", request, &check_fun));

  for (auto proxy : proxies) {
    proxy->check(callback);
  }

  io_service.run();

  return 0;
}