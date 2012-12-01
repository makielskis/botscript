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

#include <string>
#include <set>
#include <vector>
#include <list>
#include <map>
#include <sstream>

#include "boost/thread.hpp"
#include "boost/utility.hpp"
#include "boost/filesystem.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/function.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

#include "./module.h"
#include "./http/webclient.h"
#include "./lua_connection.h"

namespace botscript {

#define MAX_LOG_SIZE 50

#define CONTAINS(c, e) (find(c.begin(), c.end(), e) != c.end())

#define LOAD_ON  (0x01)  // 0000 0001
#define LOAD_OFF (0xFE)  // 1111 1110
#define EXEC_ON  (0x02)  // 0000 0010
#define EXEC_OFF (0xFD)  // 1111 1101

class module;

/// Bot class.
class bot : boost::noncopyable {
 public:
  enum { BS_LOG_DBG, BS_LOG_NFO, BS_LOG_ERR };

  /// Callback used for bot status and log updates.
  /// Arguments: identfier, key, value
  typedef boost::function<void (std::string, std::string, std::string)>
          update_callback;

  /// Callback used for initialization and login.
  /// Arguments: error string (maybe empty), error flag
  typedef boost::function<void (std::string, bool)> callback;

  /// Command sequence: Vector of command/argument pairs.
  typedef std::vector<std::pair<std::string, std::string>> command_sequence;

  /**
   * Creates a new bot.
   * Warning! Don't forget to call bot::loadConfiguration()
   * to initialize the bot.
   *
   * \param callback logging and status change update callback
   * \param io_service the boost asio io_service
   */
  explicit bot(boost::asio::io_service* io_service);

  /**
   * Destructor.
   *
   * Will call shutdown() if not already executed.
   * Call shutdown() yourself if you want to prevent blocking in the destructor.
   */
  virtual ~bot();

  /**
   * Stops all actions, will block until all actions are stopped.
   * After this the bot won't do anything, ready for destruction.
   * Don't call this method twice!
   */
  void shutdown();

  /**
   * Loads the JSON configuration and initializes the bot.
   *
   * Warning! This is needed to use the bot. Otherwise no modules are loaded
   * and the bot will not be logged in.
   *
   * Warning! This function is used asynchrnously. Don't delete the bot until
   * the provided callback has been called.
   *
   * A minimalistic configuration can look like this
   * \code
   * { "username":".", "password":".", "package":".", "server":"." }
   * \endcode
   *
   * Further configuration values are: proxy, wait_time_factor and modules
   * \code
   * {
   *   # basic configuration values
   *   "modules":{
   *     "a": {
   *        "active":"1",
   *        "do":"nothing"
   *      }
   * }
   * \endcode
   * 
   * \param configuration the configuration to load
   * \param cb will be called when the load process has finished
   */
  void configuration(const std::string& configuration, callback cb);

  /**
   * Creates a configuration string.
   *
   * \param with_password whether to include the password in the configuration
   * \return the JSON configuration string
   */
  std::string configuration(bool with_password);

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
   * \return a servers and interface listing for all packages (JSON string)
   */
  static std::string loadPackages(const std::string& folder);

  /**
   * Reads the module state.
   *
   * \param module name of the module to get the status from
   * \return the status of a module
   */
  std::map<std::string, std::string> module_status(const std::string& module);

  /**
   * Executes the given command.
   *
   * \param command the command to execute
   * \param argument the argument to pass
   */
  virtual void execute(const std::string& command, const std::string& argument);

  /// \return the bots identifier.
  std::string identifier() const { return identifier_; }

  /// \return the bots webclient.
  http::webclient* webclient() { return &webclient_; }

  /// \return the io_service
  boost::asio::io_service* io_service() { return io_service_; }

  /// \return the bots server address.
  std::string server() const { return server_; }

  /// \return all log messages in one string.
  std::string log_msgs();

  /// \return the wait time factor set.
  double wait_time_factor() { return wait_time_factor_; }

  /// \return a random wait time between min and max (multiplied with the wtf).
  int randomWait(int min, int max);

  /**
   * Logs a log message.
   *
   * \param type BS_LOG_NFO or BS_LOG_ERR
   * \param source the logging source (module)
   * \param message the message to log
   */
  void log(int type, const std::string& source, const std::string& message);

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

  /// This is the update/status change callback.
  update_callback callback_;

 private:
  class on_off_lock : boost::noncopyable {
   public:
    on_off_lock(char on_mask, char off_mask, bot* bot)
      : off_mask_(off_mask),
        bot_(bot) {
      bot->state_on(on_mask);
    }

    ~on_off_lock() {
      bot_->state_off(off_mask_);
    }

   private:
    char off_mask_;
    bot* bot_;
  };

  bool check_proxy(const std::string& proxy);
  void proxy(const std::string& proxy, callback cb);

  void state_on(char s);
  void state_off(char s);

  void load_modules();

  http::webclient webclient_;
  std::string username_;
  std::string password_;
  std::string package_;
  std::string server_;
  std::string identifier_;
  double wait_time_factor_;
  std::set<module*> modules_;
  bool stopped_;
  char state_;
  boost::mutex state_mutex_;
  boost::condition_variable state_cond_;

  boost::mutex execute_mutex_;

  std::list<std::string> log_msgs_;
  static boost::mutex log_mutex_;

  std::map<std::string, std::string> status_;
  boost::mutex status_mutex_;

  double connection_status_;
  boost::mutex connection_status_mutex_;

  std::vector<std::string> proxies_;

  boost::asio::io_service* io_service_;

  static boost::mutex server_mutex_;
  static std::vector<std::string> server_lists_;
  static std::map<std::string, std::string> servers_;
};

}  // namespace botscript

#endif  // BOT_H_
