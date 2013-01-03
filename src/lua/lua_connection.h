/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 *
 * Copyright (C) December 1st, 2012 makielskis@gmail.com
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

#define BOT_IDENTIFER ("__BOT_IDENTIFIER")
#define BOT_MODULE    ("__BOT_MODULE")
#define BOT_CALLBACK  ("__BOT_CALLBACK")
#define BOT_ERROR_CB  ("__BOT_ON_LOGIN")

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <functional>
#include <map>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include <rapidjson/document.h>

#include "../bot.h"

namespace botscript {

///  Shared pointer to a JSON value.
typedef boost::shared_ptr<rapidjson::Value> jsonval_ptr;

class bot;

/// This exception indicates an error that occured at lua script execution.
class lua_exception : public std::exception {
 public:
  explicit lua_exception(const std::string& what)
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

/// Class connecting the bot and Lua.
class lua_connection {
 public:
  // Function to be called before or after the Lua script execution.
  typedef std::function<void (lua_State*)> execution_hook;

  /// Reads the interface description of the module script.
  ///
  /// The interface description variable name is
  /// 'interface_' + {script path stem}
  ///
  /// \param script the script path to load
  /// \param allocator the rapid-json allocator to use
  /// \exception lua_exception if the execution of the lua script fails
  /// \return the interface description in JSON format
  static jsonval_ptr iface(const std::string& script,
      rapidjson::Document::AllocatorType* allocator)
  throw(lua_exception);

  /// Loads the servers table contained in the script.
  ///
  /// \param script the script containing the servers table
  /// \param servers the servers std::map to write to
  /// \return true when the servers could be read successfully
  static bool server_list(const std::string& script,
                          std::map<std::string, std::string>* servers);

  /// This function should be called when an error occures in an asynchronous
  /// function call (like http.xy). It calls the callback function registered
  /// in the state AND CLOSES IT (!) (do NOT reuse or call functions on this).
  ///
  /// \param state the lua state
  /// \param error_msg the message of the error that occured
  static void on_error(lua_State* state, const std::string& error_msg);

  /// Calls the function that is on top of the stack.
  ///
  /// \param state the state where the stack top is already a function
  /// \param nargs the argument count
  /// \param nresults the result count
  /// \param errfunc the error function
  static void exec(lua_State* state, int nargs, int nresults, int errfunc)
  throw(lua_exception);

  /// Calls the function previously pushed to the stack of the lua state.
  ///
  /// \param bot_identifier the identifier of the calling bot
  /// \param module_name the module name of the calling module
  /// \param script the script path where the script to execute is located
  /// \param function the name of the function to call
  /// \param nargs the argument count
  /// \param nresults the result count
  /// \param errfunc the error function
  /// \param pre_exec pre execution hook function
  /// \param post_exec post exection hook function
  /// \exception lua_exception if the function call fails
  static void run(const std::string& bot_identifier,
                  const std::string& script,
                  const std::string& function,
                  int nargs, int nresults, int errfunc,
                  execution_hook pre_exec = nullptr,
                  execution_hook post_exec = nullptr)
  throw(lua_exception);

  /// Performs an asynchronous login.
  /// Calls the callback function with an empty string on success and with an
  /// errro message if the login failed.
  ///
  /// \param bot the bot to login
  /// \param cb the callback to call on finish
  static void login(boost::shared_ptr<bot> bot, bot::error_callback* cb);

  /// Login callback function (lua_CFunction).
  static int handle_login(lua_State* state);

  /// Reads the lua table var from the lua script state to the given status.
  ///
  /// \param state the lua script state
  /// \param status the std::map status to write to
  /// \param var the variable name to load
  static void get_status(lua_State* state, const std::string& var,
                         std::map<std::string, std::string>* status);

  /// Writes the key and value to the given variable in the given scrip state
  ///
  /// \param state the lua script state
  /// \param var the table variable name
  /// \param key the key to write
  /// \param value the value to write
  static void set_status(lua_State* state, const std::string& var,
                         const std::string& key, const std::string& value);

  /// Returns the corresponding bot registered to the script state.
  ///
  /// \param state the script state to extract the bot identifier from
  /// \return the pointer to the bot (or nullptr if the bot could not be found)
  static boost::shared_ptr<bot> get_bot(lua_State* state);

  /// Adds the specified bot to the lua connection bot map.
  ///
  /// \param bot the bot to add
  static void add(boost::shared_ptr<bot> bot);

  /// Removes the bot with the given identifier from the bot map .
  ///
  /// \param identifier the identifier (key) to remove
  static void remove(const std::string& identifier);

  /// Checks whether a bot with the given identifier exists.
  ///
  /// \param identifier the identifier (key) to remove
  /// \return true if found false if not
  static bool contains(const std::string& identifier);

  /// Reads the table at the specified stack_index to a std::map.
  ///
  /// \param state the script state
  /// \param stack_index the stack index of the table to read
  /// \param map the map to store the keys and values to
  static void lua_str_table_to_map(lua_State* state, int stack_index,
                                   std::map<std::string, std::string>* map);
 private:
  /// Recursive toJSON.
  ///
  /// \param state the lua script state
  /// \param stack_index the index of the element that should be converted
  /// \param allocator the pointer to the rapid-json memory allocator to use
  static jsonval_ptr to_json(lua_State* state, int stack_index,
                             rapidjson::Document::AllocatorType* allocator);

  /// Bots mapping from identifier to bot pointer.
  static std::map<std::string, boost::shared_ptr<bot>> bots_;

  /// Mutex to synchronize access to the lua_connection::bots_ mapping.
  static boost::mutex bots_mutex_;
};

}  // namespace botscript

#endif  // LUA_CONNECTION_H_
