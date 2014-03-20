// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef LUA_UTIL_H_
#define LUA_UTIL_H_

#define LOGIN_CALLBACK ("LOGIN_CALLBACK")

#include "./lua_connection.h"

struct lua_State;

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
  static int set_global(lua_State* state);
  static int get_global(lua_State* state);
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
  {"set_global",       lua_util::set_global},
  {"get_global",       lua_util::get_global},
  {NULL, NULL}
};

}  // namespace botscript

#endif  // LUA_UTIL_H_
