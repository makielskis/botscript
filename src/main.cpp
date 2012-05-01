/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 8. April 2012  makielskis@gmail.com
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

// Compile with
// clang -o botscript_exe  main.cpp -I/home/wasabi/boost_1_48_0/ -L/home/wasabi/Development/cppbot/botscript -L/home/wasabi/Development/cppbot/botscript -L/home/wasabi/boost_1_48_0/stage/lib/ bot.cpp lua_connection.cpp -lboost_system -lboost_regex -lboost_iostreams -pthread -ltidy -lpugixml -llua5.1

#include <fstream>
#include <istream>
#include <iostream>
#include <string>
#include <sstream>
#include <map>

#include "boost/thread.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/bind.hpp"
#include "boost/iostreams/copy.hpp"

#include "bot.h"

int main(int argc, char* argv[]) {
  botscript::bot* b = new botscript::bot("\"disabled", "blabla", "packages/pg", "http://muenchen.pennergame.de", "");
  std::cout << b->interface_description() << "\n";
  delete b;
  return 0;
}

/*
int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " config.json\n";
    return 1;
  }

  std::stringstream config;
  std::ifstream f(argv[1]);
  boost::iostreams::copy(f, config);

  try {
    botscript::bot* bot = new botscript::bot(config.str());
    boost::this_thread::sleep(boost::posix_time::seconds(24));
    delete bot;
  } catch(botscript::bad_login_exception& e) {
    std::cout << "bad login: " << e.what() << "\n";
  } catch(botscript::lua_exception& e) {
    std::cout << "lua error: " << e.what() << "\n";
  }

  return 0;
}
*/

/*
int main(void) {
  boost::asio::io_service io_service;

  boost::thread_group threads;
  boost::shared_ptr<boost::asio::io_service::work> work(
      new boost::asio::io_service::work(io_service));

  for(unsigned int i = 0; i < 1; ++i) {
    threads.create_thread(
        boost::bind(&boost::asio::io_service::run, &io_service));
  }

  botscript::bot* b1 = NULL, *b2 = NULL, *b3 = NULL, *b4 = NULL, *b5 = NULL;
  try {
    b1 = new botscript::bot("oclife", "blabla", "pg", "http://www.pennergame.de", &io_service);
    b2 = new botscript::bot("oclife2", "blabla", "pg", "http://koeln.pennergame.de", &io_service);
    b3 = new botscript::bot("oclife", "blabla", "pg", "http://berlin.pennergame.de", &io_service);
    b4 = new botscript::bot("oclife", "blabla", "pg", "http://www.bumrise.com", &io_service);
    b5 = new botscript::bot("oclife", "blabla", "pg", "http://www.dossergame.co.uk", &io_service);
    b1->execute("collect_set_active", "1");
    b2->execute("collect_set_active", "1");
    b3->execute("collect_set_active", "1");
    b4->execute("collect_set_active", "1");
    b5->execute("collect_set_active", "1");
  } catch (botscript::lua_exception& e) {
    std::cout << "lua_exception: " << e.what() << "\n";
  } catch (botscript::bad_login_exception& e) {
    std::cout << "bad login\n";
  }

  // io_service.run();

  boost::this_thread::sleep(boost::posix_time::seconds(1300));
  delete b1;
  delete b2;
  delete b3;
  delete b4;
  delete b5;

  work.reset();
  io_service.stop();
  threads.interrupt_all();
  threads.join_all();
  
  return 0;
}
*/

/*
#include <iostream>
#include <fstream>
#include <algorithm>
#include <strstream>

#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filter/gzip.hpp"

#include "./http.h"
#include "./webclient.h"

int main(void) {
  try {
    std::map<std::string, std::string> headers;
    headers["Connection"] = "keep-alive";
    headers["Accept-Encoding"] = "gzip,deflate,sdch";

    webclient wc(headers, "", "");
    std::string s = wc.request("http://www.pennergame.de/",
                               http_source::GET, NULL, 0);
    std::map<std::string, std::string> params;
    params["username"] = "oclife";
    params["password"] = "blabla";
    wc.submit("//input[@name = 'submitForm']", s, params, "");
  } catch (std::ios_base::failure& e) {
    std::cout << "io failure: " << e.what() << "\n";
  } catch (element_not_found_exception& e) {
    std::cout << "element not found: " << e.what() << "\n";
  }
  return 0;
}
*/
