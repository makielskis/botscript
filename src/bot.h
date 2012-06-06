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
typedef boost::function<void (std::string id, std::string k, std::string v) >
        update_ptr;

/// Bot class.
class bot : boost::noncopyable {
 public:
  enum { INFO, ERROR };

  /**
   * Creates a new bot.
   *
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
  bot(const std::string& username, const std::string& password,
      const std::string& package, const std::string& server,
      const std::string& proxy)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception);

  /**
   * Loads the given bot configuration.
   *
   * \param configuration JSON configuration string
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   */
  explicit bot(const std::string& configuration)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception);

  virtual ~bot();

  /**
   * Creates a unique identifier with the given information
   *
   * \param username the bot username
   * \param package the script package
   * \param server the server address
   */
  static std::string createIdentifier(const std::string& username,
                                      const std::string& package,
                                      const std::string& server);

  /**
   * Loads all packages contained in the folder.
   *
   * \param folder the folder the packages reside in
   * \return a servers listing for all packages (JSON string)
   */
  static std::string loadPackages(const std::string& folder);

  /**
   * Creates a configuration string.
   *
   * \param with_password whether to include the password in the configuration
   * \return the JSON configuration string
   */
  std::string configuration(bool with_password);

  /// Returns the interface description (JSON).
  std::string interface_description();

  /**
   * Executes the given command.
   *
   * \param command the command to execute
   * \param argument the argument to pass
   */
  void execute(const std::string& command, const std::string& argument);

  /// Returns the bots identifier.
  std::string identifier() const { return identifier_; }

  /// Returns the bots webclient.
  botscript::webclient* webclient() { return &webclient_; }

  /// Returns the bots server address.
  std::string server() const { return server_; }

  /// Returns all log messages in one string.
  std::string log_msgs();

  /// Returns the wait time factor set.
  double wait_time_factor() { return wait_time_factor_; }

  /// Returns a random wait time between min and max (multiplied with the wtf).
  int randomWait(int min, int max);

  /**
   * Logs a log message.
   *
   * \param type INFO or ERROR
   * \param source the logging source (module)
   * \param message the message to log
   */
  void log(int type, const std::string& source, const std::string& message);

  /**
   * Callback function that should be reimplemented in derivated classes.
   *
   * \param id the bot identifier
   * \param k the key that changed
   * \param v the new value
   */
  virtual void callback(std::string id, std::string k, std::string v) {}

  /**
   * Returns the status (value) of the given key.
   *
   * \param key the key of the value to return
   * \return the value
   */
  std::string status(const std::string key);

  /**
   * Sets the status key to the given value.
   *
   * \param key the key
   * \param value the value to set
   */
  void status(const std::string key, const std::string value);

  /**
   * Called when a connection failed. Used to determine whether a proxy is too
   * bad and should be replaced. Counts fails and success.
   *
   * \param connection_error whether it was an connection error
   *                         (or for example element not found error)
   */
  void connectionFailed(bool connection_error);

  /// Called when something worked indicating that the current proxy is good.
  void connectionWorked();

 private:
  void init(const std::string& proxy)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception);

  bool checkProxy(std::string proxy);
  void setProxy(const std::string& proxy, bool check_only_first)
  throw(invalid_proxy_exception);

  void loadModules(boost::asio::io_service* io_service);

  botscript::webclient webclient_;
  std::string username_;
  std::string password_;
  std::string package_;
  std::string server_;
  std::string identifier_;
  double wait_time_factor_;
  bool stopped_;
  std::set<module*> modules_;

  boost::mutex execute_mutex_;

  std::list<std::string> log_msgs_;
  static boost::mutex log_mutex_;

  std::map<std::string, std::string> status_;
  boost::mutex status_mutex_;

  double connection_status_;
  boost::mutex connection_status_mutex_;

  std::vector<std::string> proxies_;

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
