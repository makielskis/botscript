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
 * but WITHOUT ANY WARRANTY;without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "./lua_http.h"

#include <iostream>

#include "boost/lambda/lambda.hpp"
#include "boost/bind.hpp"

#include "./lua_connection.h"

#define CALLBACK ("CALLBACK")

namespace botscript {

void lua_http::callback(lua_State* state, bool cb_on_fail,
                        const std::string& response, bool success) {
  // Get the calling bot.
  bot* b = lua_connection::bot(state);
  if (nullptr == m) {
    return;
  }

  // Check success.
  if (!success && !cb_on_fail) {
    lua_connection::log(state, BS_LOG_ERR, "request failed: " + response);
    return;
  }

  // Get and check callback function.
  lua_getglobal(state, CALLBACK);
  if (lua_isfunction(state, -1)) {
    lua_connection::log(state, BS_LOG_ERR, "callback not defined");
    return;
  }

  // Call callback function.
  lua_pushstring(state, response.c_str());
  lua_pushboolean(state, success);
  try {
    lua_connection::call(state, 2, LUA_MULTRET, 0);
  } catch(const lua_exception& e) {
    lua_connection::log(state, BS_LOG_ERR, "callback error: " + e.what());
    if (lua_connection::is_login(state)) {
      bot->login_callback();
    }
  }
}

int lua_http::sync_get(lua_State* state, bool path) {
  return 0;
}

int lua_http::async_get(lua_State* state, bool path) {
  // Read callback on failure flag.
  bool cb_on_fail = false;
  if (lua_gettop(state) == 3) {
    luaL_checktype(state, 3, LUA_TBOOLEAN);
    cb_on_fail = lua_toboolean(state, 3);
    lua_pop(state, 1);
  }

  // Check arguments.
  luaL_checktype(state, 1, LUA_TSTRING);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  // Get string.
  std::string url = lua_tostring(state, 1);
  lua_remove(state, 1);

  // Set callback as global variable.
  lua_setglobal(state, CALLBACK);

  // Get webclient.
  http::webclient* wc = webclient(state);
  if (nullptr == wc) {
    return luaL_error(state, "no bot for state");
  }

  // Do asynchronous call.
  wc->async_get(url, boost::bind(http_response, state, cb_on_fail, _1, _2));

  return 0;
}

int lua_http::sync_post(lua_State* state, bool path) {
  return 0;
}

int lua_http::async_post(lua_State* state, bool path) {
  return 0;
}

int lua_http::sync_submit_form(lua_State* state, bool path) {
  return 0;
}

int lua_http::async_submit_form(lua_State* state, bool path) {
  return 0;
}

int lua_http::get(lua_State* state) {
  if (lua_isfunction(state, -1)) {
    return async_get(state, false);
  } else {
    return sync_get(state, false);
  }
}

int lua_http::get_path(lua_State* state) {
  if (lua_isfunction(state, -1)) {
    return async_get(state, true);
  } else {
    return sync_get(state, true);
  }
}

int lua_http::post(lua_State* state) {
  if (lua_isfunction(state, -1)) {
    return async_post(state, false);
  } else {
    return sync_post(state, false);
  }
}

int lua_http::post_path(lua_State* state) {
  if (lua_isfunction(state, -1)) {
    return async_post(state, true);
  } else {
    return sync_post(state, true);
  }
}

int lua_http::submit_form(lua_State* state) {
  if (lua_isfunction(state, -1)) {
    return async_submit_form(state, false);
  } else {
    return sync_submit_form(state, false);
  }
}

int lua_http::submit_form_path(lua_State* state) {
  if (lua_isfunction(state, -1)) {
    return async_submit_form(state, true);
  } else {
    return sync_submit_form(state, true);
  }
}

}  // namespace botscript
