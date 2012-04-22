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

#include "boost/thread.hpp"
#include "boost/utility.hpp"
#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/asio/io_service.hpp"

#include "./module.h"
#include "./webclient.h"
#include "./lua_connection.h"
#include "./exceptions/lua_exception.h"
#include "./exceptions/bad_login_exception.h"

namespace botscript {

#define CONTAINS(c, e) (find(c.begin(), c.end(), e) != c.end())

class module;

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

class bot : boost::noncopyable {
 public:
  enum { INFO, ERROR };

  bot(const std::string& username, const std::string password,
      const std::string& package, const std::string server,
      boost::asio::io_service* io_service)
  throw(lua_exception, bad_login_exception);

  ~bot();

  static std::string createIdentifier(const std::string& username,
                                      const std::string& package,
                                      const std::string& server);

  void execute(const std::string& command, const std::string& argument);

  std::string identifier() const { return identifier_; }
  botscript::webclient* webclient() { return &webclient_; }
  std::string server() const { return server_; }

  int random_wait(int min, int max) { return min; }

  void log(int type, const std::string& source, const std::string& message);

  std::string status(const std::string key);
  void status(const std::string key, const std::string value);

 private:
  void loadModules(boost::asio::io_service* io_service);

  botscript::webclient webclient_;
  std::string username_;
  std::string password_;
  std::string package_;
  std::string server_;
  std::string identifier_;
  lua_State* lua_state_;
  std::set<module*> modules_;
  std::vector<log_msg> log_msgs_;
  std::map<std::string, std::string> status_;
  boost::mutex status_mutex_;

  static boost::mutex server_mutex_;
  static std::vector<std::string> server_lists_;
  static std::map<std::string, std::string> servers_;
};

}  // namespace botscript

#endif  // BOT_H_
