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

#include "./module.h"

#include <string>
#include <utility>

#include "boost/utility.hpp"
#include "boost/lexical_cast.hpp"

namespace botscript {

module::module(const std::string& script, bot* bot,
               boost::asio::io_service* io_service)
throw(lua_exception)
    : module_state_(OFF),
      bot_(bot),
      io_service_(io_service),
      script_(script),
      lua_run_("run_"),
      lua_status_("status_"),
      timer_(*io_service) {
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
  lua_State* lua_state = nullptr;
  try {
    lua_state = lua_connection::new_state(module_name_, bot);
    lua_connection::execute(script, lua_state);
  } catch(const lua_exception& e) {
    lua_close(lua_state);
    throw e;
  }

  // Initialize status.
  std::map<std::string, std::string> lua_status;
  lua_connection::get_status(lua_state, lua_status_, &lua_status);
  typedef std::pair<std::string, std::string> str_pair;
  BOOST_FOREACH(str_pair s, lua_status) {
    bot->status(module_name_ + "_" + s.first, s.second);
  }
  lua_close(lua_state);
}

module::~module() {
  boost::lock_guard<boost::mutex> shutdown_lock(shutdown_mutex_);

  switch (module_state_) {
    case WAIT: {
      boost::unique_lock<boost::mutex> state_lock(state_mutex_);
      module_state_ = STOP_RUN;
      timer_.cancel();
      bot_->log(bot::BS_LOG_NFO, module_name_, "shutdown: waiting (WAIT)");
      while (module_state_ != OFF) {
        state_cond_.wait(state_lock);
      }
      break;
    }

    case STOP_RUN:
    case RUN: {
      bot_->log(bot::BS_LOG_NFO, module_name_, "shutdown: waiting (RUN)");
      {
        boost::unique_lock<boost::mutex> state_lock(state_mutex_);
        module_state_ = STOP_RUN;
        while (module_state_ != OFF) {
          state_cond_.wait(state_lock);
        }
      }
      break;
    }

    default: {
      bot_->log(bot::BS_LOG_NFO, module_name_, "shutdown: nothing to do");
    }
  }

  bot_->log(bot::BS_LOG_NFO, module_name_, "shutdown finished");
}

void module::run(const boost::system::error_code& ec) {
  // Check module state.
  char new_module_state;
  {
    boost::lock_guard<boost::mutex> lock(state_mutex_);
    if (module_state_ == STOP_RUN) {
      // Module state is STOP_RUN - stop!
      bot_->log(bot::BS_LOG_NFO, module_name_, "STOP_RUN -> run(): OFF");
      module_state_ = OFF;
      new_module_state = OFF;
    } else {
      // Switch from WAIT to RUN.
      // If it was OFF, the module will stop.
      new_module_state = module_state_;
      module_state_ = RUN;
    }
  }

  // Turn off and inform waiting instances if needed.
  if (new_module_state == OFF) {
    state_cond_.notify_all();
    return;
  }

  // Inform user about module start.
  bot_->log(bot::BS_LOG_NFO, module_name_, "starting");
  bot_->status(lua_active_status_, "1");

  // Start lua module run function.
  lua_State* lua_state = NULL;
  try {
    // Load module script.
    lua_state = lua_connection::new_state(module_name_, bot_);
    lua_connection::execute(script_, lua_state);

    // Load module status into lua.
    set_lua_status(lua_state);

    // Push run function to the stack and execute it.
    lua_getglobal(lua_state, lua_run_.c_str());
    lua_connection::call(lua_state, 0, LUA_MULTRET, 0);

    // Check the return argument(s) of the lua function call.
    int argc = lua_gettop(lua_state);
    int n1 = -1, n0 = -1;
    if (argc >= 2) {
      n1 = lua_isnumber(lua_state, -2) ? luaL_checkint(lua_state, -2) : -1;
    }
    if (argc >= 1) {
      n0 = lua_isnumber(lua_state, -1) ? luaL_checkint(lua_state, -1) : -1;
    }

    // Close lua state.
    lua_close(lua_state);

    {
      // Lock because of r/w access to module_state_.
      boost::lock_guard<boost::mutex> lock(state_mutex_);

      // Start sleep timer if a waiting time was returned.
      // Otherwise (no waiting time): module will turn OFF.
      if (n0 != -1 && module_state_ == RUN) {
        // Determine and log the sleep time.
        int sleep = (n1 == -1) ? n0 : bot_->randomWait(n1, n0);
        std::stringstream msg;
        msg << "sleeping " << sleep << "s";
        bot_->log(bot::BS_LOG_NFO, module_name_, msg.str());

        // Tell bot that the connection worked.
        bot_->connectionWorked();

        // Start the timer.
        timer_.expires_from_now(boost::posix_time::seconds(sleep));
        timer_.async_wait(boost::bind(&module::run, this, _1));
        module_state_ = WAIT;
        return;
      }
    }
  } catch(const lua_exception& e) {
    // Close lua state on error.
    lua_close(lua_state);

    int wait_time = bot_->randomWait(120, 360);

    // Log error.
    bot_->log(bot::BS_LOG_ERR, module_name_, e.what());
    bot_->log(bot::BS_LOG_ERR, module_name_,
              std::string("restarting in ") +
              boost::lexical_cast<std::string>(wait_time) + "s");

    // Check and count error.
    if (boost::starts_with(e.what(), "#con")) {
      bot_->connectionFailed(true);
    } else if (boost::starts_with(e.what(), "#nof")) {
      bot_->connectionFailed(false);
    }

    {
      // Lock because of r/w access to module_state_.
      boost::lock_guard<boost::mutex> lock(state_mutex_);

      // Start the timer.
      if (module_state_ == RUN) {
        timer_.expires_from_now(boost::posix_time::seconds(wait_time));
        timer_.async_wait(boost::bind(&module::run, this, _1));
        module_state_ = WAIT;
        return;
      }
    }
  }

  // Since all timer actions end with a return statement:
  // Tell user that the module is stopped now.
  {
    // Lock because of write access to module_state_.
    boost::lock_guard<boost::mutex> lock(state_mutex_);
    bot_->status(lua_active_status_, "0");
    bot_->log(bot::BS_LOG_NFO, module_name_,
              state2s(module_state_) + " -> run(): OFF");
    module_state_ = OFF;

    // Notify everyone waiting for the module to stop.
    state_cond_.notify_all();
  }
}

void module::execute(const std::string& command, const std::string& argument) {
  // Stop execute if module is about to shutdown.
  if (!shutdown_mutex_.try_lock()) {
    std::cerr << "fatal: execute(), shutdown_ = true\n";
    return;
  } else {
    shutdown_mutex_.unlock();
  }

  {
    boost::lock_guard<boost::mutex> lock(shutdown_mutex_);
    boost::unique_lock<boost::mutex> state_lock(state_mutex_);

    if (!boost::starts_with(command, module_name_ + "_set_")) {
      // Don't handle commands for other modules.
      return;
    }

    // Extract variable name.
    std::string var = command.substr(module_name_.length() + 5);
    if (var == "active") {
      bool start = (argument == "1");
      if (start) {
        // Handle start command.
        switch (module_state_) {
          case OFF: {
            bot_->log(bot::BS_LOG_NFO, module_name_, "OFF -> start: RUN");
            module_state_ = RUN;
            boost::system::error_code ignored;
            io_service_->post(boost::bind(&module::run, this, ignored));
            bot_->status(lua_active_status_, "1");
            break;
          }

          case STOP_RUN: {
            bot_->log(bot::BS_LOG_NFO, module_name_, "STOP_RUN -> start: RUN");
            module_state_ = RUN;
            bot_->status(lua_active_status_, "1");
          }

          default: {
            bot_->log(bot::BS_LOG_NFO, module_name_,
                      state2s(module_state_) + " -> start: nothing to do");
          }
        }
      } else {
        // Handle stop command.
        switch (module_state_) {
          case WAIT: {
            bot_->log(bot::BS_LOG_NFO, module_name_, "WAIT -> stop: STOP_RUN");
            timer_.cancel();
            module_state_ = STOP_RUN;
            bot_->status(lua_active_status_, "0");
            break;
          }

          case RUN: {
            module_state_ = STOP_RUN;
            bot_->log(bot::BS_LOG_NFO, module_name_, "RUN -> stop: STOP_RUN");
            bot_->status(lua_active_status_, "0");
            break;
          }

          default: {
            bot_->log(bot::BS_LOG_NFO, module_name_,
                      state2s(module_state_) + " -> stop: nothing to do");
          }
        }
      }
    } else {
      // Handle status variable changes.
      // Extend internal key to full key.
      std::string full_key = module_name_ + "_" + var;

      // Apply status change if not already done.
      if (bot_->status(full_key) != argument) {
        std::stringstream msg;
        msg << "setting " << var << " to " << argument;
        bot_->log(bot::BS_LOG_NFO, module_name_, msg.str());
        bot_->status(full_key, argument);
      }
    }
  }
}

void module::set_lua_status(lua_State* lua_state) {
  // Get current module staus from the bot.
  std::map<std::string, std::string> status = bot_->module_status(module_name_);

  // Write the status to the lua script state.
  typedef std::pair<std::string, std::string> str_pair;
  BOOST_FOREACH(str_pair s, status) {
    try {
      lua_connection::set_status(lua_state, lua_status_, s.first, s.second);
    } catch(const lua_exception& e) {
      std::stringstream msg;
      msg << "could not set status " << s.first << " = " << s.second;
      bot_->log(bot::BS_LOG_NFO, module_name_, msg.str());
      return;
    }
  }
}

}  // namespace botscript
