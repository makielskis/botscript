/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 24. November 2012  makielskis@gmail.com
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

#ifndef LUA_UTIL_H_
#define LUA_UTIL_H_

#define LOGIN_CALLBACK ("LOGIN_CALLBACK")

#include "./lua_connection.h"

namespace botscript {

class lua_util {
 public:
  static void open(lua_State* state);

  static void log(lua_State* state, int log_level);

  static int get_by_xpath(lua_State* state);
  static int get_all_by_xpath(lua_State* state);
  static int get_by_regex(lua_State* state);
  static int get_all_by_regex(lua_State* state);
  static int log_debug(lua_State* state);
  static int log(lua_State* state);
  static int log_error(lua_State* state);
  static int set_status(lua_State* state);
};

static const luaL_Reg utillib[] = {
  {"get_by_xpath",     lua_util::get_by_xpath},
  {"get_all_by_xpath", lua_util::get_all_by_xpath},
  {"get_by_regex",     lua_util::get_by_regex},
  {"get_all_by_regex", lua_util::get_all_by_regex},
  {"log_debug",        lua_util::log_debug},
  {"log",              lua_util::log},
  {"log_error",        lua_util::log_error},
  {"set_status",       lua_util::set_status},
  {NULL, NULL}
};

}  // namespace botscript

#endif  // LUA_UTIL_H_
