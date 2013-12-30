// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef LUA_HTTP_H_
#define LUA_HTTP_H_

#include <string>

#include "./lua_connection.h"

struct lua_State;

namespace botscript {

class lua_http {
 public:
  static void open(lua_State* state);

  static int get(lua_State* state, bool path);
  static int post(lua_State* state, bool path);

  static int get(lua_State* state);
  static int get_path(lua_State* state);
  static int post(lua_State* state);
  static int post_path(lua_State* state);
  static int submit_form(lua_State* state);

 private:
  static void on_req_finish(lua_State* state, std::string response,
                            boost::system::error_code ec);
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
