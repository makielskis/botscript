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

#include "../http/webclient.h"
#include "../bot.h"
#include "./lua_connection.h"
#include "./lua_util.h"

namespace botscript {

void lua_http::open(lua_State* state) {
    luaL_newlib(state, httplib);
    lua_setglobal(state, "http");
}

void lua_http::on_req_finish(lua_State* state, bool fail_cb,
                             const std::string& response, bool success) {
  // Get the calling bot.
  boost::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (boost::shared_ptr<bot>() == b) {
    return;
  }

  // Get module name (for logging).
  lua_getglobal(state, BOT_MODULE);
  std::string module = luaL_checkstring(state, -1);
  lua_pop(state, 1);

  // Check success.
  if (!success && !fail_cb) {
    b->log(bot::BS_LOG_ERR, module, "request failed: " + response);
    return;
  }

  // Get and check callback function.
  lua_getglobal(state, BOT_CALLBACK);
  if (!lua_isfunction(state, -1)) {
    b->log(bot::BS_LOG_ERR, module, "callback not defined");
    return;
  }

  // The callback is used as indicator: If the callback is set on function exit,
  // the function has called an asynchronous function. So the lua_State should
  // NOT get closed until this function has finished executing.
  lua_pushnil(state);
  lua_setglobal(state, BOT_CALLBACK);

  // Call callback function.
  lua_pushlstring(state, response.c_str(), response.length());
  lua_pushboolean(state, success);
  try {
    lua_connection::exec(state, 2, LUA_MULTRET, 0);
  } catch(const lua_exception& e) {
    b->log(bot::BS_LOG_ERR, module, std::string("callback error: ") + e.what());
  }

  // Check whether an asynchronous action was startet (callback set).
  // If this is NOT the case: close state.
  // Otherwise, the asynchronous function is responsible to handle the lifetime.
  lua_getglobal(state, BOT_CALLBACK);
  if (!lua_isfunction(state, -1)) {
    lua_close(state);
  }
}

int lua_http::sync_get(lua_State* state, bool path) {
  return 0;
}

int lua_http::async_get(lua_State* state, bool path) {
  // Check arguments.
  luaL_checktype(state, 1, LUA_TSTRING);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  // Get URL string.
  std::string url = lua_tostring(state, 1);
  lua_remove(state, 1);

  // Set callback as global variable.
  lua_setglobal(state, BOT_CALLBACK);

  // Get the calling bot.
  boost::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (boost::shared_ptr<bot>() == b) {
    return luaL_error(state, "no bot for state");
  }

  // Do asynchronous call.
  url = path ? b->server() + url : url;
  b->webclient()->async_get(url, boost::bind(on_req_finish,
                                             state, false, _1, _2));

  return 0;
}

int lua_http::sync_post(lua_State* state, bool path) {
  return 0;
}

int lua_http::async_post(lua_State* state, bool path) {
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
  boost::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (boost::shared_ptr<bot>() == b) {
    return luaL_error(state, "no bot for state");
  }

  // Do asynchronous call.
  url = path ? b->server() + url : url;
  b->webclient()->async_post(url, content, boost::bind(on_req_finish,
                                                       state, false, _1, _2));

  return 0;
}

int lua_http::sync_submit_form(lua_State* state) {
  return 0;
}

int lua_http::async_submit_form(lua_State* state) {
  // Prepare variables to store the arguments.
  std::string content;
  std::string xpath;
  std::map<std::string, std::string> parameters;
  std::string action;

  // Collect parameters.
  int argc = lua_gettop(state);
  switch (argc) {
    case 5: {
      if (!lua_isstring(state, 4)) {
        return luaL_error(state, "action must be a string");
      }

      size_t action_length = 0;
      const char* s = lua_tolstring(state, 4, &action_length);
      action.resize(action_length);
      memcpy(&(action[0]), s, action_length);
    }
    case 4: {
      lua_connection::lua_str_table_to_map(state, 3, &parameters);
    }
    case 3: {
      if (!lua_isstring(state, 2)) {
        return luaL_error(state, "xpath must be a string");
      }

      if (!lua_isstring(state, 1)) {
        return luaL_error(state, "content must be a string");
      }

      size_t xpath_length = 0;
      const char* s = lua_tolstring(state, 2, &xpath_length);
      xpath.resize(xpath_length);
      memcpy(&(xpath[0]), s, xpath_length);

      size_t content_length = 0;
      s = lua_tolstring(state, 1, &content_length);
      content.resize(content_length);
      memcpy(&(content[0]), s, content_length);
    }
  }

  // Arguments read. Pop them.
  for (int i = 0; i < argc - 1; ++i) {
    lua_remove(state, 1);
  }

  // Set callback as global variable.
  lua_setglobal(state, BOT_CALLBACK);

  // Get the calling bot.
  boost::shared_ptr<bot> b = lua_connection::get_bot(state);
  if (boost::shared_ptr<bot>() == b) {
    return luaL_error(state, "no bot for state");
  }

  // Do asynchronous call.
  http::http_source::async_callback cb = boost::bind(on_req_finish, state, false, _1, _2);

  try {
    b->webclient()->async_submit("//form[starts-with(@action, '/login.html;jsessionid=')]", "<form action=\"/login.html;jsessionid=1ACF42D3F3D6EF5244DF5A8F4B8F1302.appserver004\" method=\"post\"><input type=\"hidden\" value=\"\" name=\"target\"/><input type=\"hidden\" value=\"true\" name=\"login\"/><input name=\"eid\" value=\"\" type=\"hidden\" /><input id=\"username\" maxlength=\"50\" name=\"username\" value=\"\" class=\"login_input\" type=\"text\" /><input id=\"password\" maxlength=\"30\" name=\"password\" value=\"\" class=\"login_input\" type=\"password\" /><input type=\"hidden\" name=\"_sourcePage\" value=\"gX_5dvdMOnOoz3Nj9RlO61tEmZmjeHsX9mpp0iiwtQOUZMFrFmYwDK3coCBLaVfE\" /><input type=\"hidden\" name=\"__fp\" value=\"SnaGnAjU2rs=\" /></form>", std::map<std::string, std::string>(), "", cb);
  } catch(const http::element_not_found_exception& e) {
    return luaL_error(state, e.what());
  }

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
    return async_submit_form(state);
  } else {
    return sync_submit_form(state);
  }
}

}  // namespace botscript
