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

#include "./webclient.h"
#include "./bot.h"
#include "./exceptions/lua_exception.h"
#include "./exceptions/bad_login_exception.h"

#define BOT_IDENTIFER "bot_identifier"
#define BOT_MODULE "bot_module"

namespace botscript {

class bot;

typedef boost::shared_ptr<rapidjson::Value> jsonval_ptr;

/**
 * Class containing static methods to interoperate with the lua scripting
 * language.
 */
class lua_connection {
 public:
  /**
   * Converts lua tables containing either string or tables to JSON.
   *
   * \param state the script state
   * \param lua_var the variable to convert to JSON
   * \param var_name the name to be used for the JSON value in the JSON object\n
   *                 result: { "var_name": $converted_variable }
   * \return the converted variable as JSON string
   */
  static std::string toJSON(lua_State* state, const std::string& lua_var,
                            const std::string& var_name);

  /**
   * Reads the table at the specified stack_index to a std::map.
   *
   * \param state the script state
   * \param stack_index the stack index of the table to read
   * \param map the map to store the keys and values to
   */
  static void luaStringTableToMap(lua_State* state, int stack_index,
                                  std::map<std::string, std::string>* map);

  /**
   * Loads the servers table contained in the script.
   *
   * \param script the script containing the servers table
   * \param servers the servers std::map to write to
   * \return true when the servers could be read successfully
   */
  static bool loadServerList(const std::string& script,
                             std::map<std::string, std::string>* servers);

  /**
   * Loads the script located at script_path to the given lua state.
   *
   * \param state the state to load the script to
   * \param script_path the file path of the script to load
   * \exception lua_exception if the loading fails (for example on syntax error)
   */
  static void loadScript(lua_State* state, std::string script_path)
  throw(lua_exception);

  /**
   * Calls the function previously pushed to the stack of the lua state.
   *
   * \param state the lua script state
   * \exception lua_exception if the function call failes
   */
  static void callFunction(lua_State* state,
                           int nargs, int nresults, int errfunc)
  throw(lua_exception);

  /**
   * Loads the lua script located at script_path to the given state and
   * executes it
   *
   * \param script_path the path of the script to load
   * \param state the state to use for execution
   * \exception lua_exception if the execution failes (syntax or lua error)
   */
  static void executeScript(std::string script_path, lua_State* state)
  throw(lua_exception);

  /**
   * Creates a new script state with all bot script functions registered.
   *
   * \return the script state
   * \exception lua_exception on bad_alloc
   */
  static lua_State* newState()
  throw(lua_exception);

  /**
   * Creates a new script state with all bot script functions.
   * Registers the bot identifier (also to the bots mapping) and module name.
   *
   * \param module_name the module name to register
   * \param bot the bot to register
   * \exception lua_exception on bad_alloc
   */
  static lua_State* newState(std::string module_name, bot* bot)
  throw(lua_exception);

  /**
   * Loads package/login.lua and executes login(username, password).
   *
   * \param bot the bot to login
   * \param username the login username
   * \param password the login password
   * \param package the package to load the login function from
   * \exception bad_login_exception on a failed login
   * \exception lua_exception on a lua error
   */
  static void login(bot* bot,
                    const std::string& username,
                    const std::string& password,
                    const std::string& package)
  throw(lua_exception, bad_login_exception);

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
  static bot* getBot(lua_State* state);

  /**
   * Returns the webclient of the corresponding bot for the given script state.
   *
   * \param state the script state to extract the bot identifier from
   * \return a pointer to the webclient of the given bot
   */
  static webclient* getWebClient(lua_State* state);

  /**
   * Removes the bot with the given identifier from the bot map (does locking).
   *
   * \param identifier the identifier (key) to remove
   */
  static void remove(const std::string identifier);

  /**
   * Calls the log function of the bot registered in the script state.
   * The log message has to reside on the stack on posititon 1.
   *
   * \param state the script state
   * \param log_level the log level to use (INFO or ERROR)
   */
  static void log(lua_State* state, int log_level);

  /**
   * Does a get request.
   * Prepends the base url if the path parameter is set to true.
   *
   * \param state the lua script state
   * \param path whether to prepend the base url or not
   */
  static int doRequest(lua_State* state, bool path);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_request(url) does a GET request to the specified URL and returns the
   * response.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_request(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_request_path(path) does a GET request to the specified path (relative to
   * the base url) and returns the response.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_request_path(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_post_request(url, data) does a POST request sending the specified data to
   * the given URL and returns the response.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_post_request(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_submit_form(content, xpath [, parameters [, action]]) submits the form
   * that is contained in the content and can be found by the given XPath.
   * Parameters and overriding the form action is optional. It returns the
   * web server response.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_submit_form(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_get_by_xpath(string, xpath) returns the element selected by the XPath.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_get_by_xpath(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_get_by_regex(string, regex) returns the string selected by the regex.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_get_by_regex(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_get_all_by_regex(string, regex) returns all regex matches in tables.
   * First table layer contains all matches. Matches tables contain capturing
   * groups.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_get_all_by_regex(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_get_location(page) returns the location of the page. The location will
   * be written to the page XML. m_get_location extracts this information.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_get_location(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_log(message) logs the given message as INFO message.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_log(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_log_error(message) logs the given message as ERROR message.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_log_error(lua_State* state);

  /**
   * BOTSCRIPT API FUNCTION\n
   * m_set_status(key, value) sets the status of the loaded module.
   *
   * \param state the lua script state (with parameters on the stack)
   * \return the count of return values pushed to the stack
   */
  static int m_set_status(lua_State* state);

 private:
  /**
   * Recursive toJSON.
   *
   * \param state the lua script state
   * \param stack_index the index of the element that should be converted
   * \param allocator the rapid-json memory allocator
   */
  static jsonval_ptr toJSON(lua_State* state, int stack_index,
      rapidjson::Document::AllocatorType& allocator);

  static std::map<std::string, bot*> bots_;
  static boost::mutex bots_mutex_;
  static webclient* webclient_;
};

}  // namespace botscript

#endif  // LUA_CONNECTION_H_
