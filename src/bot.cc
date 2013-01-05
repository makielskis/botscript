// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./bot.h"

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "./lua/lua_connection.h"

#if defined _WIN32 || defined _WIN64
namespace std {

// round() is missing in until C++11 standard.
// But we have one!
double round(double r) {
  return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}

}  // namespace std
#endif

namespace botscript {

// Initialization of the static bot class attributes.
std::map<std::string, std::string> bot::servers_;
std::vector<std::string> bot::server_lists_;
boost::mutex bot::server_mutex_;
boost::mutex bot::log_mutex_;

bot::bot(boost::asio::io_service* io_service)
    : io_service_(io_service),
      wait_time_factor_(1.0f) {
  browser_ = std::make_shared<bot_browser>(io_service, this);
}

bot::~bot() {
  std::cout << "deleting " << identifier_ << "\n";
}

void bot::shutdown() {
  lua_connection::remove(identifier_);
  for (const auto m : modules_) {
    m->execute("global_set_active", "0");
  }
  modules_.clear();
}

std::string bot::username()    const { return username_; }
std::string bot::password()    const { return password_; }
std::string bot::identifier()  const { return identifier_; }
std::string bot::package()     const { return package_; }
std::string bot::server()      const { return server_; }
double bot::wait_time_factor() const { return wait_time_factor_; }
bot_browser* bot::browser()          { return browser_.get(); }

void bot::init(const std::string& config, const error_callback& cb) {
  // Read JSON.
  rapidjson::Document document;
  if (document.Parse<0>(config.c_str()).HasParseError()) {
    return cb("invalid JSON");
  }

  // Check if all necessary configuration values are available.
  if (!document.HasMember("username") ||
      !document.HasMember("password") ||
      !document.HasMember("package") ||
      !document.HasMember("server") ||
      !document["username"].IsString() ||
      !document["password"].IsString() ||
      !document["package"].IsString() ||
      !document["server"].IsString()) {
    return cb("invalid configuration");
  }

  // Read basic configuration values.
  username_ = document["username"].GetString();
  password_ = document["password"].GetString();
  package_ = document["package"].GetString();
  server_ = document["server"].GetString();
  identifier_ = identifier(username_, package_, server_);

  // Do not create a bot that already exists.
  if (lua_connection::contains(identifier_)) {
    return cb("bot already registered");
  }
  lua_connection::add(shared_from_this());

  // Read wait time factor.
  std::string wait_time_factor;
  if (document.HasMember("wait_time_factor") &&
      document["wait_time_factor"].IsString()) {
    wait_time_factor = document["wait_time_factor"].GetString();
  } else {
    wait_time_factor = "1";
  }

  // Read proxy settings.
  std::string proxy;
  if (document.HasMember("proxy") && document["proxy"].IsString()) {
    proxy = document["proxy"].GetString();
  } else {
    proxy = "";
  }

  // Set wait time factor.
   execute("base_set_wait_time_factor", wait_time_factor);

  // Create execute command sequence from module configurations
  // for later execution (after the login has been performed).
  command_sequence commands;
  if (document.HasMember("modules") && document["modules"].IsObject()) {
    const rapidjson::Value& a = document["modules"];
    for (rapidjson::Value::ConstMemberIterator i = a.MemberBegin();
         i != a.MemberEnd(); ++i) {
      const rapidjson::Value& m = i->value;

      // Ignore modules that are no objects.
      if (!m.IsObject()) {
        continue;
      }

      // Extract module name.
      std::string module = i->name.GetString();

      // Iterate module settings.
      rapidjson::Value::ConstMemberIterator it = m.MemberBegin();
      for (; it != m.MemberEnd(); ++it) {
        // Read property name.
        std::string name = it->name.GetString();

        // Ignore "active" because the module should be startet
        // after the complete state has been read.
        // Ignore "name" because the module name is not relevant for the state.
        if (name == "name" || name == "active") {
          continue;
        }

        // Set module status variable.
        std::string command = module + "_set_" + name;
        std::string value = it->value.GetString();
        commands.emplace_back(command, value);
      }

      // Read active status.
      std::string active;
      if (m.HasMember("active") && m["active"].IsString()) {
        active = m["active"].GetString();
      } else {
        active = "0";
      }

      // Start module if active status is "1" (=active).
      if (active == "1") {
        commands.emplace_back(module + "_set_active", "1");
      }
    }
  }

  // Set proxy if available.
  if (!proxy.empty()) {
    std::shared_ptr<bot> self = shared_from_this();
    browser_->set_proxy_list(proxy, [this, commands, cb, self](int success) {
      if (!success) {
        return cb("no working proxy found");
      } else {
        log(BS_LOG_NFO, "base", "login: 1. try");
        login_cb_ = boost::bind(&bot::handle_login, this, _1, cb, commands, 2);
        lua_connection::login(self, &login_cb_);
      }
    });
  } else {
    log(BS_LOG_NFO, "base", "login: 1. try");
    login_cb_ = boost::bind(&bot::handle_login, this, _1, cb, commands, 2);
    lua_connection::login(shared_from_this(), &login_cb_);
  }
}

void bot::handle_login(const std::string& err,
                       const error_callback& cb,
                       const command_sequence& init_commands,
                       int tries) {
  // Call final callback if the login was successful
  // or there are no tries remaining.
  if (!err.empty() && tries == 0) {
    return cb(err);
  } else if (err.empty()) {
    load_modules(init_commands);
    return cb("");
  }

  // Login failed, but we have tries remaining.
  log(BS_LOG_ERR, "base", err);

  // Write information to the log.
  std::string t =  boost::lexical_cast<std::string>(4 - tries);
  log(BS_LOG_NFO, "base", std::string("login: ") + t + ". try");

  // Start another try.
  login_cb_ = boost::bind(&bot::handle_login, this,
                          _1, cb, init_commands, tries - 1);
  lua_connection::login(shared_from_this(), &login_cb_);
}

void bot::load_modules(const command_sequence& init_commands) {
  // Load modules from package folder:
  // Read every non-hidden file except servers.lua and base.lua
  using boost::filesystem::directory_iterator;
  if (boost::filesystem::is_directory(package_)) {
    for (directory_iterator i = directory_iterator(package_);
         i != directory_iterator(); ++i) {
      std::string path = i->path().relative_path().generic_string();
      if (!boost::algorithm::ends_with(path, "servers.lua") &&
          !boost::algorithm::ends_with(path, "base.lua") &&
          !boost::starts_with(i->path().filename().string(), ".")) {
        try {
          auto m = std::make_shared<module>(path, shared_from_this(),
                                            io_service_);
          modules_.push_back(m);
        } catch(lua_exception) {
          log(BS_LOG_ERR, "base", path + " could not be loaded");
        }
      }
    }
  }

  // Initialize modules.
  for(auto command : init_commands) {
    execute(command.first, command.second);
  }
}

std::string bot::identifier(const std::string& username,
                            const std::string& package,
                            const std::string& server) {
  // Lock because of server list r/w access.
  boost::lock_guard<boost::mutex> lock(server_mutex_);

  // Load servers from package.
  std::string server_list = package + "/servers.lua";
  if (!CONTAINS(server_lists_, server_list)) {
    lua_connection::server_list(server_list, &servers_);
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

std::string bot::load_packages(const std::string& folder) {
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
    lua_connection::server_list(server_list, &s);
    for(const auto& server : s) {
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
        jsonval_ptr interface = lua_connection::iface(mod_path, &allocator);

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

int bot::random(int a, int b) {
  // Generate non-random number. That's enough for our purposes.
  // With many bots it's already very hard (impossilbe?) to predict the result.
  static unsigned int seed = 6753;
  seed *= 31;
  seed %= 32768;
  double r = seed / static_cast<double>(32768);
  int wait_time = a + std::round(r * (b - a) * wait_time_factor_);
  return wait_time;
}

void bot::log(int type, const std::string& source, const std::string& message) {
  boost::lock_guard<boost::mutex> lock(log_mutex_);

  // Build date string.
  std::stringstream time;
  boost::posix_time::time_facet* p_time_output =
      new boost::posix_time::time_facet();
  std::locale special_locale(std::locale(""), p_time_output);
  // special_locale takes ownership of the p_time_output facet
  time.imbue(special_locale);
  p_time_output->format("%d.%m %H:%M:%S");
  time << boost::posix_time::second_clock::local_time();

  // Determine type.
  std::string type_str;
  switch(type) {
    case BS_LOG_DBG: type_str = "[DEBUG]"; break;
    case BS_LOG_NFO: type_str = "[INFO ]"; break;
    case BS_LOG_ERR: type_str = "[ERROR]"; break;
  }

  // Build log string.
  std::stringstream msg;
  msg << type_str << "[" << time.str() << "]["\
      << std::left << std::setw(20) << identifier_
      << "][" << std::left << std::setw(8) << source << "] " << message << "\n";

  // Store log message.
  if (log_msgs_.size() > MAX_LOG_SIZE) {
    log_msgs_.pop_front();
  }
  log_msgs_.push_back(msg.str());
  callback_(identifier_, "log", msg.str());
}

std::string bot::log_msgs() {
  std::stringstream log;
  for(const std::string& msg : log_msgs_) {
    log << msg;
  }
  return log.str();
}

std::string bot::status(const std::string key) {
  // Lock because of status map r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);
  auto i = status_.find(key);
  return (i == status_.end()) ? "" : i->second;
}

void bot::status(const std::string key, const std::string value) {
  {
    boost::lock_guard<boost::mutex> lock(status_mutex_);
    status_[key] = value;
  }
  callback_(identifier_, key, value);
}

std::map<std::string, std::string> bot::module_status(
    const std::string& module) {
  // Lock for status r/w access.
  boost::lock_guard<boost::mutex> lock(status_mutex_);

  // Read module settings.
  std::map<std::string, std::string> module_status;
  for(const auto& j : status_) {
    if (boost::algorithm::starts_with(j.first, module)) {
      std::string key = j.first.substr(module.length() + 1);
      module_status[key] = j.second;
    }
  }

  return module_status;
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
    sprintf(buf, "%.2f", wait_time_factor_);
    status("base_wait_time_factor", buf);
    log(BS_LOG_NFO, "base", std::string("set wait time factor to ") + buf);
    return;
  }

  // Handle set proxy command.
  if (command == "base_set_proxy") {
    // TODO set proxy list
  }

  // Forward all other commands to modules.
  for (auto module : modules_) {
    module->execute(command, argument);
  }
}

}  // namespace botscript
