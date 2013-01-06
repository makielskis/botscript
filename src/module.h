// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef MODULE_H_
#define MODULE_H_

#include <memory>
#include <string>

#include "boost/utility.hpp"

#include "./bot.h"
#include "./lua/lua_connection.h"

namespace botscript {

/// Bot module using a lua script.
class module : boost::noncopyable, public std::enable_shared_from_this<module> {
 public:
  /// \param script      the lua script to load
  /// \param bot         the bot that owns this module
  /// \param io_service  the io_service to use for asynchronous operations
  module(const std::string& script, std::shared_ptr<bot> bot,
         boost::asio::io_service* io_service);

  /// \param command   the command to execute
  /// \param argument  the command argument
  void execute(const std::string& command, const std::string& argument);

  /// \param lua_state the state to write the module_status_ to
  void set_lua_status(lua_State* lua_state);

  std::shared_ptr<bot> get_bot() const { return bot_; }
  std::string script()           const { return script_; }
  std::string lua_run()          const { return lua_run_; }
  std::string name()             const { return module_name_; }

 private:
  /// Enum representing the different states the module can be in.
  ///
  /// OFF       - The module is off (no action)
  /// RUN       - The module is running (executing the run function)
  /// STOP_RUN  - The module is running but will be stopped after this run
  /// WAIT      - The module has a timer waiting to wake up the run funciton
  enum { OFF, RUN, STOP_RUN, WAIT };

  /// \param s the state to turn into a string
  /// \return the string representation of s
  inline std::string state2s(char s) {
    switch (s) {
      case OFF:       return "OFF";
      case RUN:       return "RUN";
      case STOP_RUN:  return "STOP_RUN";
      case WAIT:      return "WAIT";
      default:        return "UNKNOWN";
    }
  }

  /// \param self shared pointer to self to keep us in mind
  /// \param ec the error code provided by the Asio deadline_timer
  void run(std::shared_ptr<module> self, boost::system::error_code);

  /// Callback function that will be called after the lua script execution has
  /// finished. Starts the wait timer to trigger module::run() again if module
  /// status has not changed to STOP_RUN.
  ///
  /// \param self      shared pointer to self to keep us in mind
  /// \param error     empty string on success, error message on error
  /// \param wait_min  minimum wait time
  /// \param wait_max  maximum wait time
  void run_cb(std::shared_ptr<module> self,
              std::string error, int wait_min, int wait_max);

  std::shared_ptr<bot> bot_;

  boost::asio::io_service* io_service_;

  std::string script_;
  std::string module_name_;
  std::string lua_run_;
  std::string lua_status_;
  std::string lua_active_status_;

  boost::asio::deadline_timer timer_;

  boost::mutex state_mutex_;
  char module_state_;

  std::function<void (std::string, int, int)> run_callback_;
};

}  // namespace botscript

#endif  // MODULE_H_
