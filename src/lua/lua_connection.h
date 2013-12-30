// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef LUA_CONNECTION_H_
#define LUA_CONNECTION_H_

#define BOT_IDENTIFER ("__BOT_IDENTIFIER")
#define BOT_MODULE    ("__BOT_MODULE")
#define BOT_CALLBACK  ("__BOT_CALLBACK")
#define BOT_LOGIN_CB  ("__BOT_ON_LOGIN")
#define BOT_FINISH    ("__BOT_FINISH")

#include <exception>
#include <memory>
#include <functional>
#include <map>
#include <string>

#include "boost/thread.hpp"
#include "boost/filesystem.hpp"

#include "../rapidjson_with_exception.h"

#include "../bot.h"
#include "../module.h"

struct lua_State;

namespace botscript {

/// Callback function for asynchronous lua scripts. If the lua script can be
/// executed correctly, the function is called 2x (""), (""):
///    * The first time in the function call of on_finish. Extract the provided
///      parameters and store them.
///    * The second time when the lua script stops execution. Close the state
///      and call your own callback function.
///
/// If the function throws an error after the on_finish call, the callback fun.
/// will also be called 2x (""), ("error"):
///    * The first time in the function call of on_finish. Extract the provided
///      parameters and store them.
///    * The second time when the lua script throws an error when finishing
///      execution. This error can be ignored, because on_finish() provided
///      already valid results.
///
/// If the function throws an error before the on_finish call, the callback fun.
/// will be called just 1x ("error") providing the error string. Close
/// the lua state and call your own callback with the provided error string.
///
/// Generally speaking, the state can always be closed if an error is set.
///
/// Warning! The state is only provided in success cases to extract on_finish
/// function call arguments, not to call lua_close() on it. State livetime
/// should be managed with the state_wrapper class.
typedef std::function<void (std::string)> on_finish_cb;

///  Shared pointer to a JSON value.
typedef std::shared_ptr<rapidjson::Value> jsonval_ptr;

class bot;
class module;

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
  /// \param script the script to load
  /// \param name the script name
  /// \param allocator the rapid-json allocator to use
  /// \exception lua_exception if the execution of the lua script fails
  /// \return the interface description in JSON format
  static jsonval_ptr iface(const std::string& script,
                           const std::string& name,
                           rapidjson::Document::AllocatorType* allocator)
  throw(lua_exception);

  /// Loads the servers table contained in the script.
  ///
  /// \param script the script containing the servers table
  /// \param servers the servers std::map to write to
  /// \return true when the servers could be read successfully
  static std::map<std::string, std::string> server_list(const std::string& script);

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
  /// \param state            the lua state to run the script on
  /// \param bot_identifier   the identifier of the calling bot
  /// \param module_name      the module name of the calling module
  /// \param script           the path where the script to execute is located
  /// \param function         the name of the function to call
  /// \param nargs            the argument count
  /// \param nresults         the result count
  /// \param errfunc          the error function
  /// \param pre_exec         pre execution hook function
  /// \param post_exec        post exection hook function
  /// \exception lua_exception if the function call fails
  static void run(lua_State* state,
                  on_finish_cb* cb,
                  const std::string& bot_identifier,
                  const std::string& name,
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
  /// \param state  the lua state to run the login script on
  /// \param bot    the bot to login
  /// \param cb     the callback to call on finish
  static void login(lua_State* state, std::shared_ptr<bot> bot,
                    const std::string& script, on_finish_cb* cb);

  /// Runs the module asynchronously. Calls the callback with an error string
  /// and -1, -1 if an error occurs. Otherwise (if on_finish got called), the
  /// callback will be called with the parameters provided to on_finish.
  ///
  /// \param state       the lua state to run the module on
  /// \param module_ptr  pointer to the module which asks to launch the script
  /// \param cb          the callack to call on error / on_finish(...) call
  static void module_run(const std::string module_name,
                         lua_State* state, module* module_ptr,
                         on_finish_cb* cb);

  /// On finish function (lua_Cfunction).
  static int on_finish(lua_State* state);

  /// Reads the lua table var from the lua script state to the given status.
  ///
  /// \param state the lua script state
  /// \param var the variable name to load
  /// \param status the std::map status to write to
  static void get_status(lua_State* state, const std::string& var,
                         std::map<std::string, std::string>* status);

  /// Reads the lua table var from the lua script to the given status.
  ///
  /// \param script path to the script to read from
  /// \param var the variable name to load
  /// \param status the std::map status to write to
  /// \return whether the script could be loaded or not
  static bool get_status(const std::string& script, const std::string& var,
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
  static std::shared_ptr<bot> get_bot(lua_State* state);

  /// Adds the specified bot to the lua connection bot map.
  ///
  /// \param bot the bot to add
  static void add(std::shared_ptr<bot> bot);

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
  /// Loads and executes the given buffer.
  ///
  /// \param state   the state to load the buffer to
  /// \param script  the script (buffer) to load
  /// \param name    the name of the script (for error messages etc.)
  static void do_buffer(lua_State* state,
                        const std::string& script,
                        const std::string& name)
  throw(lua_exception);

  /// Recursive toJSON.
  ///
  /// \param state the lua script state
  /// \param stack_index the index of the element that should be converted
  /// \param allocator the pointer to the rapid-json memory allocator to use
  static jsonval_ptr to_json(lua_State* state, int stack_index,
                             rapidjson::Document::AllocatorType* allocator);

  /// Bots mapping from identifier to bot pointer.
  static std::map<std::string, std::shared_ptr<bot>> bots_;

  /// Mutex to synchronize access to the lua_connection::bots_ mapping.
  static boost::mutex bots_mutex_;
};

}  // namespace botscript

#endif  // LUA_CONNECTION_H_
