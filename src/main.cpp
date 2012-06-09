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

#include <iostream>
#include <fstream>
#include <string>
#include <set>

#include "boost/foreach.hpp"
#include "boost/thread.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/bind.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/regex.hpp"

#include "./bot.h"
#include "./webclient.h"

/// Bot derived from botscript::bot to put log output to the console.
class stdout_bot : public botscript::bot {
 public:

  /**
   * \param username the login username
   * \param password the login password
   * \param package the lua scrip package
   *                 (contains at least servers.lua and base.lua)
   * \param server the server address to use
   * \param proxy the proxy to use (empty for direct connection).
                  (only the first proxy in a list will be checked!)
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   */
  stdout_bot(const std::string& username, const std::string& password,
        const std::string& package, const std::string& server,
        const std::string& proxy)
  throw(botscript::lua_exception, botscript::bad_login_exception,
        botscript::invalid_proxy_exception)
    : botscript::bot(username, password, package, server, proxy) {
  }

  /**
   * Loads the given bot configuration.
   *
   * \param configuration JSON configuration string
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   */
  explicit stdout_bot(const std::string& configuration)
  throw(botscript::lua_exception, botscript::bad_login_exception,
        botscript::invalid_proxy_exception)
    : bot(configuration) {
  };

  /**
   * Callback - logs to console, ignores status changes.
   */
  void callback(std::string id, std::string k, std::string v) {
    if (k == "log") {
      std::cout << v << std::flush;
    }
  }
};

int main(int argc, char* argv[]) {
  // Check usage.
  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " config.json\n";
    return 1;
  }

  // Read configuration.
  std::stringstream config;
  std::ifstream f(argv[1]);
  boost::iostreams::copy(f, config);

  // Load configuration.
  try {
    stdout_bot* bot = new stdout_bot(config.str());
    while (true) { boost::this_thread::sleep(boost::posix_time::hours(24)); }
    delete bot;
  } catch(const botscript::bad_login_exception& e) {
    std::cout << "bad login: " << e.what() << "\n";
  } catch(const botscript::lua_exception& e) {
    std::cout << "lua error: " << e.what() << "\n";
  } catch(const botscript::invalid_proxy_exception& e) {
    std::cout << "invalid proxy\n";
  }

  return 0;
}
