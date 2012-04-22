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

#include "boost/filesystem.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind.hpp"
#include "boost/function.hpp"

#include "./bot.h"
#include "./exceptions/lua_exception.h"

namespace botscript {

module::module(const std::string& script, bot* bot, lua_State* main_state,
               boost::asio::io_service* io_service)
    : bot_(bot),
      lua_run_("run_"),
      lua_status_("status_"),
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
  lua_active_status_ += module_name_ + "_active";

  // Set active status to not running.
  bot_->status(lua_active_status_, "0");

  // Load module script.
  lua_state_ = lua_connection::newState(module_name_, bot);
  lua_connection::executeScript(script, lua_state_);

  // Initialize status.
  lua_connection::get_status(lua_state_, lua_status_, &status_);
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
      } catch (lua_exception& e) {
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
    int sleep = (n1 == -1) ? n0 : bot_->random_wait(n1, n0);
    std::stringstream msg;
    msg << "sleeping " << sleep << "s";
    bot_->log(bot::INFO, module_name_, msg.str());

    // Start the timer.
    timer_.expires_from_now(boost::posix_time::seconds(sleep));
    timer_.async_wait(boost::bind(&module::run, this));
  } catch (lua_exception& e) {
    // Log error.
    bot_->log(bot::ERROR, module_name_, e.what());
    bot_->log(bot::ERROR, module_name_, "restarting in 10s");

    // Start the timer.
    timer_.expires_from_now(boost::posix_time::seconds(10));
    timer_.async_wait(boost::bind(&module::run, this));
  }
}

void module::execute(const std::string& command, const std::string& argument) {
  if (!boost::starts_with(command, module_name_ + "_set_")) {
    // Don't handle commands for other modules.
    return;
  }

  // Extract variable name.
  std::string var = command.substr(module_name_.length() + 5);
  if (var == "active") {
    // Handle active status command.
    std::string active_status = bot_->status(lua_active_status_);
    if (active_status == "1" && argument == "0") {
      // Command is to turn on and module is offline.
      // Stop the module.
      bot_->log(bot::INFO, module_name_, "stopping");
      bot_->status(lua_active_status_, "0");
      stopping_ = true;
    } else if (active_status == "0" && argument == "1") {
      // Command is to turn off and module is online.
      // Start the module.
      bot_->log(bot::INFO, module_name_, "start");
      bot_->status(lua_active_status_, "1");
      if (stopping_) {
        stopping_ = false;
      } else {
        io_service_->post(boost::bind(&module::run, this));
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
}  // namespace botscript
