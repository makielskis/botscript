#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "boost/iostreams/copy.hpp"
#include "boost/shared_ptr.hpp"

#include "./bot.h"

using namespace botscript;

void update_cb(std::string id, std::string k, std::string v) {
  if (k == "log") {
    std::cout << v;
  }
}

void cb(const std::string& error) {
  if (error.empty()) {
    std::cout << "success!\n";
  } else {
    std::cout << "ERROR: " << error << "\n";
  }
}

void on_exit(boost::asio::io_service* io_service) {
  io_service->stop();
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " c0nfig\n";
    return 0;
  }

  std::string path = argv[1];
  std::ifstream file;
  file.open(path.c_str(), std::ios::in);
  std::stringstream content;
  boost::iostreams::copy(file, content);
  file.close();

  boost::asio::io_service io_service;
  boost::asio::deadline_timer timer(io_service);
  timer.expires_from_now(boost::posix_time::seconds(40));
  timer.async_wait(boost::bind(on_exit, &io_service));
  boost::asio::io_service::work work(io_service);
  boost::shared_ptr<bot> b = boost::make_shared<bot>(&io_service);
  b->callback_ = update_cb;
  b->init(content.str(), cb);
  io_service.run();

  b->shutdown();
  std::cout << "use_count at exit: " << b.use_count() << "\n";
  b.reset();
  std::cout << "use_count at exit: " << b.use_count() << "\n";

  return 0;
}
