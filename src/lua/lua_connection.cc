// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./lua_connection.h"

#include <cstring>
#include <sstream>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "./lua_http.h"
#include "./lua_util.h"

namespace botscript {

std::map<std::string, std::shared_ptr<bot>> lua_connection::bots_;
boost::mutex lua_connection::bots_mutex_;

namespace json = rapidjson;

jsonval_ptr lua_connection::iface(const std::string& script,
                                  const std::string& name,
                                  json::Document::AllocatorType* allocator)
throw(lua_exception) {
  // Initialize lua_State.
  lua_State* state = luaL_newstate();
  if (nullptr == state) {
    throw lua_exception("could not create script state");
  }
  luaL_openlibs(state);

  // Execute script.
  try {
    do_buffer(state, script, name);
  } catch (const lua_exception& e) {
    lua_close(state);
    throw std::runtime_error(std::string("Could not execute servers script ")
                             + e.what());
  }

  // Push lua variable to stack.
  lua_getglobal(state, (std::string("interface_") + name).c_str());

  // Read interface description as rapid-json value.
  jsonval_ptr interface = to_json(state, -1, allocator);

  // Free lua script resources.
  lua_close(state);

  return interface;
}

std::map<std::string, std::string> lua_connection::server_list(const std::string& script) {
  // Initialize lua_State.
  lua_State* state = luaL_newstate();
  if (nullptr == state) {
    throw std::runtime_error("Could not open state to read server list");
  }
  luaL_openlibs(state);

  // Execute script.
  try {
    do_buffer(state, script, "servers");
  } catch (const lua_exception& e) {
    lua_close(state);
    throw std::runtime_error(std::string("Could not execute servers script ")
                             + e.what());
  }

  // Read servers list.
  std::map<std::string, std::string> servers;
  lua_getglobal(state, "servers");
  lua_str_table_to_map(state, 1, &servers);

  // Free resources.
  lua_close(state);

  return servers;
}

void lua_connection::on_error(lua_State* state, const std::string& error_msg) {
  // Check if callback is set.
  lua_getglobal(state, BOT_LOGIN_CB);
  if (!lua_isuserdata(state, -1)) {
    std::cerr << "fatal: \"" << error_msg << "\" happened but no cb set\n";
    lua_pop(state, 1);
    lua_close(state);
    return;
  }

  void* p = lua_touserdata(state, -1);
  on_finish_cb* cb = static_cast<on_finish_cb*>(p);
  lua_pop(state, 1);
  (*cb)(error_msg);
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

void lua_connection::run(lua_State* state, on_finish_cb* cb,
                         const std::string& bot_identifier,
                         const std::string& name,
                         const std::string& script,
                         const std::string& function,
                         int nargs, int nresults, int errfunc,
                         execution_hook pre_exec, execution_hook post_exec)
throw(lua_exception) {
  // Create new script state.
  if (nullptr == state) {
    throw lua_exception("script initialisation: out of memory");
  }

  // Load standard libraries.
  luaL_openlibs(state);

  // Register botscript functions.
  lua_http::open(state);
  lua_util::open(state);

  // Set special on_finish function.
  lua_register(state, "on_finish", lua_connection::on_finish);

  // Set error callback.
  lua_pushlightuserdata(state, static_cast<void*>(cb));
  lua_setglobal(state, BOT_LOGIN_CB);

  // Load buffer.
  do_buffer(state, script, name);

  // Set module name.
  lua_pushstring(state, name.c_str());
  lua_setglobal(state, BOT_MODULE);

  // Set bot identifier.
  lua_pushstring(state, bot_identifier.c_str());
  lua_setglobal(state, BOT_IDENTIFER);

  // Check function.
  lua_getglobal(state, function.c_str());
  if (!lua_isfunction(state, -1)) {
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
    throw e;
  }

  // Execute hook function.
  if (post_exec != nullptr) {
    post_exec(state);
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

void lua_connection::login(lua_State* state,
                           std::shared_ptr<bot> bot,
                           const std::string& script,
                           on_finish_cb* cb) {
  // Gather login information.
  const auto config = bot->config();
  std::string username = config->username();
  std::string password = config->password();
  std::string package = config->package();

  try {
    // Execute login function.
    run(state, cb, config->identifier(), "base", script,
        "login", 2, 0, 0,
        [&username, &password](lua_State* state) {
          // Push login function arguments.
          lua_pushstring(state, username.c_str());
          lua_pushstring(state, password.c_str());
        });
  } catch(const lua_exception& e) {
    (*cb)(e.what());
  }
}

void lua_connection::module_run(const std::string module_name,
                                lua_State* state, module* module_ptr,
                                on_finish_cb* cb) {
  // Gather run information.
  std::shared_ptr<bot> bot = module_ptr->get_bot();
  const std::string& script = module_ptr->script();
  const std::string& base_script = module_ptr->base_script();
  std::string run_fun = module_ptr->lua_run();

  try {
    // Execute login function.
    run(state, cb, bot->config()->identifier(), module_name, script, run_fun,
        0, 0, 0,
        [module_ptr, base_script](lua_State* state) {
          do_buffer(state, base_script, "base");
          module_ptr->set_lua_status(state);
        },
        [module_name](lua_State* state) {
          lua_getglobal(state, ("finally_" + module_name).c_str());
          if (lua_isfunction(state, -1)) {
            // Call function.
            try {
              exec(state, 0, 0, 0);
            } catch(const lua_exception& e) {
              throw e;
            }
          }
        });
  } catch(const lua_exception& e) {
    (*cb)(e.what());
  }
}

int lua_connection::on_finish(lua_State* state) {
  // Check if state is already finished.
  lua_getglobal(state, BOT_FINISH);
  bool finished = lua_isboolean(state, -1) && lua_toboolean(state, -1);
  lua_pop(state, 1);
  if (finished) {
    return luaL_error(state, "on_finish_error");
  }

  // Mark state as finished.
  lua_pushboolean(state, static_cast<int>(true));
  lua_setglobal(state, BOT_FINISH);

  // Check if callback is set.
  lua_getglobal(state, BOT_LOGIN_CB);
  if (!lua_isuserdata(state, -1)) {
    std::cout << "login cb not set\n";
    return 0;
  }

  // Get callback.
  void* p = lua_touserdata(state, -1);
  on_finish_cb* cb = static_cast<on_finish_cb*>(p);
  lua_pop(state, 1);

  // Call callback function.
  (*cb)("");

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
  try {
    do_buffer(state, script, var);
  } catch (lua_exception) {
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
    lua_pop(state, 1);
    throw lua_exception("module status is not a table");
  }

  // Set new value.
  lua_pushstring(state, key.c_str());
  lua_pushstring(state, value.c_str());
  lua_settable(state, -3);
  lua_pop(state, 1);
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
  if (bots_.find(bot->config()->identifier()) == bots_.end()) {
    bots_[bot->config()->identifier()] = bot;
  }
}

void lua_connection::remove(const std::string& identifier) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Search for identifier and delete entry.
  auto i = bots_.find(identifier);
  if (i != bots_.end()) {
    std::stringstream msg;
    msg << "lua_connection::remove(\""
        << identifier << "\") -> " << i->second.use_count();
    i->second->log(bot::BS_LOG_DBG, "base", msg.str());
    bots_.erase(i);
  } else {
    std::cout << "fatal: lua_connection::remove(\""
              << identifier << "\") -> not found\n";
  }
}

bool lua_connection::contains(const std::string& identifier) {
  // Lock because of bots map r/w access.
  boost::lock_guard<boost::mutex> lock(bots_mutex_);

  // Return whether the bot map contains the specified identifier
  return bots_.find(identifier) != bots_.end();
}

void lua_connection::do_buffer(lua_State* state,
                               const std::string& script,
                               const std::string& name) throw(lua_exception) {
  // Load buffer to state.
  int ret = luaL_loadbuffer(state,
                            script.c_str(), script.length(), name.c_str());

  // Check load error.
  if (LUA_OK != ret) {
    std::string error;

    // Extract error string.
    const char* s = nullptr;
    if (lua_isstring(state, -1)) {
      s = lua_tostring(state, -1);
    }

    // If extracted error string is a nullptr or empty:
    // Translate error code.
    if (nullptr == s || std::strlen(s) == 0) {
      switch (ret) {
        case LUA_ERRSYNTAX: error = "syntax error"; break;
        case LUA_ERRMEM:    error = "out of memory"; break;
        case LUA_ERRGCMM:   error = "gc error"; break;
        default:            error = "unknown error code"; break;
      }
    } else {
      error = s;
    }

    throw lua_exception(error.empty() ? "unknown error" : error);
  }

  // Run loaded buffer and check for errors.
  if (0 != (ret = lua_pcall(state, 0, LUA_MULTRET, 0))) {
    std::string error;

    const char* s = nullptr;
    if (lua_isstring(state, -1)) {
      s = lua_tostring(state, -1);
    }

    if (nullptr == s || std::strlen(s) == 0) {
      switch (ret) {
        case LUA_ERRRUN:  error = "runtime error"; break;
        case LUA_ERRMEM:  error = "out of memory"; break;
        case LUA_ERRERR:  error = "error handling error"; break;
        case LUA_ERRGCMM: error = "gc error"; break;
        default:          error = "unknown error code"; break;
      }
    } else {
      error = s;
    }

    throw lua_exception(error.empty() ? "unknown error" : error);
  }
}

jsonval_ptr lua_connection::to_json(lua_State* state, int stack_index,
    json::Document::AllocatorType* allocator) {
  switch (lua_type(state, stack_index)) {
    // Convert lua string to JSON string.
    case LUA_TSTRING: {
      const char* str = luaL_checkstring(state, stack_index);
      return std::make_shared<json::Value>(str, *allocator);
    }

    // Convert lua table to JSON object.
    case LUA_TTABLE: {
      // Push the first key (nil).
      lua_pushnil(state);
      stack_index = (stack_index > 0) ? stack_index : stack_index - 1;

      // Iterate keys.
      jsonval_ptr obj = std::make_shared<json::Value>(json::kObjectType);
      while (lua_next(state, stack_index) != 0) {
        json::Value key(luaL_checkstring(state, -2), *allocator);
        jsonval_ptr val = to_json(state, -1, allocator);
        obj->AddMember(key, *val.get(), *allocator);

        // Pop the value, key stays for lua_next.
        lua_pop(state, 1);
      }
      return obj;
    }

    // Convert unknown type (i.e. lua thread) to JSON null.
    default: {
      return std::make_shared<json::Value>(json::kNullType);
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
