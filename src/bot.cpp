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

#include "boost/algorithm/string/predicate.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "./lua_connection.h"

namespace std {

// round() is missing in C++ until C++ 2011 standard.
// But we have one!
double round(double r) {
  return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}

}  // namespace std

namespace botscript {

// Initialization of the static bot class attributes.
std::map<std::string, std::string> bot::servers_;
std::vector<std::string> bot::server_lists_;
boost::mutex bot::server_mutex_;
boost::mutex bot::log_mutex_;

std::map<std::string, std::string> bot::interface_;
boost::mutex bot::interface_mutex_;

int bot::bot_count_ = 0;
boost::mutex bot::init_mutex_;
boost::asio::io_service* bot::io_service_ = NULL;
boost::asio::io_service::work* bot::work_ = NULL;
boost::thread_group* bot::worker_threads_ = NULL;
bool bot::force_proxy_ = false;

bot::bot(const std::string& username, const std::string& password,
         const std::string& package, const std::string& server,
         const std::string& proxy)
throw(lua_exception, bad_login_exception, invalid_proxy_exception)
  : username_(username),
    password_(password),
    package_(package),
    server_(server),
    wait_time_factor_(1),
    stopped_(false),
    connection_status_(1) {
  status_["base_wait_time_factor"] = "1";
  init(proxy);
}

bot::bot(const std::string& configuration)
throw(lua_exception, bad_login_exception, invalid_proxy_exception)
  : wait_time_factor_(1),
    stopped_(false),
    connection_status_(1) {
  rapidjson::Document document;
  if (document.Parse<0>(configuration.c_str()).HasParseError()) {
    // Configuration is not valid JSON. This should NOT happen!
  }

  // Read basic configuration values.
  username_ = document["username"].GetString();
  password_ = document["password"].GetString();
  package_ = document["package"].GetString();
  server_ = document["server"].GetString();
  std::string wait_time_factor = document["wait_time_factor"].GetString();

  // Read proxy settings.
  std::string proxy = document["proxy"].GetString();

  // Create bot.
  init(proxy);
  execute("base_set_wait_time_factor", wait_time_factor);

  // Load module configuration.
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

void bot::init(const std::string& proxy)
throw(lua_exception, bad_login_exception, invalid_proxy_exception) {
  if (bot_count_ == 0) {
    boost::lock_guard<boost::mutex> lock(init_mutex_);
    if (bot_count_ == 0) {
      // If this is the first bot: initialize io service.
      io_service_ = new boost::asio::io_service();
      work_ = new boost::asio::io_service::work(*io_service_);
      worker_threads_ = new boost::thread_group();
      for(unsigned int i = 0; i < 1; ++i) {
        worker_threads_->create_thread(
                boost::bind(&boost::asio::io_service::run, io_service_));
      }
    }
  }
  bot_count_++;

  // Set proxy if given.
  if (!proxy.empty()) {
    std::string proxy_host, proxy_port;
    std::vector<std::string> proxy_split;
    boost::split(proxy_split, proxy, boost::is_any_of(":"));
    if (proxy_split.size() == 2) {
      proxy_host = proxy_split[0];
      proxy_port = proxy_split[1];
      webclient_.proxy(proxy_host, proxy_port);
    } else {
      throw invalid_proxy_exception();
    }
  }
  identifier_ = createIdentifier(username_, package_, server_);
  lua_connection::login(this, username_, password_, package_);
  loadModules(io_service_);
}

bot::~bot() {
  stopped_ = true;
  lua_connection::remove(identifier_);
  BOOST_FOREACH(module* module, modules_) {
    module->shutdown();
    delete module;
  }

  bot_count_--;
  if (bot_count_ == 0) {
    // If this was the last bot: de-initialize the io service.
    boost::lock_guard<boost::mutex> lock(init_mutex_);
    if (bot_count_ == 0) {
      delete work_;
      io_service_->stop();
      worker_threads_->join_all();
      delete io_service_;
      delete worker_threads_;
    }
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
  if (!webclient_.proxy_host().empty()) {
    std::string proxy = webclient_.proxy_host() + ":" + webclient_.proxy_port();
    rapidjson::Value proxy_value(proxy.c_str(), allocator);
    document.AddMember("proxy", proxy_value, allocator);
  } else {
    document.AddMember("proxy", "", allocator);
  }
  document.AddMember("wait_time_factor",
                     status_["base_wait_time_factor"].c_str(), allocator);

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

std::string bot::interface_description() {
  boost::lock_guard<boost::mutex> lock(interface_mutex_);
  if (interface_.find(package_) == interface_.end()) {
    std::stringstream out;
    out << "{\"interface\":[";
    for (std::set<module*>::iterator m = modules_.begin();;) {
      out << (*m)->interface_description();
      if (++m == modules_.end()) {
        break;
      } else {
        out << ",";
      }
    }
    out << "]}";
    interface_[package_] = out.str();
  }
  return interface_[package_];
}

void bot::execute(const std::string& command, const std::string& argument) {
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
    if (argument.empty()) {
      webclient_.proxy("", "");
      log(INFO, "base", "proxy reset");
      return;
    }
    std::vector<std::string> proxy_split;
    boost::split(proxy_split, argument, boost::is_any_of(":"));
    if (proxy_split.size() != 2) {
      log(ERROR, "base", "unknown proxy format");
      return;
    }
    webclient_.proxy(proxy_split[0], proxy_split[1]);
    log(INFO, "base", "proxy set");
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

    // Write servers from package.
    rapidjson::Value l(rapidjson::kArrayType);
    std::map<std::string, std::string> s;
    lua_connection::loadServerList(server_list, &s);
    typedef std::pair<std::string, std::string> str_pair;
    BOOST_FOREACH(str_pair server, s) {
      rapidjson::Value server_name(server.first.c_str(), allocator);
      l.PushBack(server_name, allocator);
    }
    a.AddMember("servers", l, allocator);
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

void bot::loadModules(boost::asio::io_service* io_service) {
  using boost::filesystem::directory_iterator;
  if (boost::filesystem::is_directory(package_)) {
    for (directory_iterator i = directory_iterator(package_);
         i != directory_iterator(); ++i) {
      std::string path = i->path().relative_path().generic_string();
      if (!boost::algorithm::ends_with(path, "servers.lua") &&
          !boost::algorithm::ends_with(path, "base.lua") &&
          !boost::starts_with(i->path().filename().string(), ".")) {
        try {
          module* new_module = new module(path, this, io_service);
          modules_.insert(new_module);
        } catch(const lua_exception& e) {
          log(ERROR, "base", e.what());
        }
      }
    }
  }
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
      // TODO(felix) set next proxy
    } catch(const bad_login_exception& e) {
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
  boost::lock_guard<boost::mutex> lock(status_mutex_);
  status_[key] = value;
  callback(identifier_, key, value);
}

}  // namespace botscript
