/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 17. April 2012  makielskis@gmail.com
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

#include "./module.h"

#include <string>
#include <sstream>
#include <utility>

#include "boost/filesystem.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind.hpp"
#include "boost/function.hpp"

#include "./bot.h"
#include "./exceptions/lua_exception.h"

namespace botscript {

module::module(const std::string& script, bot* bot,
               boost::asio::io_service* io_service)
throw(lua_exception)
    : bot_(bot),
      lua_run_("run_"),
      lua_status_("status_"),
      lua_interface_("interface_"),
      lua_state_(NULL),
      io_service_(io_service),
      timer_(*io_service),
      stopping_(false) {
  // Discover module name.
  using boost::filesystem::path;
  std::string filename = path(script).filename().generic_string();
  module_name_ = filename.substr(0, filename.length() - 4);

  // Build basic strings.
  lua_run_ += module_name_;
  lua_status_ += module_name_;
  lua_interface_ += module_name_;
  lua_active_status_ += module_name_ + "_active";

  // Set active status to not running.
  bot_->status(lua_active_status_, "0");

  // Load module script.
  try {
    lua_state_ = lua_connection::newState(module_name_, bot);
    lua_connection::executeScript(script, lua_state_);
  } catch(const lua_exception& e) {
    lua_close(lua_state_);
    throw e;
  }

  // Initialize status.
  lua_connection::get_status(lua_state_, lua_status_, &status_);
  typedef std::pair<std::string, std::string> str_pair;
  BOOST_FOREACH(str_pair s, status_) {
    bot->status(module_name_ + "_" + s.first, s.second);
  }
  status_.clear();

  // Get user interface definition.
  interface_description_ = lua_connection::toJSON(lua_state_, lua_interface_,
                                                  module_name_);
}

module::~module() {
  lua_close(lua_state_);
}

void module::applyStatus() {
  // Lock because of r/w access to module status.
  boost::lock_guard<boost::mutex> lock(status_mutex_);

  // Write the status to the lua script state.
  typedef std::pair<std::string, std::string> str_pair;
  BOOST_FOREACH(str_pair s, status_) {
    try {
      lua_connection::set_status(lua_state_, lua_status_, s.first, s.second);
    } catch(const lua_exception& e) {
      std::stringstream msg;
      msg << "could not set status " << s.first << " = " << s.second;
      bot_->log(bot::INFO, module_name_, msg.str());
      return;
    }
  }

  // Clear status.
  status_.clear();
}

void module::run() {
  // Lock to prevent shutdown while execution.
  boost::lock_guard<boost::mutex> lock(run_mutex_);

  // Check active status.
  if (stopping_ || bot_->status(lua_active_status_) != "1") {
    stopping_ = false;
    bot_->log(bot::INFO, module_name_, "quit");
    return;
  }

  // Apply status changes for next run.
  applyStatus();

  // Start lua module run function.
  try {
    // Push run function to the stack and execute it.
    lua_getglobal(lua_state_, lua_run_.c_str());
    lua_connection::callFunction(lua_state_, 0, LUA_MULTRET, 0);

    // Check the return argument(s) of the lua function call.
    int argc = lua_gettop(lua_state_);
    int n1 = -1, n0 = -1;
    if (argc >= 2) {
      n1 = lua_isnumber(lua_state_, -2) ? luaL_checkint(lua_state_, -2) : -1;
    }
    if (argc >= 1) {
      n0 = lua_isnumber(lua_state_, -1) ? luaL_checkint(lua_state_, -1) : -1;
    }

    if (n0 == -1) {
      // Nothing returned that could be a waiting time. Quit.
      bot_->log(bot::ERROR, module_name_, "quit");
      bot_->status(lua_active_status_, "0");
      stopping_ = false;
      return;
    }

    // Determine and log the sleep time.
    int sleep = (n1 == -1) ? n0 : bot_->randomWait(n1, n0);
    std::stringstream msg;
    msg << "sleeping " << sleep << "s";
    bot_->log(bot::INFO, module_name_, msg.str());

    // Tell bot that the connection worked.
    bot_->connectionWorked();

    // Start the timer.
    if (!stopping_) {
      timer_.expires_from_now(boost::posix_time::seconds(sleep));
      timer_.async_wait(boost::bind(&module::run, this));
      return;
    }
  } catch(const lua_exception& e) {
    // Log error.
    bot_->log(bot::ERROR, module_name_, e.what());
    bot_->log(bot::ERROR, module_name_, "restarting in 10s");

    // Check and count error.
    if (boost::starts_with(e.what(), "#con")) {
      bot_->connectionFailed(true);
    } else if (boost::starts_with(e.what(), "#nof")) {
      bot_->connectionFailed(false);
    }

    // Start the timer.
    if (!stopping_) {
      timer_.expires_from_now(boost::posix_time::seconds(10));
      timer_.async_wait(boost::bind(&module::run, this));
      return;
    }
  }

  // Since all timer actions end with a return statement:
  // Tell user that the module is stopped now.
  stopping_ = false;
  bot_->log(bot::INFO, module_name_, "stop");
}

void module::execute(const std::string& command, const std::string& argument) {
  if (!boost::starts_with(command, module_name_ + "_set_")) {
    // Don't handle commands for other modules.
    // Don't handle commands when we're offline.
    return;
  }

  // Extract variable name.
  std::string var = command.substr(module_name_.length() + 5);
  if (var == "active") {
    // Handle active status command.
    std::string active_status = bot_->status(lua_active_status_);
    if (active_status == "1" && argument == "0") {
      // Command is to turn off and module is online.
      // Stop the module.
      if (run_mutex_.try_lock()) {
        timer_.cancel();
        bot_->log(bot::INFO, module_name_, "stop");
        bot_->status(lua_active_status_, "0");
        run_mutex_.unlock();
      } else {
        stopping_ = true;
        bot_->status(lua_active_status_, "0");
      }
    } else if (active_status == "0" && argument == "1") {
      // Command is to turn on and module is offline.
      // Start the module.
      bot_->log(bot::INFO, module_name_, "start");
      bot_->status(lua_active_status_, "1");
      if (run_mutex_.try_lock()) {
        io_service_->post(boost::bind(&module::run, this));
        run_mutex_.unlock();
      } else {
        stopping_ = false;
      }
    }
  } else {
    // Handle status variable changes.
    // Lock because of status map r/w access.
    boost::lock_guard<boost::mutex> lock(status_mutex_);

    // Extend internal key to full key.
    std::string full_key = module_name_ + "_" + var;

    // Apply status change if not already done.
    if (bot_->status(full_key) != argument) {
      std::stringstream msg;
      msg << "setting " << var << " to " << argument;
      bot_->log(bot::INFO, module_name_, msg.str());
      status_[var] = argument;
      bot_->status(full_key, argument);
    }
  }
}

void module::shutdown() {
  // Lock to not disturb the run function.
  boost::lock_guard<boost::mutex> lock(run_mutex_);

  // Cancel all asynchronous operations.
  bot_->status(lua_active_status_, "0");
  stopping_ = true;
  timer_.cancel();
}

}  // namespace botscript
