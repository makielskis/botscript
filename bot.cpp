/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 15. April 2012  makielskis@gmail.com
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

#include <map>
#include <vector>
#include <string>

#include "boost/algorithm/string/predicate.hpp"

#include "./bot.h"

#include "./lua_connection.h"

namespace botscript {

// Initialization of the static bot class attributes.
std::map<std::string, std::string>bot::servers_;
std::vector<std::string> bot::server_lists_;
boost::mutex bot::server_mutex_;

bot::bot(const std::string& username, const std::string password,
         const std::string& package, const std::string server)
throw(lua_exception, bad_login_exception)
  : username_(username),
    password_(password),
    package_(package),
    server_(server),
    lua_state_(NULL) {
  identifier_ = createIdentifier(username, package, server);
  lua_state_ = lua_connection::login(this, username_, password_, package_);
  loadModules();
}

bot::~bot() {
  lua_connection::remove(identifier_);
  lua_close(lua_state_);
}

std::string bot::createIdentifier(const std::string& username,
                                    const std::string& package,
                                    const std::string& server) {
  // Lock because of server list r/w access.
  boost::lock_guard<boost::mutex> lock(server_mutex_);

  // Load servers from package.
  std::string server_list = package + "/servers.lua";
  if (!CONTAINS(server_lists_, server_list)) {
    lua_connection::loadServerList(server_list, &servers_);
    server_lists_.push_back(server_list);
  }

  // Create identifier.
  std::string identifier = package + "_";
  if (servers_.find(server) == servers_.end()) {
    identifier += server;
  } else {
    identifier += servers_[server];
  }
  identifier += "_";
  identifier += username;

  return identifier;
}

void bot::loadModules()  {
  using namespace boost::filesystem;
  if (is_directory(package_)) {
    for (directory_iterator i = directory_iterator(package_);
         i != directory_iterator(); ++i) {
      std::string path = i->path().relative_path().generic_string();
      if (!boost::algorithm::ends_with(path, "servers.lua") &&
          !boost::algorithm::ends_with(path, "base.lua")) {
        module_ptr new_module = module_ptr(new module(path, this, lua_state_));
        modules_.insert(new_module);
      }
    }
  }
}

}  // namespace botscript
