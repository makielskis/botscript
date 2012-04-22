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

#ifndef LUA_CONNECTION_H_
#define LUA_CONNECTION_H_

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <map>
#include <string>

#include "boost/thread.hpp"

#include "./webclient.h"
#include "./bot.h"
#include "./exceptions/lua_exception.h"
#include "./exceptions/bad_login_exception.h"

#define BOT_IDENTIFER "bot_identifier"
#define BOT_MODULE "bot_module"

namespace botscript {

class bot;

class lua_connection {
 public:
  static void luaStringTableToMap(lua_State* state, int stack_index,
                           std::map<std::string, std::string>* map);

  static bool loadServerList(const std::string script,
                             std::map<std::string, std::string>* servers);

  static void loadScript(lua_State* state, std::string script_path)
  throw(lua_exception);

  static void callFunction(lua_State* state,
                           int nargs, int nresults, int errfunc)
  throw(lua_exception);

  static void executeScript(std::string script_path, lua_State* state)
  throw(lua_exception);

  static lua_State* newState()
  throw(lua_exception);

  static lua_State* newState(std::string module_name, bot* bot)
  throw(lua_exception);

  static lua_State* login(bot* bot,
                          const std::string& username,
                          const std::string& password,
                          const std::string& package)
  throw(lua_exception, bad_login_exception);

  static void get_status(lua_State* state, const std::string& var,
                  std::map<std::string, std::string>* status)
  throw(lua_exception);

  static void set_status(lua_State* state, const std::string& var,
                         const std::string& key, const std::string& value);                             

  static void new_environment(lua_State* state);

  static bot* getBot(lua_State* state);
  static webclient* getWebClient(lua_State* state);
  static void remove(const std::string identifier);

  static void log(lua_State* state, int log_level);
  static int doRequest(lua_State* state, bool path);

  static int m_request(lua_State* state);
  static int m_request_path(lua_State* state);
  static int m_post_request(lua_State* state);
  static int m_submit_form(lua_State* state);
  static int m_get_by_xpath(lua_State* state);
  static int m_get_by_regex(lua_State* state);
  static int m_get_all_by_regex(lua_State* state);
  static int m_get_location(lua_State* state);
  static int m_log(lua_State* state);
  static int m_log_error(lua_State* state);
  static int m_set_status(lua_State* state);

 private:
  static std::map<std::string, bot*> bots_;
  static boost::mutex bots_mutex_;
  static webclient* webclient_;
};

}  // namespace botscript

#endif  // LUA_CONNECTION_H_
