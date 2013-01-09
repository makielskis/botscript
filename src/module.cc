// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./module.h"

#include "boost/lexical_cast.hpp"

namespace botscript {

namespace asio = boost::asio;

module::module(const std::string& script, std::shared_ptr<bot> bot,
               asio::io_service* io_service)
    : io_service_(io_service),
      bot_(bot),
      script_(script),
      lua_run_("run_"),
      lua_status_("status_"),
      timer_(*io_service),
      module_state_(OFF),
      run_result_stored_(false),
      wait_min_(-1),
      wait_max_(-1) {
  bot_->log(bot::BS_LOG_NFO, "base", std::string("loading module ") + script);

  // Discover module name.
  using boost::filesystem::path;
  std::string filename = path(script).filename().generic_string();
  module_name_ = filename.substr(0, filename.length() - 4);

  // Build basic strings.
  lua_run_ += module_name_;
  lua_status_ += module_name_;
  lua_active_status_ += module_name_ + "_active";

  // Set active status to "0" (not running).
  bot_->status(lua_active_status_, "0");

  // Initialize status.
  std::map<std::string, std::string> lua_status;
  lua_connection::get_status(script, lua_status_, &lua_status);
  for(const auto& s : lua_status) {
    bot->status(module_name_ + "_" + s.first, s.second);
  }
}

void module::run(std::shared_ptr<module> self, boost::system::error_code) {
  // Check module state.
  {
    boost::lock_guard<boost::mutex> lock(state_mutex_);
    if (module_state_ == OFF || module_state_ == STOP_RUN) {
      // Module state is STOP_RUN - stop!
      bot_->log(bot::BS_LOG_DBG, module_name_, "STOP_RUN -> run(): OFF");
      module_state_ = OFF;
      return;
    } else {
      // Switch from WAIT to RUN.
      module_state_ = RUN;
    }
  }

  // Start module.
  bot_->log(bot::BS_LOG_NFO, module_name_, "starting");
  std::shared_ptr<state_wrapper> state = std::make_shared<state_wrapper>();
  run_callback_ = boost::bind(&module::run_cb, this, self, state, _1);
  lua_connection::module_run(state->get(), this, &run_callback_);
}

void module::run_cb(std::shared_ptr<module> self,
                    std::shared_ptr<state_wrapper> state_wr,
                    std::string err) {
  if (!err.empty()) {
    state_wr->close();
    run_callback_ = nullptr;
    run_result_stored_ = false;

    bot_->log(bot::BS_LOG_ERR, module_name_, err);

    module_state_ = WAIT;
    int sleep = bot_->random(60, 120);
    timer_.expires_from_now(boost::posix_time::seconds(sleep));
    timer_.async_wait(boost::bind(&module::run, this, self, _1));
    std::string s_str = boost::lexical_cast<std::string>(sleep);
    bot_->log(bot::BS_LOG_NFO, module_name_, std::string("sleeping ") + s_str);
  } else {
    if (!run_result_stored_) {
      run_result_stored_ = true;

      lua_State* state = state_wr->get();
      int argc = lua_gettop(state);
      wait_min_ = -1;
      wait_max_ = -1;
      if (argc >= 2) {
        wait_min_ = lua_isnumber(state, -2) ? luaL_checkint(state, -2) : -1;
      }
      if (argc >= 1) {
        wait_max_ = lua_isnumber(state, -1) ? luaL_checkint(state, -1) : -1;
      }
    } else {
      state_wr->close();
      run_callback_ = nullptr;
      run_result_stored_ = false;

      {
        boost::lock_guard<boost::mutex> lock(state_mutex_);
        if (module_state_ == OFF || module_state_ == STOP_RUN) {
          // Module state is STOP_RUN - stop!
          bot_->log(bot::BS_LOG_DBG, module_name_, "STOP_RUN -> run(): OFF");
          module_state_ = OFF;
          return;
        }
      }

      module_state_ = WAIT;

      int sleep;
      if (wait_min_ >= 0 && wait_max_ >= 0) {
        sleep = bot_->random(wait_min_, wait_max_);
      } else if (wait_min_ >= 0) {
        sleep = wait_min_;
      } else {
        sleep = bot_->random(60, 120);
      }

      timer_.expires_from_now(boost::posix_time::seconds(sleep));
      timer_.async_wait(boost::bind(&module::run, this, self, _1));

      std::string str = boost::lexical_cast<std::string>(sleep);
      bot_->log(bot::BS_LOG_NFO, module_name_, std::string("sleeping ") + str);
    }
  }
}

void module::execute(const std::string& command, const std::string& argument) {
  // Don't handle commands for other modules.
  bool global = false;
  if (!boost::starts_with(command, module_name_ +  "_set_") &&
      !(global = boost::starts_with(command, "global_set_"))) {
    return;
  }

  // Stop execute if we would have to wait for the state mutex.
  if (!state_mutex_.try_lock()) {
    bot_->log(bot::BS_LOG_NFO, module_name_, "execute not possible (locked)");
    return;
  }

  // Let the lock guard adopt the lock (RAII).
  boost::lock_guard<boost::mutex> state_lock(state_mutex_, boost::adopt_lock);

  // Extract variable name.
  std::string var = global ? command.substr(11) :
                             command.substr(module_name_.length() + 5);

  if (var == "active") {
    bool start = (argument == "1");
    if (start) {
      // Handle start command.
      switch (module_state_) {
        case OFF: {
          bot_->log(bot::BS_LOG_DBG, module_name_, "OFF -> start: RUN");
          module_state_ = RUN;
          boost::system::error_code ignored;
          io_service_->post(boost::bind(&module::run, this,
                                        shared_from_this(), ignored));
          bot_->status(lua_active_status_, "1");
          break;
        }

        case STOP_RUN: {
          bot_->log(bot::BS_LOG_DBG, module_name_, "STOP_RUN -> start: RUN");
          module_state_ = RUN;
          bot_->status(lua_active_status_, "1");
        }

        default: {
          bot_->log(bot::BS_LOG_DBG, module_name_,
                    state2s(module_state_) + " -> start: nothing to do");
        }
      }
    } else {
      // Handle stop command.
      switch (module_state_) {
        case WAIT: {
          bot_->log(bot::BS_LOG_DBG, module_name_, "WAIT -> stop: STOP_RUN");
          timer_.cancel();
          module_state_ = STOP_RUN;
          bot_->status(lua_active_status_, "0");
          break;
        }

        case RUN: {
          module_state_ = STOP_RUN;
          bot_->log(bot::BS_LOG_DBG, module_name_, "RUN -> stop: STOP_RUN");
          bot_->status(lua_active_status_, "0");
          break;
        }

        default: {
          bot_->log(bot::BS_LOG_DBG, module_name_,
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

void module::set_lua_status(lua_State* lua_state) {
  // Get current module staus from the bot.
  std::map<std::string, std::string> status = bot_->module_status(module_name_);

  // Write the status to the lua script state.
  for(const auto& s : status) {
    // Don't set the active status in lua.
    if (s.first == "active") {
      continue;
    }

    try {
      lua_connection::set_status(lua_state, lua_status_, s.first, s.second);
    } catch(lua_exception) {
      std::stringstream msg;
      msg << "could not set status " << s.first << " = " << s.second;
      bot_->log(bot::BS_LOG_NFO, module_name_, msg.str());
      return;
    }
  }
}

}  // namespace botscript
