/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 *
 * Copyright (C) December 1st, 2012 makielskis@gmail.com
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

#define MAX_LOG_SIZE 50
#define CONTAINS(c, e) (find(c.begin(), c.end(), e) != c.end())

#include <string>
#include <set>
#include <utility>

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/function.hpp>

#include "./http/webclient.h"

namespace botscript {

/// Bot class.
class bot : boost::noncopyable, public boost::enable_shared_from_this<bot> {
 public:
  /// Log message types.
  enum { BS_LOG_DBG, BS_LOG_NFO, BS_LOG_ERR };

  /// Command sequence: Vector of command/argument pairs.
  typedef std::vector<std::pair<std::string, std::string>> command_sequence;

  /// Update callback: called when the bot status changed or for log messages.
  typedef boost::function<void (std::string, std::string, std::string)> upd_cb;

  /// Callback function for asynchronous actions.
  /// Provides the error message if an error was thrown. The error string is
  /// empty for operations that were completed successfuly.
  typedef boost::function<void (std::string)> error_callback;

  /// Creates a new bot. The bot needs to be initialized
  /// by a seperate call to bot::init() to be ready for usage.
  ///
  /// \param io_service the boost asio io_service object
  explicit bot(boost::asio::io_service* io_service);

  /// Destructor (just for debug output).
  virtual ~bot();

  /// Starts the shutdown process of the bot (remove shared references, ...)
  void shutdown();

  /// Initialization function:
  ///
  ///   * Loads the bot configuration
  ///   * Tries to login the bot
  ///   * Instantiates the modules
  ///
  /// Calls the specified callback function with an empty string on success
  /// or with an error string if an error occured.
  ///
  /// A minimalistic configuration can look like this
  /// \code
  /// { "username":".", "password":".", "package":".", "server":"." }
  /// \endcode
  ///
  /// Further configuration values are: proxy, wait_time_factor and modules
  /// \code
  /// {
  ///   # basic configuration values
  ///   "modules":{
  ///     "a": {
  ///        "active":"1",
  ///        "do":"nothing"
  ///      }
  /// }
  /// \endcode
  ///
  /// \param config the configuration to load
  /// \param cb the callback to call when the operation has finished
  void init(const std::string& config, const error_callback& cb);

  /// Creates a unique identifier with the given information.
  ///
  /// \param username the bot username
  /// \param package the script package
  /// \param server the server address
  /// \return the created identifier
  static std::string identifier(const std::string& username,
                                const std::string& package,
                                const std::string& server);

  static std::string load_packages(const std::string& folder);

  /// Login callback function to be called when an login try finished.
  ///
  /// \param error the error message (empty if no errors occured)
  /// \param cb the callback to call on login finish
  /// \param init_commands the commands to call when the login finished
  /// \param tries the count of remaining tries
  void handle_login(const std::string& err,
                    const error_callback& cb,
                    const command_sequence& init_commands,
                    int tries);

  /// \return the username
  std::string username();

  /// \return the password
  std::string password();

  /// \return the package
  std::string package();

  /// \return the server
  std::string server();

  /// \return the identifier
  std::string identifier();

  /// \return the webclient
  http::webclient* webclient();

  /// \return the wait time factor set.
  double wait_time_factor();

  /// \return a random wait time between min and max (multiplied with the wtf).
  int random(int a, int b);

  /// \return all log messages in one string.
  std::string log_msgs();

  /// Logs a log message.
  ///
  /// \param type the log level
  /// \param source the logging source (module)
  /// \param message the message to log
  void log(int type, const std::string& source, const std::string& message);

  /// \param key the key of the value to return
  /// \return the value
  std::string status(const std::string key);

  /// \param key the key
  /// \param value the value to set
  void status(const std::string key, const std::string value);

  /// Extracts the status of a single module from the bot state.
  ///
  /// \param module name of the module to get the status from
  /// \return the status of a module
  std::map<std::string, std::string> module_status(const std::string& module);

  void execute(std::string, std::string) { std::cout << "exec not implmented\n"; }

  /// This is the update/status change callback.
  upd_cb callback_;

 private:
  /// Login callback.
  error_callback login_cb_;

  /// Boost Asio I/O service object.
  boost::asio::io_service* io_service_;

  /// Web browser agent.
  http::webclient webclient_;

  /// Basic bot information.
  std::string username_, password_, package_, server_, identifier_;

  /// Wait time factor.
  double wait_time_factor_;

  /// List of log messages.
  std::list<std::string> log_msgs_;

  /// Lock to synchronize logging.
  static boost::mutex log_mutex_;

  /// Bot status.
  std::map<std::string, std::string> status_;

  /// Lock to synchronize status access.
  boost::mutex status_mutex_;

  /// Mutex synchronizing the access the server address list.
  static boost::mutex server_mutex_;

  /// List of server lists contained in the server mapping.
  static std::vector<std::string> server_lists_;

  /// Server address to short tag mapping.
  static std::map<std::string, std::string> servers_;
};

}  // namespace botscript

#endif  // BOT_H_
