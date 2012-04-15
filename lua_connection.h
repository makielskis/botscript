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

#include <map>
#include <string>
#include <map>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "./bot.h"

namespace botscript {

namespace lua {

#define BOT_IDENTIFER "bot_identifier"
#define BOT_MODULE "bot_module"

std::map<std::string, botscript::bot*> bots = std::map<std::string, botscript::bot*>();

class bad_login_exception : public std::exception {
};

class lua_exception : public std::exception {
 public:
  lua_exception(const std::string& what)
    : error(what) {
  }

  ~lua_exception() throw() {
  }

  virtual const char* what() const throw() {
    return error.c_str();
  }

 private:
  std::string error;
};

botscript::bot* getBot(lua_State* state) {
  lua_getglobal(state, BOT_IDENTIFER);
  const char* identifier = luaL_checkstring(state, -1);

  // Check for errors.
  if (identifier == NULL || bots.find(identifier) == bots.end()) {
    std::string error = "could not get the associated bot for ";
    error += identifier;
    luaL_error(state, "%s", error.c_str());
    return NULL;
  }

  botscript::bot* bot = bots[identifier];
  lua_pop(state, 1);
  return bot;
}

void luaStringTableToMap(lua_State* state, int stack_index,
                         std::map<std::string, std::string>* map) {
  // Check - check - double check!
  if (!lua_istable(state, stack_index)) {
    return;
  }

  // Push the first key (nil).
  lua_pushnil(state);

  // Iterate keys.
  while (lua_next(state, stack_index) != 0) {
    const char* key = luaL_checkstring(state, -2);
    const char* value = luaL_checkstring(state, -1);
    (*map)[key] = value;

    // Pop the value, key stays for lua_next.
    lua_pop(state, 1);
  }
}

bool loadServerList(const std::string script,
                    std::map<std::string, std::string>* servers) {
  // Initialize lua_State.
  lua_State* state = lua_open();
  if (NULL == state) {
    return false;
  }
  luaL_openlibs(state);

  // Execute script.
  if (0 != luaL_dofile(state, script.c_str())) {
    return false;
  }

  // Read servers list.
  lua_getglobal(state, "servers");
  luaStringTableToMap(state, 1, servers);

  // Free resources.
  lua_close(state);

  return true;
}

void loadScript(lua_State* state, std::string script_path) throw (lua_exception) {
  int ret = 0;
  if (0 != (ret = luaL_loadfile(state, script_path.c_str()))) {
    std::string error;
    switch (ret) {
      case LUA_ERRFILE: error = std::string("could not open ") + script_path;
	      break;
      case LUA_ERRSYNTAX: error = "syntax error";
	      break;
      case LUA_ERRMEM: error = "out of memory";
	      break;
    }
    throw lua_exception(error);
  }
}

void callFunction(lua_State* state,
                  int nargs, int nresults, int errfunc)
throw (lua_exception) {
    int ret;
    if (0 != (ret = lua_pcall(state, nargs, nresults, errfunc))) {
      std::string error;

      const char* s = lua_tostring(state, -1);

      if (s == NULL) {
        switch (ret) {
          case LUA_ERRRUN: error = "runtime error"; break;
          case LUA_ERRMEM: error = "out of memory"; break;
          case LUA_ERRERR: error = "error handling error"; break;
        }
      } else {
        error = s;
      }

      throw lua_exception(error);
    }
}

void executeScript(std::string script_path, lua_State* state)
throw (lua_exception) {
	loadScript(state, script_path);
	callFunction(state, 0, LUA_MULTRET, 0);
}

lua_State* newState() throw (lua_exception) {
	// Create new script state.
	lua_State* state = lua_open();
	if (state == NULL) {
		throw lua_exception("script initialization: out of memory");
	}

/*
	// Register botscript functions.
	lua_register(state, "m_request", m_request);
	lua_register(state, "m_request_path", m_request_path);
	lua_register(state, "m_post_request", m_post_request);
	lua_register(state, "m_submit_form", m_submit_form);
	lua_register(state, "m_submit_form_path", m_submit_form_path);
	lua_register(state, "m_get_by_xpath", m_get_by_xpath);
	lua_register(state, "m_get_by_regex", m_get_by_regex);
	lua_register(state, "m_get_all_by_regex", m_get_all_by_regex);
	lua_register(state, "m_log", m_log);
	lua_register(state, "m_log_error", m_log_error);
	lua_register(state, "m_set_status", m_set_status);
*/

	// Load libraries.
	luaL_openlibs(state);

	return state;
}

lua_State* newState(std::string module_name, botscript::bot* bot)
throw (lua_exception) {
  // Open a new lua_State* with m_ and libraries already loaded.
  lua_State* state = newState();

  // Set bot identifier.
  bots[bot->identifier()] = bot;
  lua_pushstring(state, bot->identifier().c_str());
  lua_setglobal(state, BOT_IDENTIFER);

  // Set module name.
  lua_pushstring(state, module_name.c_str());
  lua_setglobal(state, BOT_MODULE);

  return state;
}

lua_State* login(botscript::bot* bot,
                 const std::string& username,
                 const std::string& password,
                 const std::string& package)
throw (lua_exception, bad_login_exception) {
  // Load login script.
  lua_State* state = newState("base", bot);
  executeScript(package + "/base.lua", state);

  // Tell Lua to call the login function.
  lua_getglobal(state, "login");
  lua_pushstring(state, username.c_str());
  lua_pushstring(state, password.c_str());

  // Call login function.
  callFunction(state, 2, 1, 0);

  // Get result, free resources and return.
  if (!lua_toboolean(state, -1)) {
    throw bad_login_exception();
  }

  return state;
}

} }  // namespace botscript, lua

#endif  // LUA_CONNECTION_H_
