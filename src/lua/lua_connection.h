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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <map>
#include <string>

#include "boost/thread.hpp"
#include "boost/shared_ptr.hpp"

#include "rapidjson/document.h"

#include "../http/webclient.h"
#include "../bot.h"
#include "../exceptions/bad_login_exception.h"

#define BOT_IDENTIFER ("bot_identifier")
#define BOT_MODULE ("bot_module")

namespace botscript {

typedef boost::shared_ptr<rapidjson::Value> jsonval_ptr;

class bot;

/**
 * This exception indicates an error that occured at lua script execution.
 */
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

/**
 * Class containing static methods to interoperate with lua.
 */
class lua_connection {
 public:

  /// Login callback (call with true to indicate success).
  typedef boost::function<void (bool)> login_callback;

  /**
   * Reads the interface description of the module script.
   *
   * The interface description variable name is
   * 'interface_' + {script path stem}
   * example: 'packages/pg/sell.lua' -> interface_sell
   *
   * \param script the script path to load
   * \param allocator the rapid-json allocator to use
   * \exception lua_exception if the execution of the lua script fails
   * \return the interface description in JSON format
   */
  static jsonval_ptr iface(const std::string& script,
      rapidjson::Document::AllocatorType* allocator)
  throw(lua_exception);

  /**
   * Loads the servers table contained in the script.
   *
   * \param script the script containing the servers table
   * \param servers the servers std::map to write to
   * \return true when the servers could be read successfully
   */
  static bool server_list(const std::string& script,
                          std::map<std::string, std::string>* servers);

  /**
   * Creates a new script state with all bot script functions registered.
   *
   * \return the script state
   * \exception lua_exception on bad_alloc
   */
  static lua_State* new_state()
  throw(lua_exception);

  /**
   * Creates a new script state with all bot script functions.
   * Registers the bot identifier (also to the bots mapping) and module name.
   *
   * \param module_name the module name to register
   * \param bot the bot to register
   * \exception lua_exception on bad_alloc
   */
  static lua_State* new_state(std::string module_name, bot* bot)
  throw(lua_exception);

  /**
   * Loads the script located at script_path to the given lua state.
   *
   * \param state the state to load the script to
   * \param script_path the file path of the script to load
   * \exception lua_exception if the loading fails (for example on syntax error)
   */
  static void load(lua_State* state, std::string script_path)
  throw(lua_exception);

  /**
   * Loads the lua script located at script_path to the given state and
   * executes it.
   *
   * \param script_path the path of the script to load
   * \param state the state to use for execution
   * \exception lua_exception if the execution failes (syntax or lua error)
   */
  static void execute(std::string script_path, lua_State* state)
  throw(lua_exception);

  /**
   * Calls the function previously pushed to the stack of the lua state.
   *
   * \param state the lua script state
   * \param nargs the argument count
   * \param nresults the result count
   * \param errfunc the error function
   * \exception lua_exception if the function call failes
   */
  static void call(lua_State* state,
                   int nargs, int nresults, int errfunc)
  throw(lua_exception);

  bool on_error(lua_State* state, const std::string& error);

  static void login(bot* bot,
                    const std::string& username,
                    const std::string& password,
                    const std::string& package,
                    login_callback cb);


  /**
   * Reads the lua table var from the lua script state to the given status.
   *
   * \param state the lua script state
   * \param status the std::map status to write to
   * \param var the variable name to load
   */
  static void get_status(lua_State* state, const std::string& var,
                         std::map<std::string, std::string>* status);

  /**
   * Writes the key and value to the given variable in the given scrip state
   *
   * \param state the lua script state
   * \param var the table variable name
   * \param key the key to write
   * \param value the value to write
   */
  static void set_status(lua_State* state, const std::string& var,
                         const std::string& key, const std::string& value);

  /**
   * Returns the corresponding bot registered to the script state.
   *
   * \param state the script state to extract the bot identifier from
   * \return the pointer to the bot
   */
  static bot* bot(lua_State* state);

  /**
   * Returns the webclient of the corresponding bot for the given script state.
   *
   * \param state the script state to extract the bot identifier from
   * \return a pointer to the webclient of the given bot
   */
  static http::webclient* webclient(lua_State* state);

  /**
   * Removes the bot with the given identifier from the bot map (does locking).
   *
   * \param identifier the identifier (key) to remove
   */
  static void remove(const std::string& identifier);

  /**
   * Checks whether a bot with the given identifier exists the bot map (does locking).
   *
   * \param identifier the identifier (key) to remove
   * \return true if found false if not
   */
  static bool contains(const std::string& identifier);

 private:
  /**
   * Recursive toJSON.
   *
   * \param state the lua script state
   * \param stack_index the index of the element that should be converted
   * \param allocator the pointer to the rapid-json memory allocator to use
   */
  static jsonval_ptr to_json(lua_State* state, int stack_index,
                             rapidjson::Document::AllocatorType* allocator);

  /**
   * Reads the table at the specified stack_index to a std::map.
   *
   * \param state the script state
   * \param stack_index the stack index of the table to read
   * \param map the map to store the keys and values to
   */
  static void lua_str_table_to_map(lua_State* state, int stack_index,
                                   std::map<std::string, std::string>* map);

  static std::map<std::string, bot*> bots_;
  static boost::mutex bots_mutex_;
};

}  // namespace botscript

#endif  // LUA_CONNECTION_H_
