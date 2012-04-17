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

#include "./lua_connection.h"

#include <string>
#include <map>

#include "./webclient.h"

namespace botscript {

std::map<std::string, bot*> lua_connection::bots_;
boost::mutex lua_connection::bots_mutex_;
webclient* lua_connection::webclient_ = NULL;

void lua_connection::luaStringTableToMap(lua_State* state, int stack_index,
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

bool lua_connection::loadServerList(const std::string script,
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

void lua_connection::loadScript(lua_State* state, std::string script_path)
throw(lua_exception) {
  int ret = 0;
  if (0 != (ret = luaL_loadfile(state, script_path.c_str()))) {
    std::string error;
    switch (ret) {
      case LUA_ERRFILE:
        error = std::string("could not open ") + script_path;
        break;
      case LUA_ERRSYNTAX:
        error = "syntax error";
        break;
      case LUA_ERRMEM:
        error = "out of memory";
        break;
    }
    throw lua_exception(error);
  }
}

void lua_connection::callFunction(lua_State* state,
                                  int nargs, int nresults, int errfunc)
throw(lua_exception) {
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

void lua_connection::executeScript(std::string script_path, lua_State* state)
throw(lua_exception) {
  loadScript(state, script_path);
  callFunction(state, 0, LUA_MULTRET, 0);
}

lua_State* lua_connection::newState() throw(lua_exception) {
  // Create new script state.
  lua_State* state = lua_open();
  if (state == NULL) {
    throw lua_exception("script initialization: out of memory");
  }

  // Register botscript functions.
  lua_register(state, "m_request", m_request);
  lua_register(state, "m_request_path", m_request_path);
  lua_register(state, "m_post_request", m_post_request);
  lua_register(state, "m_submit_form", m_submit_form);
  lua_register(state, "m_get_by_xpath", m_get_by_xpath);
  lua_register(state, "m_get_by_regex", m_get_by_regex);
  lua_register(state, "m_get_all_by_regex", m_get_all_by_regex);
  lua_register(state, "m_log", m_log);
  lua_register(state, "m_log_error", m_log_error);
  lua_register(state, "m_set_status", m_set_status);

  // Load libraries.
  luaL_openlibs(state);

  return state;
}

lua_State* lua_connection::newState(std::string module_name,
                                    bot* bot) throw(lua_exception) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Open a new lua_State* with m_ and libraries already loaded.
  lua_State* state = newState();

  // Set bot identifier.
  bots_[bot->identifier()] = bot;
  lua_pushstring(state, bot->identifier().c_str());
  lua_setglobal(state, BOT_IDENTIFER);

  // Set module name.
  lua_pushstring(state, module_name.c_str());
  lua_setglobal(state, BOT_MODULE);

  return state;
}

lua_State* lua_connection::login(bot* bot,
                                 const std::string& username,
                                 const std::string& password,
                                 const std::string& package)
throw(lua_exception, bad_login_exception) {
  // Load login script.
  lua_State* state = newState("base", bot);
  executeScript(package + "/base.lua", state);

  // Tell Lua to call the login function.
  lua_getglobal(state, "login");
  lua_pushstring(state, username.c_str());
  lua_pushstring(state, password.c_str());

  try {
    // Call login function.
    callFunction(state, 2, 1, 0);
  } catch(lua_exception& e) {
    lua_close(state);
    throw(e);
  }

  // Get result, free resources and return.
  if (!lua_toboolean(state, -1)) {
    lua_close(state);
    throw bad_login_exception();
  }

  return state;
}

bot* lua_connection::getBot(lua_State* state) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  lua_getglobal(state, BOT_IDENTIFER);
  const char* identifier = luaL_checkstring(state, -1);

  // Check for errors.
  if (identifier == NULL || bots_.find(identifier) == bots_.end()) {
    std::string error = "could not get the associated bot for ";
    error += identifier;
    luaL_error(state, "%s", error.c_str());
    return NULL;
  }

  bot* bot = bots_[identifier];
  lua_pop(state, 1);
  return bot;
}

webclient* lua_connection::getWebClient(lua_State* state) {
  return webclient_ != NULL ? webclient_ : getBot(state)->webclient();
}

void lua_connection::remove(const std::string identifier) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Search for identifier and delete entry.
  std::map<std::string, bot*>::iterator i = bots_.find(identifier);
  if (i != bots_.end()) {
    bots_.erase(i);
  }
}

void lua_connection::log(lua_State* state, int log_level) {
  bot* bot = getBot(state);

  // Get module name
  lua_getglobal(state, BOT_MODULE);
  std::string module = luaL_checkstring(state, -1);
  lua_pop(state, 1);

  // Get and log message
  std::string message = luaL_checkstring(state, 1);
  bot->log(log_msg(0, log_level, message));
}

int lua_connection::doRequest(lua_State* state, bool path) {
  std::string url = luaL_checkstring(state, 1);

  std::string response;
  try {
    if (path) {
      bot* bot = getBot(state);
      webclient* wc  = bot->webclient();
      response = wc->request_get(bot->server() + url);
    } else {
      response = getWebClient(state)->request_get(url);
    }
  } catch(std::ios_base::failure& e) {
    return luaL_error(state, "%s", e.what());
  }

  lua_pushstring(state, response.c_str());
  return 1;
}

int lua_connection::m_request(lua_State* state) {
  return doRequest(state, false);
}

int lua_connection::m_request_path(lua_State* state) {
  return doRequest(state, true);
}

int lua_connection::m_post_request(lua_State* state) {
  // Read arguments.
  std::string url = luaL_checkstring(state, 1);
  std::string data luaL_checkstring(state, 2);

  // Do request.
  std::string response;
  try {
    response = getWebClient(state)->request_post(url,
                                                 (char*) data.c_str(),
                                                 data.length());
  } catch (std::ios_base::failure& e) {
    return luaL_error(state, "%s", e.what());
  }
  lua_pushstring(state, response.c_str());

  // Return result.
  lua_pushstring(state, response.c_str());
  return 1;
}

int lua_connection::m_submit_form(lua_State* state) {
  int argc = lua_gettop(state);
  std::string content;
  std::string xpath;
  std::map<std::string, std::string> parameters;
  std::string action;

  // Collect parameters.
  switch (argc) {
    case 4:
      action = luaL_checkstring(state, 4);
    case 3:
      luaStringTableToMap(state, 3, &parameters);
    case 2:
      xpath = luaL_checkstring(state, 2);
      content = luaL_checkstring(state, 1);
  }

  // Do request.
  std::string response;
  try {
      bot* bot = getBot(state);
      webclient* wc = bot->webclient();
      response = wc->submit(xpath, content, parameters, action);
  } catch (std::ios_base::failure& e) {
    return luaL_error(state, "%s", e.what());
  } catch (element_not_found_exception& e) {
    return luaL_error(state, "%s", e.what());
  }

  // Return result.
  lua_pushstring(state, response.c_str());
  return 1;
}

int lua_connection::m_get_by_xpath(lua_State* state) {
  // Get arguments from stack.
  std::string str = luaL_checkstring(state, 1);
  const char* xpath = luaL_checkstring(state, 2);

  // Use pugi for xpath query.
  std::string value;
  try {
    pugi::xml_document doc;
    doc.load(str.c_str());
    pugi::xpath_query query(xpath);
    value = query.evaluate_string(doc);
  } catch (pugi::xpath_exception& e) {
    std::string error = xpath;
    error += " is not a valid xpath";
    return luaL_error(state, "%s", error.c_str());
  }

  // Return result.
  lua_pushstring(state, value.c_str());
  return 1;
}

int lua_connection::m_get_by_regex(lua_State* state) {
  // get arguments from stack
  std::string str = luaL_checkstring(state, 1);
  const char* regex = luaL_checkstring(state, 2);

  std::string match;
  try {
    // apply regular expression
    boost::regex r(regex);
    boost::match_results<std::string::const_iterator> what;
    boost::regex_search(str, what, r);
    match = what.size() > 1 ? what[1].str().c_str() : "";
  } catch (boost::regex_error& e) {
    std::string error = regex;
    error += " is not a valid regex";
    return luaL_error(state, "%s", error.c_str());
  }

  // return result
  lua_pushstring(state, match.c_str());
  return 1;
}

int lua_connection::m_get_all_by_regex(lua_State* state) {
  // Get arguments from stack.
  std::string str = luaL_checkstring(state, 1);
  const char* regex = luaL_checkstring(state, 2);

  try {
    // Result table and match index.
    lua_newtable(state);
    int matchIndex = 0;

    // Prepare for search:
    // results structure, flags, start, end and compiled regex
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;
    std::string::const_iterator start = str.begin();
    std::string::const_iterator end = str.end();
    boost::regex r(regex);
    
    // Search:
    while (boost::regex_search(start, end, what, r, flags)) {
      // Push match index:
      // the index for this match in the result table
      lua_pushnumber(state, matchIndex++);
      
      // Build group table.
      lua_newtable(state);
      for (unsigned i = 1; i <= what.size(); i++) {
        lua_pushnumber(state, i);
        lua_pushstring(state, what[i].str().c_str());
        lua_rawset(state, -3);
      }
      
      // Insert match to the result table.
      lua_rawset(state, -3);

      // Update search position and flags.
      start = what[0].second;
      flags |= boost::match_prev_avail;
      flags |= boost::match_not_bob;
    }
    
    return 1;
  } catch (boost::regex_error& e) {
    std::string error = regex;
    error += " is not a valid regex";
    return luaL_error(state, "%s", error.c_str());
  }
}

int lua_connection::m_log(lua_State* state) {
  log(state, 0);
  return 0;
}

int lua_connection::m_log_error(lua_State* state) {
  log(state, 1);
  return 0;
}

int lua_connection::m_set_status(lua_State* state) {
  bot* bot = getBot(state);

  // Get module name.
  lua_getglobal(state, BOT_MODULE);
  std::string module = luaL_checkstring(state, -1);

  // Get key and value.
  std::string key = luaL_checkstring(state, -3);
  std::string value = luaL_checkstring(state, -2);

  // Execute set command.
  bot->execute(module + "_set_" + key, value);
  return 0;
}

}  // namespace botscript
