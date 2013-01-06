#include <iostream>
#include <sstream>

#include "boost/asio/io_service.hpp"
#include "boost/asio/deadline_timer.hpp"

#include "./bot.h"

using namespace botscript;
namespace asio = boost::asio;

void print_log(const std::string& msg) {
#if defined _WIN32 || defined _WIN64
  HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
  int color = 0x0007;
  if (msg.substr(0, 4) == "[ERR") {
    color = FOREGROUND_RED;
  } else if (msg.substr(0, 4) == "[INF") {
    color = FOREGROUND_GREEN;
  }
  SetConsoleTextAttribute(console, color);
  std::cout << msg << std::flush;
  SetConsoleTextAttribute(console, 0x0007);
#else
  if (msg.substr(0, 4) == "[ERR") {
    std::cout << "\033[0;31m";
  } else if (msg.substr(0, 4) == "[INF") {
    std::cout << "\033[0;32m";
  }
  std::cout << msg << "\033[0m" << std::flush;
#endif
}

void cb(std::shared_ptr<bot>, std::string error) {
  if (!error.empty()) {
    std::cout << "ERROR: " << error << "\n";
  }
}

int main() {
  asio::io_service io_service;

  asio::io_service::work work(io_service);

  std::shared_ptr<bot> b = std::make_shared<bot>(&io_service);
  b->callback_ = [](std::string, std::string k, std::string v) {
    if (k == "log") { print_log(v); }
  };
  b->init("{ "\
          "\"username\": \"oclife\","\
          "\"password\": \"blabla\","\
          "\"package\": \"packages/du\","\
          /* "\"proxy\": \"81.169.173.88:3128\","\ */
          "\"server\": \"http://www.knastvoegel.de\","\
          "\"modules\": { \"train\": { \"active\":\"1\", \"timeslot\": \"0\", \"type\": \"mental\" } }"\
          "}",
          cb);

  asio::deadline_timer stop_timer(io_service, boost::posix_time::seconds(30));
  stop_timer.async_wait([&io_service](boost::system::error_code) {
    //io_service.stop();
  });

  io_service.run();
  b->shutdown();

  return 0;
}
