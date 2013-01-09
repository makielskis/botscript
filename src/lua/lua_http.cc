// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./lua_http.h"

#include <iostream>
#include <memory>
#ifdef BS_DEBUG
#include <sstream>
#endif

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/bind.hpp"

#include "../http/util.h"
#include "../http/url.h"
#include "../bot_browser.h"
#include "../bot.h"
#include "./lua_connection.h"
#include "./lua_util.h"

namespace botscript {

void lua_http::open(lua_State* state) {
  luaL_newlib(state, httplib);
  lua_setglobal(state, "http");
}

void lua_http::on_req_finish(lua_State* state, std::string response,
                             boost::system::error_code ec
#ifdef BS_DEBUG
                             ,std::string dbg) {
#else
                                             ) {
#endif
  // Check failure.
  if (ec) {
    return lua_connection::on_error(state, ec.message());
  }

  // Get and check callback function.
  lua_getglobal(state, BOT_CALLBACK);
  if (!lua_isfunction(state, -1)) {
    return lua_connection::on_error(state, "on_req_finish: no callback set");
  }

  // The callback is used as indicator: If the callback is set on function exit,
  // the function has called an asynchronous function. Clear our BOT_CALLBACK.
  lua_pushnil(state);
  lua_setglobal(state, BOT_CALLBACK);

  // Call BOT_CALLBACK function.
  lua_pushlstring(state, response.c_str(), response.length());
  try {
    lua_connection::exec(state, 1, LUA_MULTRET, 0);
  } catch(const lua_exception& e) {
    return lua_connection::on_error(state, e.what());
  }

  // Check whether an asynchronous action was startet (BOT_CALLBACK set).
  // If this is NOT the case: call terminated - call on_finish cb the 2nd time.
  // Otherwise (BOT_CALLBACK is set), the asynchronous function will do this.
  lua_getglobal(state, BOT_CALLBACK);
  if (!lua_isfunction(state, -1)) {
    // Check if callback is set.
    lua_getglobal(state, BOT_LOGIN_CB);
    if (!lua_isuserdata(state, -1)) {
      assert(false);
    }

    // Get callback.
    void* p = lua_touserdata(state, -1);
    on_finish_cb* cb = static_cast<on_finish_cb*>(p);
    lua_pop(state, 1);

    // Unset login callback
    lua_pushnil(state);
    lua_setglobal(state, BOT_LOGIN_CB);

    // Call callback function.
    (*cb)("");
  }
}

int lua_http::get(lua_State* state, bool path) {
  // Check if state is finished.
  lua_getglobal(state, BOT_FINISH);
  bool finished = lua_isboolean(state, -1) && lua_toboolean(state, -1);
  lua_pop(state, 1);
  if (finished) {
    return luaL_error(state, "on_finish_error");
  }

  // Check arguments.
  luaL_checktype(state, 1, LUA_TSTRING);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  // Get URL string.
  std::string url = lua_tostring(state, 1);
  lua_remove(state, 1);

  // Set callback as global variable.
  lua_setglobal(state, BOT_CALLBACK);

  // Get the calling bot.
  std::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (std::shared_ptr<bot>() == b) {
    return luaL_error(state, "no bot for state");
  }

#ifdef BS_DEBUG
  lua_Debug ar;
  lua_getstack(state, 1, &ar);
  lua_getinfo(state, "nSl", &ar);
  int line = ar.currentline;
  std::string src = ar.short_src;
  std::stringstream dbg_stream;
  dbg_stream << src << ": " << line;
  std::string debug = dbg_stream.str();
#endif

  // Do asynchronous call.
  url = path ? b->server() + url : url;
  b->browser()->request(http::url(url), http::util::GET, "",
                        boost::bind(on_req_finish, state, _1, _2
#ifdef BS_DEBUG
,debug
#endif
),
                        boost::posix_time::seconds(15), 3);

  return 0;
}

int lua_http::post(lua_State* state, bool path) {
  // Check if state is finished.
  lua_getglobal(state, BOT_FINISH);
  bool finished = lua_isboolean(state, -1) && lua_toboolean(state, -1);
  lua_pop(state, 1);
  if (finished) {
    return luaL_error(state, "on_finish_error");
  }

  // Check arguments.
  luaL_checktype(state, 1, LUA_TSTRING);
  luaL_checktype(state, 2, LUA_TSTRING);
  luaL_checktype(state, 3, LUA_TFUNCTION);

  // Get URL string.
  std::string url = lua_tostring(state, 1);
  std::string content = lua_tostring(state, 2);
  lua_remove(state, 1);
  lua_remove(state, 1);

  // Set callback as global variable.
  lua_setglobal(state, BOT_CALLBACK);

  // Get the calling bot.
  std::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (std::shared_ptr<bot>() == b) {
    return luaL_error(state, "no bot for state");
  }

#ifdef BS_DEBUG
  lua_Debug ar;
  lua_getstack(state, 1, &ar);
  lua_getinfo(state, "nSl", &ar);
  int line = ar.currentline;
  std::string src = ar.short_src;
  std::stringstream dbg_stream;
  dbg_stream << src << ": " << line;
  std::string debug = dbg_stream.str();
#endif

  // Do asynchronous call.
  url = path ? b->server() + url : url;
  b->browser()->request(http::url(url), http::util::POST, content,
                        boost::bind(on_req_finish, state, _1, _2
#ifdef BS_DEBUG
,debug
#endif
),
                        boost::posix_time::seconds(15), 3);

  return 0;
}

int lua_http::get(lua_State* state) {
  return get(state, false);
}

int lua_http::get_path(lua_State* state) {
  return get(state, true);
}

int lua_http::post(lua_State* state) {
  return post(state, false);
}

int lua_http::post_path(lua_State* state) {
  return post(state, true);
}

int lua_http::submit_form(lua_State* state) {
  // Check if state is finished.
  lua_getglobal(state, BOT_FINISH);
  bool finished = lua_isboolean(state, -1) && lua_toboolean(state, -1);
  lua_pop(state, 1);
  if (finished) {
    return luaL_error(state, "on_finish_error");
  }

  // Prepare variables to store the arguments.
  std::string content;
  std::string xpath;
  std::map<std::string, std::string> parameters;
  std::string action;

  // Collect parameters.
  int argc = lua_gettop(state);
  switch (argc) {
    case 5:
      action = luaL_checkstring(state, 4);
    case 4:
      lua_connection::lua_str_table_to_map(state, 3, &parameters);
    case 3:
      xpath = luaL_checkstring(state, 2);
      content = luaL_checkstring(state, 1);
  }

  // Arguments read. Pop them.
  for (int i = 0; i < argc - 1; ++i) {
    lua_remove(state, 1);
  }

  // Set callback as global variable.
  lua_setglobal(state, BOT_CALLBACK);

  // Get the calling bot.
  std::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (std::shared_ptr<bot>() == b) {
    return luaL_error(state, "no bot for state");
  }

#ifdef BS_DEBUG
  lua_Debug ar;
  lua_getstack(state, 1, &ar);
  lua_getinfo(state, "nSl", &ar);
  int line = ar.currentline;
  std::string src = ar.short_src;
  std::stringstream dbg_stream;
  dbg_stream << src << ": " << line;
  std::string debug = dbg_stream.str();
#endif

  // Do asynchronous call.
  auto cb = std::bind(on_req_finish, state,
                      std::placeholders::_1, std::placeholders::_2
#ifdef BS_DEBUG
,debug
#endif
);
  b->io_service()->post(boost::bind(&bot_browser::submit, b->browser(),
                                    xpath, content, parameters, action, cb,
                                    boost::posix_time::seconds(15), 3));

  return 0;
}

}  // namespace botscript
