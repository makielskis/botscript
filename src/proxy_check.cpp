#include <iostream>
#include <fstream>
#include <functional>

#include "./http/url.h"
#include "./http/util.h"
#include "./http/webclient.h"

namespace asio = boost::asio;

using namespace http;

void callback(std::string response, boost::system::error_code ec) {
  if (!ec) {
    std::cout << "response: {" << response << "}\n";
  } else {
    std::cout << "error: " << ec.message() << "\n";
  }
}

int main(int argc, char* argv[]) {
  typedef std::shared_ptr<http_con> http_ptr;

  if (argc == 1) {
    std::cout << "no URL provided\n";
    return 1;
  }

  boost::asio::io_service io_service;
  webclient wc(&io_service);
  url u(argv[1]);
  wc.request(url(argv[1]), util::GET, "", callback, 3);
  io_service.run();

  return 0;
}

/*
int main(int argc, char* argv[]) {
  using http_ptr = std::shared_ptr<http_con>;

  if (argc == 1) {
    std::cout << "no URL provided\n";
    return 1;
  }

  url u(argv[1]);
  std::string request = std::string("GET ") + u.path() + " HTTP/1.1\r\n"\
                        "Host: " + u.host() + "\r\n"\
                        "Connection: Keep-Alive\r\n"\
                        "Accept-Encoding: gzip,deflate\r\n\r\n";
  boost::asio::io_service io_service;
  http_ptr con0 = std::make_shared<http_con>(&io_service, u.host(), u.port());
  http_ptr con1 = std::make_shared<http_con>(&io_service, u.host(), u.port());
  con0->operator()(request, callback);
  con1->operator()(request, callback);
  io_service.run();

  io_service.reset();
  con0->operator()(request, callback);
  io_service.run();

  return 0;
}
*/

/*
int main() {
  url url1("http://www.domain.tl/");
  url url2("http://www.domain.tl/path");
  url url3("http://www.domain.tl:8080/");
  url url4("http://www.domain.tl:8080/path");

  std::cout << url1.host() << " " << url1.port() << " " << url1.path() << "\n";
  std::cout << url2.host() << " " << url2.port() << " " << url2.path() << "\n";
  std::cout << url3.host() << " " << url3.port() << " " << url3.path() << "\n";
  std::cout << url4.host() << " " << url4.port() << " " << url4.path() << "\n";

  try {
    url url5("http//domain.tl:8080/");
  } catch(std::invalid_argument e) {
    std::cout << "invalid_argument catched\n";
  } catch(...) {
    std::cout << "something else catched\n";
  }

  return 0;
}
*/

/*
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
*/
