#include <iostream>

#include "boost/asio/io_service.hpp"

#include "./bot.h"

using namespace botscript;
namespace asio = boost::asio;

void cb(std::string error) {
  if (error.empty()) {
    std::cout << "login successful!\n";
  } else {
    std::cout << "ERROR: " << error << "\n";
  }
}

int main() {
  asio::io_service io_service;

  asio::io_service::work work(io_service);

  bot::load_packages("packages");
  std::shared_ptr<bot> b = std::make_shared<bot>(&io_service);
  b->callback_ = [](std::string, std::string k, std::string v) {
    if (k == "log") { std::cout << v << std::flush; }
  };
  b->init("{ "\
          "\"username\": \"oclife\","\
          "\"password\": \"blabla\","\
          "\"package\": \"packages/du\","\
          "\"server\": \"http://www.knastvoegel.de\" }",
          cb);

  io_service.run();

  return 0;
}
