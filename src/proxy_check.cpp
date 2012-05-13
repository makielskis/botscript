/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 13. May 2012  makielskis@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <istream>
#include <fstream>
#include <iostream>
#include <string>
#include <set>

#include "boost/foreach.hpp"
#include "boost/thread.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/bind.hpp"
#include "boost/regex.hpp"

#include "client/dbclient.h"

#include "./webclient.h"

#define CHECK_URL "http://www.pennergame.de/"
#define CHECK_STR "Farbflut"

mongo::DBClientConnection db;
boost::mutex db_mutex;

class proxy_check {
 public:
  proxy_check(std::string host, std::string port,
              boost::asio::io_service* io_service)
    : host_(host),
      port_(port),
      check_result_(false) {
    io_service->post(boost::bind(&proxy_check::check, this));
  }

  void check() {
    // Try to get page with proxy.
    botscript::webclient wc;
    wc.proxy(host_, port_);
    std::string page;
    try {
      page = wc.request_get(CHECK_URL);
    } catch(const std::ios_base::failure& e) {
    }

    // Check page.
    check_result_ = (page.find(CHECK_STR) == std::string::npos);

    {
      // Update database.
      boost::lock_guard<boost::mutex> lock(db_mutex);
      std::string proxy = host_ + ":" + port_;
      if (check_result_) {
        db.update("test.proxies",
                  BSON("address" << proxy),
                  BSON("address" << proxy << "good" << check_result_),
                  true);
      } else {
        db.remove("test.proxies", BSON("address" << proxy));
      }
    }
  }

  std::string host() { return host_; }
  std::string port() { return port_; }
  bool result() { return check_result_; }

 private:
  std::string host_;
  std::string port_;
  bool check_result_;
};

int main(int argc, char* argv[]) {
  // Check command line argument.
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " proxypages.txt\n";
    return 1;
  }

  // Connect to database.
  std::string err;
  if (!db.connect("localhost", err)) {
    std::cerr << "mongodb error: " << err << "\n";
    return 1;
  }

  // Prepare thread pool for boost::asio::io_service execution.
  boost::thread_group threads;
  boost::asio::io_service io_service;
  boost::shared_ptr<boost::asio::io_service::work> work(
      new boost::asio::io_service::work(io_service));
  for(unsigned int i = 0; i < 5; ++i) {
    threads.create_thread(
        boost::bind(&boost::asio::io_service::run, &io_service));
  }

  // Prepare proxy information depository.
	std::set<std::string> proxies;
  std::set<proxy_check*> checks;

  // Open file.
  std::string line;
  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "could not open proxies file\n";
  }

  while (file.good()) {
    // Read page address.
    std::getline(file, line);
    if (line.length() < 11) {
      continue;
    }
    std::cout << "reading " << line << "\n";

    // Read page.
	  botscript::webclient wc;
	  std::string page;
    try {
      page = wc.request_get(line);
    } catch(const std::ios_base::failure& e) {
      std::cout << "could not read " << line << e.what() << "\n";
      continue;
    }

    // Extract proxies with regular expression.
	  boost::regex regex(
        "((\\d{1, 3}\\.\\d{1, 3}\\.\\d{1, 3}\\.\\d{1, 3}):(\\d{1, 5}))");
	  boost::sregex_iterator regex_iter(page.begin(), page.end(), regex), end;
	  for (; regex_iter != end; ++regex_iter) {
      std::string proxy = (*regex_iter)[1].str();
		  if (proxies.find(proxy) == proxies.end()) {
		    std::string host = (*regex_iter)[2].str();
        std::string port = (*regex_iter)[3].str();
        checks.insert(new proxy_check(host, port, &io_service));
      	proxies.insert(proxy);
      }
	  }
  }

  // Close file.
  file.close();

  // Stop io_service and all threads.
  work.reset();
  io_service.run();
  io_service.stop();
  threads.join_all();

  // Delete checks.
  BOOST_FOREACH(proxy_check* check, checks) {
    delete check;
  }

  return 0;
}
