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

#ifndef MODULE_H_
#define MODULE_H_

#include <string>
#include <map>

#include "boost/utility.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/asio/basic_deadline_timer.hpp"
#include "boost/thread.hpp"

#include "./bot.h"
#include "./lua_connection.h"

namespace botscript {

class bot;

class module : boost::noncopyable {
 public:
  module(const std::string& script, bot* bot, lua_State* main_state,
         boost::asio::io_service* io_service);
  ~module();

  std::string name() { return module_name_; }
  std::string interface_description() { return interface_description_; }

  void shutdown();
  void applyStatus();
  void run();
  void execute(const std::string& command, const std::string& argument);

 private:
  bot* bot_;
  std::map<std::string, std::string> status_;
  std::string module_name_;
  std::string lua_run_;
  std::string lua_status_;
  std::string lua_interface_;
  std::string lua_active_status_;
  std::string interface_description_;
  lua_State* lua_state_;
  boost::asio::io_service* io_service_;
  boost::asio::deadline_timer timer_;
  boost::mutex status_mutex_;
  boost::mutex run_mutex_;
  bool stopping_;
  bool online_;
};

}  // namespace botscript

#endif  // MODULE_H_
