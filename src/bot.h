// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef BOT_H_
#define BOT_H_

#define MAX_LOG_SIZE 50

#include <string>
#include <set>
#include <utility>
#include <algorithm>
#include <memory>
#include <functional>

#include "boost/utility.hpp"
#include "boost/asio/io_service.hpp"

#include "./bot_browser.h"
#include "./module.h"
#include "./lua/state_wrapper.h"
#include "./package.h"
#include "./bot_config.h"

namespace botscript {

class bot_browser;
class module;

/// Bot class.
class bot : boost::noncopyable, public std::enable_shared_from_this<bot> {
 public:
  /// Log message types.
  enum { BS_LOG_DBG, BS_LOG_NFO, BS_LOG_ERR };

  /// Command sequence: Vector of command/argument pairs.
  typedef std::vector<std::pair<std::string, std::string>> command_sequence;

  /// Update callback: called when the bot status changed or for log messages.
  typedef std::function<void (std::string, std::string, std::string)> upd_cb;

  /// Callback function for asynchronous actions.
  /// Provides the error message if an error was thrown. The error string is
  /// empty for operations that were completed successfuly.
  typedef std::function<void (std::shared_ptr<bot>, std::string)> error_cb;

  /// Creates a new bot. The bot needs to be initialized
  /// by a seperate call to bot::init() to be ready for usage.
  ///
  /// \param io_service          the boost asio io_service object
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
  /// \param config the configuration to load
  /// \param cb the callback to call when the operation has finished
  void init(std::shared_ptr<bot_config> configuration, const error_cb& cb);

  /// Creates a unique identifier with the given information.
  ///
  /// \param username the bot username
  /// \param package the script package
  /// \param server the server address
  /// \return the created identifier
  static std::string identifier(const std::string& username,
                                const std::string& package,
                                const std::string& server);

  /// Login callback function to be called when an login try finished.
  ///
  /// \param self           shared pointer to self to keep us in mind
  /// \param state_wr       the lua state that's executing the login script
  /// \param error          the error message (empty if no errors occured)
  /// \param cb             the callback to call on login finish
  /// \param init_commands  the commands to call when the login finished
  /// \param tries          the count of remaining tries
  void handle_login(std::shared_ptr<bot> self,
                    std::shared_ptr<state_wrapper> state_wr,
                    const std::string& err,
                    const error_cb& cb,
                    const command_sequence& init_commands, bool load_mod,
                    int tries);

  /// \return the underlying config object
  bot_config& configuration();

  const bot_config& configuration() const;

  std::shared_ptr<bot_config> config();

  /// \return the webclient
  bot_browser* browser();

  /// Loads the packages and returns them JSON encoded string.
  ///
  /// \param path the path to load the packages from
  /// \return the packages as vector of JSON encoded package information
  static std::vector<std::string> load_packages(const std::string& path);

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

  /// Calls the callback function with the current value of the key.
  ///
  /// \param key the key to refresh
  void refresh_status(const std::string& key);

  /// Updates the bot status in the configuration and triggers the update cb.
  ///
  /// \param key    the key of the value that changed
  /// \param value  the new value
  void status(const std::string& key, const std::string& value);

  /// \param command   command to execute
  /// \param argument  command argument
  void execute(const std::string& command, const std::string& argument);

  /// This is the update/status change callback.
  upd_cb update_callback_;

 private:
  /// Loads the lua modules located at package_. This includes only files that
  /// end on ".lua", don't start with '.' and are not named 'servers.lua' or
  /// 'base.lua'. Executes the given command sequence to initialize the modules.
  ///
  /// \param init_commands the initialization commands
  void load_modules(const command_sequence& init_commands);

  /// Login callback.
  std::function<void(std::string)> login_cb_;

  /// Boost Asio I/O service object.
  boost::asio::io_service* io_service_;

  /// Bot configuration.
  std::shared_ptr<bot_config> configuration_;

  /// Web browser agent.
  std::shared_ptr<bot_browser> browser_;

  /// Bot identifier.
  std::string identifier_;

  /// Modules.
  std::vector<std::shared_ptr<module>> modules_;

  /// Wait time factor.
  float wait_time_factor_;

  /// List of log messages.
  std::list<std::string> log_msgs_;

  /// Flag indicating whether the login_result_ variable is active.
  bool login_result_stored_;

  /// Stores the login result.
  bool login_result_;

  /// Flag indicating whether a proxy check is currently active.
  bool proxy_check_active_;

  /// The bots package.
  std::shared_ptr<package> package_;

  /// Packages.
  static std::map<std::string, std::shared_ptr<package>> packages_;
};

}  // namespace botscript

#endif  // BOT_H_
