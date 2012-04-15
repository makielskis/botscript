/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 14. April 2012  makielskis@gmail.com
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

#ifndef BOT_H_
#define BOT_H_

#include <inttypes.h>
#include <string>
#include <set>
#include <vector>
#include <map>

#include "boost/utility.hpp"
#include "boost/filesystem.hpp"

#include "./lua_connection.h"
#include "./webclient.h"

namespace botscript {

#define CONTAINS(c, e) (find(c.begin(), c.end(), e) != c.end())

class module;
class log_msg;
class bad_login_exception;

class bot : boost::noncopyable {
 public:
  bot(const std::string& username, const std::string password,
      const std::string& package, const std::string server)
    : username_(username),
      password_(password),
      package_(package),
      server_(server) {
    identifier_ = createIdentifier(username, package, server);
  }

  static std::string createIdentifier(const std::string& username,
                                      const std::string& package,
                                      const std::string& server) {
    // Load servers from package.
    std::string server_list = package + "/servers.lua";
    if (!CONTAINS(server_lists_, server_list)) {
      lua::loadServerList(server_list, &servers_);
      server_lists_.push_back(server_list);
    }

    // Create identifier.
    std::string identifier = package + "_";
    if (servers_.find(server) == servers_.end()) {
      identifier += server;
    } else {
      identifier += servers_[server];
    }
    identifier += "_";
    identifier += username;

    return identifier;
  }

  std::string identifier() const {
    return identifier_;
  }

 private:
  void loadModules(const std::string& package) {
  }

  webclient wc_;
  std::string username_;
  std::string password_;
  std::string package_;
  std::string server_;
  std::string identifier_;
  std::set<module> modules;
  std::vector<log_msg> log_msgs;
  std::map<std::string, std::string> status;

  static std::vector<std::string> server_lists_;
  static std::map<std::string, std::string> servers_;
};

// Initialization of the static bot class attributes.
std::map<std::string, std::string>bot::servers_ =
        std::map<std::string, std::string>();
std::vector<std::string> bot::server_lists_ = std::vector<std::string>();

class module : boost::noncopyable {
 public:
  module(const std::string& script, bot* bot, lua_State* main_state) {
    boost::filesystem::path p(script);
    //  std::cout << p.file_string() << "\n";
  }

 private:
};

class log_msg {
 public:
  log_msg(int64_t timestamp, int type, const std::string& message)
    : timestamp_(timestamp),
      type_(type),
      message_(message) {
  }

  int64_t timestamp() const {
    return timestamp_;
  }

  int type() const {
    return type_;
  }

  std::string message() const {
    return message_;
  }

 private:
  int64_t timestamp_;
  int type_;
  std::string message_;
};

}  // namespace botscript

#endif  // BOT_H_
