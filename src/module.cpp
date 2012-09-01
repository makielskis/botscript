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

namespace botscript {

module::module(const std::string& script, bot* bot,
               boost::asio::io_service* io_service)
throw(lua_exception)
    : stop_(false),
      bot_(bot),
      lua_run_("run_"),
      lua_status_("status_"),
      lua_state_(NULL),
      io_service_(io_service),
      timer_(*io_service),
      module_state_(OFF) {
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
  try {
    lua_state_ = lua_connection::newState(module_name_, bot);
    lua_connection::executeScript(script, lua_state_);
  } catch(const lua_exception& e) {
    lua_close(lua_state_);
    throw e;
  }

  // Initialize status.
  // We're abusing the status_ member variable as temporary store.
  lua_connection::get_status(lua_state_, lua_status_, &status_);
  typedef std::pair<std::string, std::string> str_pair;
  BOOST_FOREACH(str_pair s, status_) {
    bot->status(module_name_ + "_" + s.first, s.second);
  }
  status_.clear();
}

module::~module() {
  stop_ = true;
  switch(module_state_) {
    case WAIT:
      stop_waiting();
      break;
    case RUN:
      boost::unique_lock<boost::mutex> state_lock(state_mutex_);
      while (module_state_ != OFF) {
        state_cond_.wait(state_lock);
      }
      break;
  }
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

void module::state(char new_state) {
  boost::lock_guard<boost::mutex> lock(state_mutex_);
  switch(new_state) {
    case RUN:
      bot_->log(bot::INFO, module_name_, "started");
      bot_->status(lua_active_status_, "1");
      break;
    case WAIT:
      bot_->log(bot::INFO, module_name_, "started waiting");
      bot_->status(lua_active_status_, "1");
      break;
    case OFF:
      stop_ = false;
      bot_->log(bot::INFO, module_name_, "quit");
      bot_->status(lua_active_status_, "0");
      break;
  }
  module_state_ = new_state;
  state_cond_.notify_all();
}

void module::stop_waiting() {
  // Cancel timer and ensure that it has finished execution.
  if (timer_.cancel()) {
    boost::unique_lock<boost::mutex> state_lock(state_mutex_);
    while (module_state_ != OFF) {
      state_cond_.wait(state_lock);
    }
  }
}

void module::run(const boost::system::error_code& ec) {
  // Handle stop.
  if (ec == boost::asio::error::operation_aborted || stop_) {
    state(OFF);
    return;
  }

  // Set module state to running.
  state(RUN);

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
      state(OFF);
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
    if (!stop_) {
      timer_.expires_from_now(boost::posix_time::seconds(sleep));
      timer_.async_wait(boost::bind(&module::run, this, _1));
      state(WAIT);
      return;
    }
  } catch(const lua_exception& e) {
    // Log error.
    bot_->log(bot::ERROR, module_name_, e.what());
    bot_->log(bot::ERROR, module_name_, "restarting in 30s");

    // Check and count error.
    if (boost::starts_with(e.what(), "#con")) {
      bot_->connectionFailed(true);
    } else if (boost::starts_with(e.what(), "#nof")) {
      bot_->connectionFailed(false);
    }

    // Start the timer.
    if (!stop_) {
      timer_.expires_from_now(boost::posix_time::seconds(30));
      timer_.async_wait(boost::bind(&module::run, this, _1));
      state(WAIT);
      return;
    }
  }

  // Since all timer actions end with a return statement:
  // Tell user that the module is stopped now.
  state(OFF);
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
    bool start = (argument == "1");
    if (!stop_) {
      // Condition: stop_ flag is not set.
      if (start) {
        // Handle START command.
        switch(module_state_) {
          case RUN:
          case WAIT:
            break;
          case OFF:
            boost::system::error_code ignored;
            io_service_->post(boost::bind(&module::run, this, ignored));
            break;
        }
      } else {
        // Handle STOP command.
        switch(module_state_) {
          case RUN:
            stop_ = true;
            break;
          case WAIT:
            stop_waiting();
            break;
          case OFF:
            break;
        }
      }
    } else {
      // Condition: stop_ flag is not set.
      if (start) {
        // Handle START command.
        switch(module_state_) {
          case RUN:
            stop_ = false;
            break;
          case WAIT: {
            bot_->log(bot::ERROR, module_name_,
                      "fatal error: state = WAIT, stop_");
            stop_ = false;
            stop_waiting();
            boost::system::error_code ignored;
            io_service_->post(boost::bind(&module::run, this, ignored));
            break;
          }
          case OFF:
            stop_ = false;
            bot_->log(bot::ERROR, module_name_,
                      "fatal error: state = OFF, stop_");
            break;
        }
      } else {
        // Handle STOP command.
        switch(module_state_) {
          case RUN:
            break;
          case WAIT:
            bot_->log(bot::ERROR, module_name_,
                      "fatal error: state = WAIT, stop_");
            stop_ = false;
            stop_waiting();
            break;
          case OFF:
            bot_->log(bot::ERROR, module_name_,
                      "fatal error: state = OFF, stop_");
            stop_ = false;
            break;
        }
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
