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
 * but WITHOUT ANY WARRANTY;without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "./lua_util.h"

namespace botscript {

int lua_util::get_by_xpath(lua_State* state) {
  // Get arguments from stack.
  std::string str = luaL_checkstring(state, 1);
  std::string xpath = luaL_checkstring(state, 2);

  // Arguments read. Pop them.
  lua_pop(state, 2);

  // Use pugi for xpath query.
  std::string value;
  try {
    pugi::xml_document doc;
    doc.load(str.c_str());
    pugi::xpath_query query(xpath.c_str());
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

int lua_util::get_all_by_xpath(lua_State* state) {
  // Get arguments from stack.
  std::string str = luaL_checkstring(state, 1);
  std::string xpath = luaL_checkstring(state, 2);

  // Arguments read. Pop them.
  lua_pop(state, 2);

  // Use pugi for xpath query.
  try {
    // Result table and match index.
    lua_newtable(state);
    int matchIndex = 1;

    pugi::xml_document doc;
    doc.load(str.c_str());
    pugi::xpath_query query(xpath.c_str());
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

int lua_util::get_by_regex(lua_State* state) {
  // get arguments from stack
  std::string str = luaL_checkstring(state, 1);
  std::string regex = luaL_checkstring(state, 2);

  // Arguments read. Pop them.
  lua_pop(state, 2);

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

int lua_util::get_all_by_regex(lua_State* state) {
  // Get arguments from stack.
  std::string str = luaL_checkstring(state, 1);
  std::string regex = luaL_checkstring(state, 2);

  // Arguments read. Pop them.
  lua_pop(state, 2);

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

int lua_util::log_debug(lua_State* state) {
  log(state, BS_LOG_DBG);
  return 0;
}

int lua_util::log(lua_State* state) {
  log(state, BS_LOG_NFO);
  return 0;
}

int lua_util::log_error(lua_State* state) {
  log(state, BS_LOG_ERR);
  return 0;
}

int lua_util::set_status(lua_State* state) {
  // Get module name.
  lua_getglobal(state, BOT_MODULE);
  std::string module = luaL_checkstring(state, -1);
  lua_pop(state, 1);

  // Get key and value.
  std::string key = luaL_checkstring(state, -2);
  std::string value = luaL_checkstring(state, -1);

  // Arguments read. Pop them.
  lua_pop(state, 2);

  // Get bot.
  bot* bot = getBot(state);
  if (nullptr == bot) {
    return luaL_error(state, "no bot for state");
  }

  // Execute set command.
  bot->execute(module + "_set_" + key, value);
  return 0;
}

void lua_util::log(lua_State* state, int log_level) {
  // Get module name.
  lua_getglobal(state, BOT_MODULE);
  std::string module = luaL_checkstring(state, -1);
  lua_pop(state, 1);

  // Get and log message.
  std::string message = luaL_checkstring(state, 1);
  lua_pop(state, 1);

  // Get bot.
  bot* bot = lua_connection::bot(state);
  if (nullptr == bot) {
    luaL_error(state, "no bot for state");
    return;
  }

  bot->log(log_level, module, message);
}

void lua_util::login_callback(lua_State* state) {
  // Get bot.
  bot* bot = lua_connection::bot(state);
  if (nullptr == bot) {
    luaL_error(state, "no bot for state");
    return;
  }

  luaL_checktype(state, 1, LUA_TBOOLEAN);
  bool success = lua_toboolean(state, 1);
  bot->login_callback(state);
}

}  // namespace botscript
