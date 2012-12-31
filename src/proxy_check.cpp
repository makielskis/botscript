#include <iostream>
#include <fstream>
#include <functional>

#include "./http/http_source.h"
#include "./http/url.h"

#include "boost/asio.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/iostreams/filter/gzip.hpp"

namespace asio = boost::asio;

using namespace http;

void callback(std::shared_ptr<http_source> http, boost::system::error_code ec) {
  if (!ec || ec == asio::error::eof) {
    typedef boost::reference_wrapper<http::http_source> http_stream_ref;
    boost::iostreams::stream<http_stream_ref> s(boost::ref(*http));

    std::ofstream file_out;
    file_out.open ("out.bin", std::ios::binary);
    if (http->header("content-encoding") == "gzip") {
      boost::iostreams::filtering_streambuf<boost::iostreams::input> filter;
      filter.push(boost::iostreams::gzip_decompressor());
      filter.push(s);
      boost::iostreams::copy(filter, file_out);
    } else {
      boost::iostreams::copy(s, file_out);
    }
    file_out.close();
  } else {
    std::cout << "error: " << ec.message() << "\n";
  }
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cout << "no URL provided\n";
    return 1;
  }

  url u(argv[1]);

  asio::io_service io_service;
  asio::ip::tcp::resolver resolver(io_service);
  asio::ip::tcp::resolver::query query(u.host(), u.port());

  resolver.async_resolve(query,
                         [&u, &io_service](boost::system::error_code ec,
                                           asio::ip::tcp::resolver::iterator iterator) {
                           if (!ec) {
                             std::shared_ptr<http_source> http = std::make_shared<http_source>(
                                 &io_service, iterator,
                                 std::string("GET ") + u.path() + " HTTP/1.0\r\n"\
                                 "Host: " + u.host() + "\r\n"\
                                 "Accept-Encoding: gzip,deflate\r\n\r\n");
                             http->operator()(callback);
                           } else {
                             std::cout << ec.message() << "\n";
                           }
                         });
  io_service.run();

  return 0;
}

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
