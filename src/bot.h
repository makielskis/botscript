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
#include <list>
#include <map>
#include <sstream>

#include "boost/thread.hpp"
#include "boost/utility.hpp"
#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/thread.hpp"
#include "boost/function.hpp"

#include "./module.h"
#include "./webclient.h"
#include "./lua_connection.h"
#include "./exceptions/lua_exception.h"
#include "./exceptions/bad_login_exception.h"
#include "./exceptions/invalid_proxy_exception.h"

namespace botscript {

#define MAX_LOG_SIZE 50

#define CONTAINS(c, e) (find(c.begin(), c.end(), e) != c.end())

class module;
typedef boost::function<void (std::string id, std::string k, std::string v) > update_ptr;

class bot : boost::noncopyable {
 public:
  enum { INFO, ERROR };

  bot(const std::string& username, const std::string& password,
      const std::string& package, const std::string& server,
      const std::string& proxy)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception);

  explicit bot(const std::string& configuration)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception);

  virtual ~bot();

  static std::string createIdentifier(const std::string& username,
                                      const std::string& package,
                                      const std::string& server);
  static std::string loadPackages(const std::string& folder);

  std::string configuration(bool with_password);

  std::string interface_description();

  void execute(const std::string& command, const std::string& argument);

  std::string identifier() const { return identifier_; }
  botscript::webclient* webclient() { return &webclient_; }
  std::string server() const { return server_; }
  double wait_time_factor() { return wait_time_factor_; }

  int randomWait(int min, int max);

  void log(int type, const std::string& source, const std::string& message);
  std::string log_msgs();

  virtual void callback(std::string id, std::string k, std::string v) {};

  std::string status(const std::string key);
  void status(const std::string key, const std::string value);

 private:
  void init(const std::string& proxy)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception);

  void loadModules(boost::asio::io_service* io_service);

  botscript::webclient webclient_;
  std::string username_;
  std::string password_;
  std::string package_;
  std::string server_;
  std::string identifier_;
  double wait_time_factor_;
  lua_State* lua_state_;
  bool stopped_;
  std::set<module*> modules_;
  std::list<std::string> log_msgs_;
  static boost::mutex log_mutex_;
  std::map<std::string, std::string> status_;
  boost::mutex status_mutex_;

  static boost::mutex server_mutex_;
  static std::vector<std::string> server_lists_;
  static std::map<std::string, std::string> servers_;

  static std::map<std::string, std::string> interface_;
  static boost::mutex interface_mutex_;

  static int bot_count_;
  static boost::mutex init_mutex_;
  static boost::asio::io_service* io_service_;
  static boost::asio::io_service::work* work_;
  static boost::thread_group* worker_threads_;
};

}  // namespace botscript

#endif  // BOT_H_
