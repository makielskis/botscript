/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 14. August 2012  makielskis@gmail.com
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

#ifndef MODULE_H_
#define MODULE_H_

#include <string>
#include <map>

#include "boost/asio/io_service.hpp"
#include "boost/asio/basic_deadline_timer.hpp"
#include "boost/thread.hpp"
#include "boost/thread/condition_variable.hpp"

#include "./bot.h"
#include "./exceptions/lua_exception.h"
#include "./lua_connection.h"

namespace botscript {

class bot;

/// Bot module using a lua script.
class module : boost::noncopyable {
 public:
  /**
   * Creates a new module starting the given script. The script has to contain
   * a table called modulename_state and run_modulename.
   *
   * \param script the path of the script to use
   * \param bot the bot owning the module
   * \param io_service the io service to use for asynchronous operations
   * \exception lua_exception if loading the script fails
   */
  module(const std::string& script, bot* bot,
         boost::asio::io_service* io_service)
  throw(lua_exception);

  /// Destructor.
  ~module();

  /// Returns the module name.
  std::string name() { return module_name_; }

  /**
   * Executes the given command on this module.
   * Does nothing if the command is not available.
   *
   * Warning - do not execute this function in parallel.
   * It is NOT threadsafe. The bot prevents parallel execution.
   *
   * \param command the command to execute
   * \param argument the argument to deliver
   */
  void execute(const std::string& command, const std::string& argument);

 private:
  /*
   * Enum representing the different states the module can be in.
   *
   * OFF       - The module is off (no action)
   * RUN       - The module is running (executing the run function)
   * STOP_RUN  - The module is running but will be stopped after this run
   * WAIT      - The module has a timer waiting to wake up the run funciton
   */
  enum { OFF, RUN, STOP_RUN, WAIT };

  /**
   * \param s the state to turn into a string
   * \return the string representation of s
   */
  std::string state2s(char s) {
    switch (s) {
      case OFF:       return "OFF";
      case RUN:       return "RUN";
      case STOP_RUN:  return "STOP_RUN";
      case WAIT:      return "WAIT";
      default:        return "UNKNOWN";
    }
  }

  void applyStatus();
  void run(const boost::system::error_code& ec);

  boost::mutex shutdown_mutex_;

  char module_state_;
  boost::condition_variable state_cond_;
  boost::mutex state_mutex_;

  bot* bot_;
  boost::asio::io_service* io_service_;

  std::string module_name_;
  std::string lua_run_;
  std::string lua_status_;
  std::string lua_active_status_;

  lua_State* lua_state_;

  std::map<std::string, std::string> status_;
  boost::mutex status_mutex_;

  boost::asio::deadline_timer timer_;
};

}  // namespace botscript

#endif  // MODULE_H_
