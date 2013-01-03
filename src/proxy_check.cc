#include <iostream>
#include <fstream>
#include <functional>

#include "./http/url.h"
#include "./http/util.h"
#include "./http/webclient.h"
#include <iostream>
#include <vector>
#include <memory>

#include "./http/proxy_list_check.h"

#include "boost/asio/io_service.hpp"

bool check(const std::string& s) {
  std::cout << s << "\n\n\n\n";
  return s.find("farbflut") != std::string::npos;
}

void callback(const std::vector<std::string> proxies) {
  std::cout << "working proxies:\n";
  for (const std::string& p : proxies) {
    std::cout << p << "\n";
  }
}

int main() {
  asio::io_service io_service;

  asio::io_service::work work(io_service);

  std::vector<std::string> proxies;
  proxies.push_back("62.113.200.202:3128");
  proxies.push_back("85.10.202.142:3128");

  std::shared_ptr<proxy_list_check> list_check = std::make_shared<proxy_list_check>(&io_service, 20, proxies, check, callback);
  list_check->start();

  io_service.run();

  return 0;
}
