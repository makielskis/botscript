// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef LUA_HTTP_H_
#define LUA_HTTP_H_

#include <string>

#include "./lua_connection.h"

namespace botscript {

class lua_http {
 public:
  static void open(lua_State* state);

  static int sync_get(lua_State* state, bool path);
  static int async_get(lua_State* state, bool path);
  static int sync_post(lua_State* state, bool path);
  static int async_post(lua_State* state, bool path);
  static int sync_submit_form(lua_State* state);
  static int async_submit_form(lua_State* state);

  static int get(lua_State* state);
  static int get_path(lua_State* state);
  static int post(lua_State* state);
  static int post_path(lua_State* state);
  static int submit_form(lua_State* state);

 private:
  static void on_req_finish(lua_State* state, bool cb_on_fail,
                            const std::string& response, bool success);
};

static const luaL_Reg httplib[] = {
  {"get",              lua_http::get},
  {"get_path",         lua_http::get_path},
  {"post",             lua_http::post},
  {"post_path",        lua_http::post_path},
  {"submit_form",      lua_http::submit_form},
  {NULL, NULL}
};

}  // namespace botscript

#endif  // LUA_HTTP_H_
