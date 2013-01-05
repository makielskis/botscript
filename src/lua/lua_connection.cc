// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./lua_connection.h"

#include <cstring>

#include "./lua_http.h"
#include "./lua_util.h"

namespace botscript {

std::map<std::string, std::shared_ptr<bot>> lua_connection::bots_;
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

void lua_connection::on_error(lua_State* state, const std::string& error_msg) {
  // Check if callback is set.
  lua_getglobal(state, BOT_LOGIN_CB);
  if (!lua_isuserdata(state, -1)) {
    lua_pop(state, 1);
    lua_close(state);
    return;
  }

  // Get module name to distinguish login (module == "base")
  // from module run (module != "base").
  lua_getglobal(state, BOT_MODULE);
  std::string module;
  if (lua_isstring(state, -1)) {
    module = lua_tostring(state, -1);
  } else {
    lua_pop(state, 1);
    lua_close(state);
    return;
  }
  lua_pop(state, 1);

  if (module == "base") {
    // Get login callback.
    void* p = lua_touserdata(state, -1);
    bot::error_callback* cb = static_cast<bot::error_callback*>(p);
    lua_pop(state, 1);

    // Call callback with error message.
    (*cb)(error_msg);
  } else {
    // Get module run callback.
    void* p = lua_touserdata(state, -1);
    std::function<void (std::string, int, int)>* cb =
        static_cast<std::function<void (std::string, int, int)>*>(p);
    lua_pop(state, 1);

    // Call callback with error message.
    (*cb)(error_msg, -1, -1);
  }

  // Close state.
  lua_close(state);
}

void lua_connection::exec(lua_State* state,
                          int nargs, int nresults, int errfunc)
throw(lua_exception) {
  int ret = 0;
  if (0 != (ret = lua_pcall(state, nargs, nresults, errfunc))) {
    std::string error;

    const char* s = nullptr;
    if (lua_isstring(state, -1)) {
      s = lua_tostring(state, -1);
    }

    if (nullptr == s || std::strlen(s) == 0) {
      switch (ret) {
        case LUA_ERRRUN: error = "runtime error"; break;
        case LUA_ERRMEM: error = "out of memory"; break;
        case LUA_ERRERR: error = "error handling error"; break;
      }
    } else {
      error = s;
    }

    throw lua_exception(error.empty() ? "unknown error" : error);
  }
}

void lua_connection::run(const std::string& bot_identifier,
                         const std::string& script,
                         const std::string& function,
                         int nargs, int nresults, int errfunc,
                         execution_hook pre_exec, execution_hook post_exec)
throw(lua_exception) {
  // Create new script state.
  lua_State* state = luaL_newstate();
  if (nullptr == state) {
    throw lua_exception("script initialisation: out of memory");
  }

  // Load standard libraries.
  luaL_openlibs(state);

  // Register botscript functions.
  lua_http::open(state);
  lua_util::open(state);

  // Load file.
  int ret = 0;
  if (0 != (ret = luaL_loadfile(state, script.c_str()))) {
    std::string error;

    const char* s = nullptr;
    if (lua_isstring(state, -1)) {
      s = lua_tostring(state, -1);
    }

    if (nullptr == s || std::strlen(s) == 0) {
      switch (ret) {
        case LUA_ERRFILE:   error = std::string("file error ") + script; break;
        case LUA_ERRSYNTAX: error = "syntax error"; break;
        case LUA_ERRMEM:    error = "out of memory"; break;
      }
    } else {
      error = s;
    }

    lua_close(state);
    throw lua_exception(error.empty() ? "unknown error" : error);
  }

  // Execute file.
  if (0 != (ret = lua_pcall(state, 0, LUA_MULTRET, 0))) {
    std::string error;

    const char* s = nullptr;
    if (lua_isstring(state, -1)) {
      s = lua_tostring(state, -1);
    }

    if (nullptr == s || std::strlen(s) == 0) {
      switch (ret) {
        case LUA_ERRRUN: error = "runtime error"; break;
        case LUA_ERRMEM: error = "out of memory"; break;
        case LUA_ERRERR: error = "error handling error"; break;
      }
    } else {
      error = s;
    }

    lua_close(state);
    throw lua_exception(error.empty() ? "unknown error" : error);
  }

  // Discover module name (stem without '.lua').
  using boost::filesystem::path;
  std::string filename = path(script).filename().generic_string();
  std::string stem = filename.substr(0, filename.length() - 4);

  // Set module name.
  lua_pushstring(state, stem.c_str());
  lua_setglobal(state, BOT_MODULE);

  // Set bot identifier.
  lua_pushstring(state, bot_identifier.c_str());
  lua_setglobal(state, BOT_IDENTIFER);

  // Check function.
  lua_getglobal(state, function.c_str());
  if (!lua_isfunction(state, -1)) {
    lua_close(state);
    throw lua_exception(std::string("function ") + function + " not defined");
  }

  // Execute hook function.
  if (pre_exec != nullptr) {
    pre_exec(state);
  }

  // Call function.
  try {
    exec(state, nargs, nresults, errfunc);
  } catch(const lua_exception& e) {
    lua_close(state);
    throw e;
  }

  // Execute hook function.
  if (post_exec != nullptr) {
    post_exec(state);
  }

  // Check whether an asynchronous action was startet (callback set).
  // If this is NOT the case: close state.
  // Otherwise, the asynchronous function is responsible to handle the lifetime.
  lua_getglobal(state, BOT_CALLBACK);
  if (!lua_isfunction(state, -1)) {
    lua_close(state);
  }
}

void lua_connection::login(std::shared_ptr<bot> bot, bot::error_callback* cb) {
  // Gather login information.
  std::string username = bot->username();
  std::string password = bot->password();
  std::string package = bot->package();

  try {
    // Execute login function.
    run(bot->identifier(), package + "/base.lua", "login", 2, 0, 0,
        [username, password, cb](lua_State* state) {
          // Set special on_finish function.
          lua_register(state, "on_finish", lua_connection::on_login_finish);

          // Set error callback.
          lua_pushlightuserdata(state, static_cast<void*>(cb));
          lua_setglobal(state, BOT_LOGIN_CB);

          // Push login function arguments.
          lua_pushstring(state, username.c_str());
          lua_pushstring(state, password.c_str());
        });
  } catch(const lua_exception& e) {
    (*cb)(e.what());
  }
}

int lua_connection::on_login_finish(lua_State* state) {
  // Check if callback is set.
  lua_getglobal(state, BOT_LOGIN_CB);
  if (!lua_isuserdata(state, -1)) {
    return 0;
  }

  // Get callback.
  void* p = lua_touserdata(state, -1);
  bot::error_callback* cb = static_cast<bot::error_callback*>(p);
  lua_pop(state, 1);

  // Get result.
  if (!lua_isboolean(state, -1)) {
    return luaL_error(state, "no result set");
  }
  bool success = static_cast<bool>(lua_toboolean(state, -1));
  lua_pop(state, 1);

  // Call callback function.
  (*cb)(success ? "" : "failed");

  return 0;
}

void lua_connection::module_run(module* module_ptr,
    std::function<void (std::string, int, int)>* cb) {
  // Gather run information.
  std::shared_ptr<bot> bot = module_ptr->get_bot();
  std::string script = module_ptr->script();
  std::string run_fun = module_ptr->lua_run();

  try {
    // Execute login function.
    run(bot->identifier(), script, run_fun, 0, 0, 0,
        [cb](lua_State* state) {
          // Set special on_finish function.
          lua_register(state, "on_finish", lua_connection::on_run_finish);

          // Set error callback.
          lua_pushlightuserdata(state, static_cast<void*>(cb));
          lua_setglobal(state, BOT_LOGIN_CB);
        });
  } catch(const lua_exception& e) {
    (*cb)(e.what(), -1, -1);
  }
}

int lua_connection::on_run_finish(lua_State* state) {
  // Check if callback is set.
  lua_getglobal(state, BOT_LOGIN_CB);
  if (!lua_isuserdata(state, -1)) {
    return 0;
  }

  // Get callback.
  void* p = lua_touserdata(state, -1);
  std::function<void (std::string, int, int)>* cb =
      static_cast<std::function<void (std::string, int, int)>*>(p);
  lua_pop(state, 1);

  // Wait time.
  int argc = lua_gettop(state);
  int n1 = -1, n0 = -1;
  if (argc >= 2) {
    n1 = lua_isnumber(state, -2) ? luaL_checkint(state, -2) : -1;
  }
  if (argc >= 1) {
    n0 = lua_isnumber(state, -1) ? luaL_checkint(state, -1) : -1;
  }

  // Call callback function.
  (*cb)("", n1, n0);

  return 0;
}

bool lua_connection::get_status(const std::string& script,
                                const std::string& var,
                                std::map<std::string, std::string>* status) {
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
  get_status(state, var, status);

  // Free resources.
  lua_close(state);

  return true;
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

std::shared_ptr<bot> lua_connection::get_bot(lua_State* state) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Get bot identifier.
  lua_getglobal(state, BOT_IDENTIFER);
  if (!lua_isstring(state, -1)) {
    lua_pop(state, 1);
    return std::shared_ptr<bot>();
  }

  // Extract and pop bot identifier string.
  std::string identifier = lua_tostring(state, -1);
  lua_pop(state, 1);

  // Check if bot exists.
  if (bots_.find(identifier) == bots_.end()) {
    return std::shared_ptr<bot>();
  }

  return bots_[identifier];
}

void lua_connection::add(std::shared_ptr<bot> bot) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Add bot if not it is not already stored.
  if (bots_.find(bot->identifier()) == bots_.end()) {
    bots_[bot->identifier()] = bot;
  }
}

void lua_connection::remove(const std::string& identifier) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Search for identifier and delete entry.
  auto i = bots_.find(identifier);
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
    std::string key = luaL_checkstring(state, -2);
    std::string value = luaL_checkstring(state, -1);
    (*map)[key] = value;

    // Pop the value, key stays for lua_next.
    lua_pop(state, 1);
  }
}

}  // namespace botscript
