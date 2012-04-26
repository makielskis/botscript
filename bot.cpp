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

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <set>

#include "boost/algorithm/string/predicate.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/foreach.hpp"

#include "rapidjson/document.h"

#include "./bot.h"

#include "./lua_connection.h"

namespace std {
double round(double r) {
  return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}
}  // namespace std

namespace botscript {


// Initialization of the static bot class attributes.
std::map<std::string, std::string>bot::servers_;
std::vector<std::string> bot::server_lists_;
boost::mutex bot::server_mutex_;
boost::mutex bot::log_mutex_;

bot::bot(const std::string& username, const std::string password,
         const std::string& package, const std::string server,
         const std::string& proxy_host, const std::string& proxy_port,
         boost::asio::io_service* io_service)
throw(lua_exception, bad_login_exception)
  : username_(username),
    password_(password),
    package_(package),
    server_(server),
    lua_state_(NULL) {
  if (!proxy_host.empty()) {
    webclient_.proxy(proxy_host, proxy_port);
  }
  identifier_ = createIdentifier(username, package, server);
  lua_state_ = lua_connection::login(this, username_, password_, package_);
  loadModules(io_service);
}

bot::~bot() {
  lua_connection::remove(identifier_);
  lua_close(lua_state_);
  BOOST_FOREACH(module* module, modules_) {
    module->shutdown();
    delete module;
  }
}

bot* bot::load(const std::string& config, boost::asio::io_service* io_service) {
  bot* bot = NULL;
  rapidjson::Document document;
  if (document.Parse<0>(config.c_str()).HasParseError()) {
    // Configuration is not valid JSON. Return null.
    return NULL;
  }

  // Read basic configuration values.
  std::string username = document["username"].GetString();
  std::string password = document["password"].GetString();
  std::string package = document["package"].GetString();
  std::string server = document["server"].GetString();
  std::string wait_time_factor = document["wait_time_factor"].GetString();

  // Read proxy settings.
  std::string proxy = document["proxy"].GetString();
  std::string proxy_host, proxy_port;
  std::vector<std::string> proxy_split;
  boost::split(proxy_split, proxy, boost::is_any_of(":"));
  if (proxy_split.size() == 2) {
    proxy_host = proxy_split[0];
    proxy_port = proxy_split[1];
  }

  // Create bot.
  bot = new botscript::bot(username, password, package, server,
                           proxy_host, proxy_port, io_service);
  bot->execute("base_set_wait_time_factor", wait_time_factor);

  // Load module configuration.
  const rapidjson::Value& a = document["modules"];
  for (rapidjson::SizeType i = 0; i < a.Size(); i++) {
    const rapidjson::Value& m = a[i];
    std::string module = m["name"].GetString();
    rapidjson::Value::ConstMemberIterator it = m.MemberBegin();
    for (; it != m.MemberEnd(); ++it) {
      std::string name = it->name.GetString();
      if (name == "name" || name == "active") {
        continue;
      }
      std::string value = it->value.GetString();
      bot->execute(module + "_set_" + name, value);
    }
    std::string active = m["active"].GetString();
    if (active == "1") {
      bot->execute(module + "_set_active", "1");
    }
  }

  return bot;
}

std::string bot::configuration() {
  // Lock for status r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);

  // Write basic configuration values.
  std::stringstream config;
  config << "{\"username\":\"" << username_ << "\",";
  config << "\"password\":\"" << password_ << "\",";
  config << "\"package\":\"" << package_ << "\",";
  config << "\"server\":\"" << server_ << "\",";
  if (!webclient_.proxy_host().empty()) {
    config << "\"proxy\":\"" << webclient_.proxy_host() << ":"\
           << webclient_.proxy_port() << "\",";
  } else {
    config << "\"proxy\":\"\",";
  }
  config << "\"wait_time_factor\":\"" << status_["wait_time_factor"] << "\",";

  // Write module configuration values.
  config << "\"modules\":[";
  for (std::set<module*>::iterator i = modules_.begin();;) {
    module* module = *i;
    std::string module_name = module->name();
    config << "{\"name\":\"" << module_name << "\"";
    typedef std::map<std::string, std::string>::iterator map_iter;
    for (map_iter j = status_.begin(); j != status_.end(); ++j) {
      if (boost::algorithm::starts_with(j->first, module_name)) {
        config << ",\"" << j->first.substr(module_name.length() + 1) << "\":";
        config << "\"" << j->second << "\"";
      }
    }
    config << "}";

    if (++i == modules_.end()) {
      break;
    } else {
      config << ",";
    }
  }
  config << "]}";
  return config.str();
}

void bot::execute(const std::string& command, const std::string& argument) {
  if (command == "base_set_wait_time_factor") {
    std::string new_wait_time_factor = argument;
    if (new_wait_time_factor.find(".") == std::string::npos) {
      new_wait_time_factor += ".0";
    }
    wait_time_factor_ = atof(new_wait_time_factor.c_str());
    char buf[5];
    snprintf(buf, sizeof(buf), "%.2f", wait_time_factor_);
    status("wait_time_factor", buf);
    log(INFO, "base", std::string("set wait time factor to ") + buf);
    return;
  }

  if (command == "base_set_proxy") {
    std::vector<std::string> proxy_split;
    boost::split(proxy_split, argument, boost::is_any_of(":"));
    if (proxy_split.size() != 2) {
      log(ERROR, "base", "unknown proxy format");
      return;
    }
    webclient_.proxy(proxy_split[0], proxy_split[1]);
    log(INFO, "base", "proxy set");
  }

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

void bot::loadModules(boost::asio::io_service* io_service)  {
  using boost::filesystem::directory_iterator;
  if (boost::filesystem::is_directory(package_)) {
    for (directory_iterator i = directory_iterator(package_);
         i != directory_iterator(); ++i) {
      std::string path = i->path().relative_path().generic_string();
      if (!boost::algorithm::ends_with(path, "servers.lua") &&
          !boost::algorithm::ends_with(path, "base.lua")) {
        modules_.insert(new module(path, this, lua_state_, io_service));
      }
    }
  }
}

int bot::randomWait(int a, int b) {
  unsigned int seed = 32336753;
  double random = static_cast<double>(rand_r(&seed)) / RAND_MAX;
  int wait_time = a + std::round(random * (b - a));
  wait_time *= wait_time_factor_;
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

  // Print log message.
  std::cout << msg.str();

  // Store log message.
  if (log_msgs_.size() > MAX_LOG_SIZE) {
    log_msgs_.pop_front();
  }
  log_msgs_.push_back(msg.str());
}

std::string bot::status(const std::string key) {
  // Lock because of status map r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);
  std::map<std::string, std::string>::const_iterator i = status_.find(key);
  return (i == status_.end()) ? "" : i->second;
}

void bot::status(const std::string key, const std::string value) {
  // Lock because of status map r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);
  status_[key] = value;
}

}  // namespace botscript
