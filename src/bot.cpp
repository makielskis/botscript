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

#include "./bot.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>
#include <utility>

#include "boost/bind.hpp"
#include "boost/algorithm/string/join.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/lexical_cast.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "./lua_connection.h"

namespace botscript {

// Initialization of the static bot class attributes.
std::map<std::string, std::string> bot::servers_;
std::vector<std::string> bot::server_lists_;
boost::mutex bot::server_mutex_;
boost::mutex bot::log_mutex_;

bot::bot(boost::asio::io_service* io_service)
  : wait_time_factor_(1),
    stopped_(false),
    connection_status_(1),
    io_service_(io_service) {
  status_["base_wait_time_factor"] = "1";
}

void bot::loadConfiguration(const std::string& configuration, int login_tries)
throw(lua_exception, bad_login_exception, invalid_proxy_exception) {
  rapidjson::Document document;
  if (document.Parse<0>(configuration.c_str()).HasParseError()) {
    // Configuration is not valid JSON. This should NOT happen!
  }

  // Read basic configuration values.
  username_ = document["username"].GetString();
  password_ = document["password"].GetString();
  package_ = document["package"].GetString();
  server_ = document["server"].GetString();
  std::string wait_time_factor = document.HasMember("wait_time_factor")
                                 ? document["wait_time_factor"].GetString()
                                 : "1";

  // Read proxy settings.
  std::string proxy = document.HasMember("proxy")
                      ? document["proxy"].GetString() : "";

  // Create bot.
  init(proxy, 3, false);
  if (wait_time_factor != "1") {
    execute("base_set_wait_time_factor", wait_time_factor);
  }

  // Load module configuration.
  if (document.HasMember("modules")) {
    const rapidjson::Value& a = document["modules"];
    for (rapidjson::Value::ConstMemberIterator i = a.MemberBegin();
         i != a.MemberEnd(); ++i) {
      const rapidjson::Value& m = i->value;
      std::string module = i->name.GetString();
      rapidjson::Value::ConstMemberIterator it = m.MemberBegin();
      for (; it != m.MemberEnd(); ++it) {
        std::string name = it->name.GetString();
        if (name == "name" || name == "active") {
          continue;
        }
        std::string value = it->value.GetString();
        execute(module + "_set_" + name, value);
      }
      std::string active = m["active"].GetString();
      if (active == "1") {
        execute(module + "_set_active", "1");
      }
    }
  }
}

void bot::init(const std::string& proxy, int login_trys, bool check_only_first)
throw(lua_exception, bad_login_exception, invalid_proxy_exception) {
  // Set identifier.
  identifier_ = createIdentifier(username_, package_, server_);

  // Do not create a bot that already exists.
  if (lua_connection::contains(identifier_)) {
    throw lua_exception("bot already registered");
  }

  // Set proxy - if a proxy was used the bot is already logged in.
  setProxy(proxy, check_only_first, login_trys);
  if (proxy.empty()) {
    lua_connection::login(this, username_, password_, package_);
  }

  // Load modules
  using boost::filesystem::directory_iterator;
  if (boost::filesystem::is_directory(package_)) {
    for (directory_iterator i = directory_iterator(package_);
         i != directory_iterator(); ++i) {
      std::string path = i->path().relative_path().generic_string();
      if (!boost::algorithm::ends_with(path, "servers.lua") &&
          !boost::algorithm::ends_with(path, "base.lua") &&
          !boost::starts_with(i->path().filename().string(), ".")) {
        try {
          module* new_module = new module(path, this, io_service_);
          modules_.insert(new_module);
        } catch(const lua_exception& e) {
          log(ERROR, "base", e.what());
        }
      }
    }
  }
}

bool bot::checkProxy(std::string proxy, int login_trys) {
  // Don't accept empty proxies
  if (proxy.empty()) {
    return false;
  }

  // Split proxy to host and port and set it.
  std::vector<std::string> proxy_split;
  boost::split(proxy_split, proxy, boost::is_any_of(":"));
  if (proxy_split.size() != 2) {
    log(ERROR, "base", "unknown proxy format");
    return false;
  }
  webclient_.proxy(proxy_split[0], proxy_split[1]);

  // Try to login.
  std::cout << login_trys << " login trys\n";
  for (int i = 0; i < login_trys; i++) {
    try {
      std::string t = boost::lexical_cast<std::string>(i + 1);
      log(INFO, "base", t + ". try - checking proxy - login");
      lua_connection::login(this, username_, password_, package_);
      return true;
    } catch(const bad_login_exception& e) {
      log(ERROR, "base", "bad login");
    } catch(const lua_exception& e) {
      log(ERROR, "base", e.what());
    }
  }

  return false;
}

void bot::setProxy(const std::string& proxy, bool check_only_first,
                   int login_trys)
throw(invalid_proxy_exception) {
  // Change status.
  status("base_proxy", proxy);

  // Use direct connection for empty proxy.
  if (proxy.empty()) {
    webclient_.proxy("", "");
    return;
  }

  // Clear and fill proxy list with new proxies.
  std::string original_proxy_host = webclient_.proxy_host();
  std::string original_proxy_port = webclient_.proxy_port();
  proxies_.clear();
  boost::split(proxies_, proxy, boost::is_any_of(", \t\n"));
  if (proxies_.empty()) {
    throw invalid_proxy_exception();
  }

  // Check first proxy / proxies.
  std::vector<std::string>::iterator proxy_it;
  if (check_only_first) {
    proxy_it = checkProxy(proxies_[0], login_trys)
               ? proxies_.begin() : proxies_.end();
  } else {
    proxy_it = std::find_if(proxies_.begin(), proxies_.end(),
        boost::bind(&bot::checkProxy, this, _1, login_trys) == true);
  }

  // Check result.
  if (proxy_it == proxies_.end()) {
    webclient_.proxy(original_proxy_host, original_proxy_port);
    log(ERROR, "base", "no working proxy found");
    throw invalid_proxy_exception();
  } else {
    log(INFO, "base", std::string("proxy set to ") + (*proxy_it));
  }
}

bot::~bot() {
  stopped_ = true;
  lua_connection::remove(identifier_);
  BOOST_FOREACH(module* module, modules_) {
    delete module;
  }
}

std::string bot::configuration(bool with_password) {
  // Lock for status r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);

  // Write basic configuration values.
  rapidjson::Document document;
  document.SetObject();
  rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
  document.AddMember("username", username_.c_str(), allocator);
  if (with_password) {
    document.AddMember("password", password_.c_str(), allocator);
  }
  document.AddMember("package", package_.c_str(), allocator);
  document.AddMember("server", server_.c_str(), allocator);
  document.AddMember("wait_time_factor",
                     status_["base_wait_time_factor"].c_str(), allocator);

  // Write proxy.
  std::map<std::string, std::string>::const_iterator i =
      status_.find("base_proxy");
  std::string proxy = (i == status_.end()) ? "" : i->second;
  rapidjson::Value proxy_value(proxy.c_str(), allocator);
  document.AddMember("proxy", proxy_value, allocator);

  // Write module configuration values.
  rapidjson::Value modules(rapidjson::kObjectType);
  BOOST_FOREACH(module* m, modules_) {
    // Initialize module JSON object and add name property.
    std::string module_name = m->name();
    rapidjson::Value module(rapidjson::kObjectType);

    // Add module settings.
    typedef std::pair<std::string, std::string> str_pair;
    BOOST_FOREACH(str_pair j, status_) {
      if (boost::algorithm::starts_with(j.first, module_name)) {
        std::string key = j.first.substr(module_name.length() + 1);
        rapidjson::Value key_attr(key.c_str(), allocator);
        rapidjson::Value val_attr(j.second.c_str(), allocator);
        module.AddMember(key_attr, val_attr, allocator);
      }
    }
    rapidjson::Value name_attr(module_name.c_str(), allocator);
    modules.AddMember(name_attr, module, allocator);
  }
  document.AddMember("modules", modules, allocator);

  // Convert to string.
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);

  return buffer.GetString();
}

void bot::execute(const std::string& command, const std::string& argument) {
  boost::lock_guard<boost::mutex> lock(execute_mutex_);
  if (stopped_) {
    return;
  }

  // Handle wait time factor command.
  if (command == "base_set_wait_time_factor") {
    std::string new_wait_time_factor = argument;
    if (new_wait_time_factor.find(".") == std::string::npos) {
      new_wait_time_factor += ".0";
    }
    wait_time_factor_ = atof(new_wait_time_factor.c_str());
    char buf[5];
    snprintf(buf, sizeof(buf), "%.2f", wait_time_factor_);
    status("base_wait_time_factor", buf);
    log(INFO, "base", std::string("set wait time factor to ") + buf);
    return;
  }

  // Handle set proxy command.
  if (command == "base_set_proxy") {
    std::string proxy_host = webclient_.proxy_host();
    std::string proxy_port = webclient_.proxy_port();
    try {
      setProxy(argument, false, 1);
    } catch(const invalid_proxy_exception& e) {
      log(INFO, "base", "new proxy failed - resetting");
      webclient_.proxy(proxy_host, proxy_port);
    }
    return;
  }

  // Forward all other commands to modules.
  BOOST_FOREACH(module* module, modules_) {
    module->execute(command, argument);
  }
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
  std::string p = boost::filesystem::path(package).filename().string();
  std::string identifier = p + "_";
  if (servers_.find(server) == servers_.end()) {
    identifier += server;
  } else {
    identifier += servers_[server];
  }
  identifier += "_";
  identifier += username;

  return identifier;
}

std::string bot::loadPackages(const std::string& folder) {
  // Lock because of server list r/w access.
  boost::lock_guard<boost::mutex> lock(server_mutex_);

  // Folder parameter has to be a directory.
  if (!boost::filesystem::is_directory(folder)) {
    return "";
  }

  // Iterate packages.
  rapidjson::Document document;
  rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
  document.SetObject();
  rapidjson::Value packages(rapidjson::kArrayType);
  using boost::filesystem::directory_iterator;
  for (directory_iterator i = directory_iterator(folder);
       i != directory_iterator(); ++i) {
    // Check if package is a directory.
    if (!boost::filesystem::is_directory(*i)) {
      continue;
    }

    // Check if servers file exists.
    boost::filesystem::path path = i->path();
    std::string server_list = path.relative_path().string() + "/servers.lua";
    boost::filesystem::path servers_path(server_list);
    if (!boost::filesystem::exists(servers_path)
        || boost::filesystem::is_directory(servers_path)) {
      continue;
    }

    // Write package name.
    rapidjson::Value a(rapidjson::kObjectType);
    rapidjson::Value package_name(path.filename().string().c_str(), allocator);
    a.AddMember("name", package_name, allocator);

    // Write servers from package:
    // Lua table -> map -> JSON array
    rapidjson::Value l(rapidjson::kArrayType);
    std::map<std::string, std::string> s;
    lua_connection::loadServerList(server_list, &s);
    typedef std::pair<std::string, std::string> str_pair;
    BOOST_FOREACH(str_pair server, s) {
      rapidjson::Value server_name(server.first.c_str(), allocator);
      l.PushBack(server_name, allocator);
    }
    a.AddMember("servers", l, allocator);

    // Write interface descriptions from all modules.
    for (directory_iterator j = directory_iterator(i->path());
         j != directory_iterator(); ++j) {
      // Iterate over all module paths but exclude hidden files
      // and special files (servers.lua and base.lua)
      std::string mod_path = j->path().relative_path().generic_string();
      if (!boost::algorithm::ends_with(mod_path, "servers.lua") &&
          !boost::algorithm::ends_with(mod_path, "base.lua") &&
          !boost::starts_with(j->path().filename().string(), ".")) {
        // Extract module name.
        std::string filename = j->path().filename().string();
        rapidjson::Value module_name(
            filename.substr(0, filename.length() - 4).c_str(),
            allocator);

        // Read interface information from lua script.
        jsonval_ptr interface = lua_connection::interface(mod_path, &allocator);

        // Write value to package information.
        a.AddMember(module_name, *interface.get(), allocator);
      }
    }

    // Store package information.
    packages.PushBack(a, allocator);

    // Store result if not already done.
    if (!CONTAINS(server_lists_, server_list)) {
      server_lists_.push_back(server_list);
      servers_.insert(s.begin(), s.end());
    }
  }
  document.AddMember("packages", packages, allocator);

  // Convert to string.
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);

  return buffer.GetString();
}

int bot::randomWait(int a, int b) {
  static unsigned int seed = 6753;
  seed *= 31;
  seed %= 32768;
  double random = static_cast<double>(rand_r(&seed)) / RAND_MAX;
  int wait_time = a + std::round(random * (b - a) * wait_time_factor_);
  return wait_time;
}

void bot::log(int type, const std::string& source, const std::string& message) {
  boost::lock_guard<boost::mutex> lock(log_mutex_);

  // Build date string.
  time_t timestamp;
  std::time(&timestamp);
  struct std::tm current;
  localtime_r(&timestamp, &current);
  char time_str[9];
  std::snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                current.tm_hour, current.tm_min, current.tm_sec);

  // Build log string.
  std::stringstream msg;
  msg << "[" << time_str << "]["\
      << std::left << std::setw(20) << identifier_
      << "][" << std::left << std::setw(8) << source << "] " << message << "\n";

  // Store log message.
  if (log_msgs_.size() > MAX_LOG_SIZE) {
    log_msgs_.pop_front();
  }
  log_msgs_.push_back(msg.str());
  callback(identifier_, "log", msg.str());
}

void bot::callback(std::string id, std::string k, std::string v) {
  callback_(id, k, v);
}

std::string bot::log_msgs() {
  std::stringstream log;
  BOOST_FOREACH(std::string msg, log_msgs_) {
    log << msg;
  }
  return log.str();
}

void bot::connectionFailed(bool connection_error) {
  boost::lock_guard<boost::mutex> lock(connection_status_mutex_);
  connection_status_ -= (connection_error ? 0.1 : 0.025);
  if (connection_status_ <= 0) {
    connection_status_ = 0;
    log(ERROR, "base", "problems - trying to relogin");
    try {
      lua_connection::login(this, username_, password_, package_);
    } catch(const lua_exception& e) {
      execute("base_set_proxy", status("base_proxy"));
    } catch(const bad_login_exception& e) {
      execute("base_set_proxy", status("base_proxy"));
    }
  }
}

void bot::connectionWorked() {
  boost::lock_guard<boost::mutex> lock(connection_status_mutex_);
  connection_status_ += 0.1;
  if (connection_status_ > 1) {
    connection_status_ = 1;
  }
}

std::string bot::status(const std::string key) {
  // Lock because of status map r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);
  std::map<std::string, std::string>::const_iterator i = status_.find(key);
  return (i == status_.end()) ? "" : i->second;
}

void bot::status(const std::string key, const std::string value) {
  // Lock because of status map r/w access.
  {
    boost::lock_guard<boost::mutex> lock(status_mutex_);
    status_[key] = value;
  }
  callback(identifier_, key, value);
}

}  // namespace botscript
