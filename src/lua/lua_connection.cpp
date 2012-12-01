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

jsonval_ptr lua_connection::iface(const std::string& script,
    rapidjson::Document::AllocatorType* allocator)
throw(lua_exception) {
  // Initialize lua_State.
  lua_State* state = luaL_newstate();
  if (nullptr == state) {
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
  jsonval_ptr interface = to_json(state, -1, allocator);

  // Free lua script resources.
  lua_close(state);

  return interface;
}

bool lua_connection::server_list(const std::string& script,
    std::map<std::string, std::string>* servers) {
  // Initialize lua_State.
  lua_State* state = luaL_newstate();
  if (nullptr == state) {
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
  lua_str_table_to_map(state, 1, servers);

  // Free resources.
  lua_close(state);

  return true;
}

lua_State* lua_connection::new_state() throw(lua_exception) {
  // Create new script state.
  lua_State* state = luaL_newstate();
  if (nullptr == state) {
    throw lua_exception("script initialization: out of memory");
  }

  // Register botscript functions.
  lua_http::open(state);
  lua_util::open(state);

  // Load libraries.
  luaL_openlibs(state);

  return state;
}

lua_State* lua_connection::new_state(std::string module_name,
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

void lua_connection::load(lua_State* state, std::string script_path)
throw(lua_exception) {
  int ret = 0;
  if (0 != (ret = luaL_loadfile(state, script_path.c_str()))) {
    std::string error;

    const char* s = lua_tostring(state, -1);

    if (nullptr == s) {
      switch (ret) {
        case LUA_ERRFILE:
          error = std::string("could not open ") + script_path;
          break;
        case LUA_ERRSYNTAX:
          error = std::string("syntax error ") + ;
          break;
        case LUA_ERRMEM:
          error = "out of memory";
          break;
      }
    } else {
      error =
    }

    throw lua_exception(error);
  }
}

void lua_connection::execute(std::string script_path, lua_State* state)
throw(lua_exception) {
  load(state, script_path);
  call(state, 0, LUA_MULTRET, 0);
}

void lua_connection::call(lua_State* state,
                          int nargs, int nresults, int errfunc)
throw(lua_exception) {
  int ret;
  if (0 != (ret = lua_pcall(state, nargs, nresults, errfunc))) {
    std::string error;

    const char* s = lua_tostring(state, -1);

    if (nullptr == s) {
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

bool lua_connection::on_error(lua_State* state, const std::string& error) {
  bool set = false;
  lua_getglobal(state, "ON_ERROR");
  if (set = lua_isuserdata(state, -1)) {
    bot::callback* cb = static_cast<bot::callback*>(lua_touserdata(state, -1));
    cb(error, true);
  }
  lua_pop(state, 1);
  return set;
}

void lua_connection::login(bot* bot,
                           const std::string& username,
                           const std::string& password,
                           const std::string& package,
                           login_callback cb) {
  // Load login script.
  lua_State* state = new_state("base", bot);
  executeScript(package + "/base.lua", state);

  // Tell Lua to call the login function.
  lua_getglobal(state, "login");
  lua_pushstring(state, username.c_str());
  lua_pushstring(state, password.c_str());

  try {
    // Call login function.
    call(state, 2, 0, 0);
  } catch(const lua_exception& e) {
    lua_close(state);
    throw(e);
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
  lua_str_table_to_map(state, 1, status);
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

bot* lua_connection::bot(lua_State* state) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Get bot identifier.
  lua_getglobal(state, BOT_IDENTIFER);
  if (!lua_isstring(state, -1)) {
    lua_pop(state, 1);
    return nullptr;
  }

  // Extract and pop bot identifier string.
  std::string identifier = lua_tostring(state, -1);
  lua_pop(state, 1);

  // Check if bot exists.
  if (bots_.find(identifier) == bots_.end()) {
    return nullptr;
  }

  return bots_[identifier];
}

http::webclient* lua_connection::webclient(lua_State* state) {
  // Get bot.
  bot* bot = getBot(state);
  if (nullptr == bot) {
    return nullptr;
  }
  return bot->webclient();
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

jsonval_ptr lua_connection::to_json(lua_State* state, int stack_index,
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
        jsonval_ptr val = to_json(state, -1, allocator);
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

void lua_connection::lua_str_table_to_map(lua_State* state, int stack_index,
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

}  // namespace botscript
