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

#include <sstream>
#include <string>
#include <map>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace botscript {

std::map<std::string, bot*> lua_connection::bots_;
boost::mutex lua_connection::bots_mutex_;
http::webclient* lua_connection::webclient_ = NULL;

jsonval_ptr lua_connection::toJSON(lua_State* state, int stack_index,
    rapidjson::Document::AllocatorType* allocator) {
  switch (lua_type(state, stack_index)) {
    // Convert lua string to JSON string.
    case LUA_TSTRING: {
      const char* str = luaL_checkstring(state, stack_index);
      return jsonval_ptr(new rapidjson::Value(str, *allocator));
    }

    // Convert lua table to JSON object.
    case LUA_TTABLE: {
      // Push the first key (nil).
      lua_pushnil(state);
      stack_index = (stack_index > 0) ? stack_index : stack_index - 1;

      // Iterate keys.
      jsonval_ptr obj = jsonval_ptr(
          new rapidjson::Value(rapidjson::kObjectType));
      while (lua_next(state, stack_index) != 0) {
        rapidjson::Value key(luaL_checkstring(state, -2), *allocator);
        jsonval_ptr val = toJSON(state, -1, allocator);
        obj->AddMember(key, *val.get(), *allocator);

        // Pop the value, key stays for lua_next.
        lua_pop(state, 1);
      }
      return obj;
    }

    // Convert unknown type (i.e. lua thread) to JSON null.
    default: {
      return jsonval_ptr(new rapidjson::Value(rapidjson::kNullType));
    }
  }
}

jsonval_ptr lua_connection::iface(const std::string& script,
    rapidjson::Document::AllocatorType* allocator)
throw(lua_exception) {
  // Initialize lua_State.
  lua_State* state = luaL_newstate();
  if (NULL == state) {
    throw lua_exception("could not create script state");
  }
  luaL_openlibs(state);

  // Execute script.
  int ret;
  if (0 != (ret = luaL_dofile(state, script.c_str()))) {
    std::string error = lua_tostring(state, -1);
    lua_close(state);
    throw lua_exception(error);
  }

  // Discover module name.
  using boost::filesystem::path;
  std::string filename = path(script).filename().generic_string();
  std::string stem = filename.substr(0, filename.length() - 4);

  // Push lua variable to stack.
  lua_getglobal(state, (std::string("interface_") + stem).c_str());

  // Read interface description as rapid-json value.
  jsonval_ptr interface = toJSON(state, -1, allocator);

  // Free lua script resources.
  lua_close(state);

  return interface;
}

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

bool lua_connection::loadServerList(const std::string& script,
    std::map<std::string, std::string>* servers) {
  // Initialize lua_State.
  lua_State* state = luaL_newstate();
  if (NULL == state) {
    return false;
  }
  luaL_openlibs(state);

  // Execute script.
  if (0 != luaL_dofile(state, script.c_str())) {
    lua_close(state);
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
        error = std::string("syntax error ") + lua_tostring(state, -1);
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
  lua_State* state = luaL_newstate();
  if (state == NULL) {
    throw lua_exception("script initialization: out of memory");
  }

  // Register botscript functions.
  lua_register(state, "m_request", m_request);
  lua_register(state, "m_request_path", m_request_path);
  lua_register(state, "m_post_request", m_post_request);
  lua_register(state, "m_post_request_path", m_post_request_path);
  lua_register(state, "m_submit_form", m_submit_form);
  lua_register(state, "m_get_by_xpath", m_get_by_xpath);
  lua_register(state, "m_get_all_by_xpath", m_get_all_by_xpath);
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

void lua_connection::login(bot* bot,
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
  } catch(const lua_exception& e) {
    lua_close(state);
    throw(e);
  }

  // Get result, free resources and return.
  if (!lua_toboolean(state, -1)) {
    lua_close(state);
    throw bad_login_exception();
  }

  lua_close(state);
}

void lua_connection::get_status(lua_State* state, const std::string& var,
                                std::map<std::string, std::string>* status) {
  // Clear stack.
  lua_pop(state, lua_gettop(state));

  // Get and check state.
  lua_getglobal(state, var.c_str());
  if (!lua_istable(state, 1)) {
    // Recreate.
    lua_newtable(state);
    lua_setglobal(state, var.c_str());

    // Clear stack.
    lua_pop(state, lua_gettop(state));

    // Get table.
    lua_getglobal(state, var.c_str());
  }

  // Read table.
  luaStringTableToMap(state, 1, status);
}

void lua_connection::set_status(lua_State* state,
                                const std::string& var,
                                const std::string& key,
                                const std::string& value) {
  // Get and check state.
  lua_getglobal(state, var.c_str());
  if (!lua_istable(state, -1)) {
    throw lua_exception("module status is not a table");
  }

  // Set new value.
  lua_pushstring(state, key.c_str());
  lua_pushstring(state, value.c_str());
  lua_settable(state, -3);
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

http::webclient* lua_connection::getWebClient(lua_State* state) {
  return webclient_ != NULL ? webclient_ : getBot(state)->webclient();
}

void lua_connection::remove(const std::string& identifier) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Search for identifier and delete entry.
  std::map<std::string, bot*>::iterator i = bots_.find(identifier);
  if (i != bots_.end()) {
    bots_.erase(i);
  }
}

bool lua_connection::contains(const std::string& identifier) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Return whether the bot map contains the specified identifier
  return bots_.find(identifier) != bots_.end();
}

void lua_connection::log(lua_State* state, int log_level) {
  bot* bot = getBot(state);

  // Get module name
  lua_getglobal(state, BOT_MODULE);
  std::string module = luaL_checkstring(state, -1);
  lua_pop(state, 1);

  // Get and log message
  std::string message = luaL_checkstring(state, 1);
  bot->log(log_level, module, message);
}

int lua_connection::doRequest(lua_State* state, bool path) {
  std::string url = luaL_checkstring(state, 1);

  std::string response;
  try {
    if (path) {
      bot* bot = getBot(state);
      http::webclient* wc = bot->webclient();
      response = wc->request_get(bot->server() + url);
    } else {
      response = getWebClient(state)->request_get(url);
    }
  } catch(const std::ios_base::failure& e) {
    return luaL_error(state, "#con %s", e.what());
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

int lua_connection::doPostRequest(lua_State* state, bool path) {
  // Read arguments.
  std::string url = luaL_checkstring(state, 1);
  std::string data luaL_checkstring(state, 2);

  // Do request.
  std::string response;
  try {
    if (path) {
      bot* bot = getBot(state);
      http::webclient* wc = bot->webclient();
      response = wc->request_post(bot->server() + url,
          reinterpret_cast<const char*>(data.c_str()), data.length());
    } else {
      response = getWebClient(state)->request_post(url,
          reinterpret_cast<const char*>(data.c_str()), data.length());
    }
  } catch(const std::ios_base::failure& e) {
    return luaL_error(state, "#con %s", e.what());
  }
  lua_pushstring(state, response.c_str());

  // Return result.
  lua_pushstring(state, response.c_str());
  return 1;
}

int lua_connection::m_post_request(lua_State* state) {
  return doPostRequest(state, false);
}

int lua_connection::m_post_request_path(lua_State* state) {
  return doPostRequest(state, true);
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
    http::webclient* wc = bot->webclient();
    response = wc->submit(xpath, content, parameters, action);
  } catch(const std::ios_base::failure& e) {
    return luaL_error(state, "#con %s", e.what());
  } catch(const http::element_not_found_exception& e) {
    return luaL_error(state, "#nof %s", e.what());
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
  } catch(const pugi::xpath_exception& e) {
    std::string error = xpath;
    error += " is not a valid xpath";
    return luaL_error(state, "%s", error.c_str());
  }

  // Return result.
  lua_pushstring(state, value.c_str());
  return 1;
}

int lua_connection::m_get_all_by_xpath(lua_State* state) {
  // Get arguments from stack.
  std::string str = luaL_checkstring(state, 1);
  const char* xpath = luaL_checkstring(state, 2);

  // Use pugi for xpath query.
  try {
    // Result table and match index.
    lua_newtable(state);
    int matchIndex = 1;

    pugi::xml_document doc;
    doc.load(str.c_str());
    pugi::xpath_query query(xpath);
    pugi::xpath_node_set result = query.evaluate_node_set(doc);
    for (pugi::xpath_node_set::const_iterator i = result.begin();
       i != result.end(); ++i) {
      // Push match index:
      // the index for this match in the result table
      lua_pushnumber(state, matchIndex++);

      // Get node xml content
      std::stringstream s;
      i->node().print(s);

      // Insert value to table
      lua_pushstring(state, s.str().c_str());

      // Insert match to the result table.
      lua_rawset(state, -3);
    }
  } catch(const pugi::xpath_exception& e) {
    std::string error = xpath;
    error += " is not a valid xpath";
    return luaL_error(state, "%s", error.c_str());
  }

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
  } catch(const boost::regex_error& e) {
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
    int matchIndex = 1;

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
  } catch(const boost::regex_error& e) {
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
