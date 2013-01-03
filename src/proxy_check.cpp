// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include <iostream>
#include <functional>
#include <memory>

#include "boost/asio/io_service.hpp"

#include "./proxy_check.h"

namespace asio = boost::asio;
using namespace botscript;

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