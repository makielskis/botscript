// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./bot.h"

#include <sstream>

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
boost::mutex bot::log_mutex_;
std::map<std::string, std::shared_ptr<botscript::package>> bot::packages_;

bot::bot(boost::asio::io_service* io_service)
    : io_service_(io_service),
      wait_time_factor_(1.0f),
      login_result_stored_(false),
      login_result_(false),
      proxy_check_active_(false) {
}

bot::~bot() {
  std::stringstream msg;
  msg << "bot::~bot() " << identifier_;
  log(BS_LOG_ERR, "base", msg.str());
}

void bot::shutdown() {
  lua_connection::remove(identifier_);
  for (const auto m : modules_) {
    m->execute("global_set_active", "0");
  }
  modules_.clear();

  std::stringstream msg;
  msg << "login_callback " << (nullptr == login_cb_ ? "not " : "") << "set";
  log(BS_LOG_DBG, "base", msg.str());
}

std::string bot::username()    const { return username_; }
std::string bot::password()    const { return password_; }
std::string bot::identifier()  const { return identifier_; }
std::string bot::package()     const { return package_; }
std::string bot::server()      const { return server_; }
double bot::wait_time_factor() const { return wait_time_factor_; }
bot_browser* bot::browser()          { return browser_.get(); }

std::vector<std::string> bot::load_packages(const std::string& p) {
  // Iterate specified directory.
  using boost::filesystem::directory_iterator;
  for (auto i = directory_iterator(p); i != directory_iterator(); ++i) {
    // Get file path and filename.
    std::string path = i->path().relative_path().generic_string();
    std::string name = i->path().filename().string();

    // Don't read hidden files.
    if (boost::starts_with(name, ".")) {
      continue;
    }

    // Discover package name.
    std::string stripped_path = path;
    std::size_t dot_pos = path.find_last_of(".");
    if (dot_pos != std::string::npos && dot_pos != 0) {
      stripped_path = path.substr(0, dot_pos);
    }

    // Strip until last slash.
    std::size_t slash_pos = stripped_path.find_last_of("/");
    if (slash_pos != std::string::npos) {
      stripped_path = stripped_path.substr(slash_pos + 1);
    }

    // Load modules (either from lib or from file).
    std::map<std::string, std::string> modules;
    bool dir = boost::filesystem::is_directory(path);
    if (dir) {
      modules = package::from_folder(path);
    } else {
      modules = package::from_lib(path);
    }

    // Check whether base and servers "modules" are contained.
    if (modules.find("base") == modules.end() ||
        modules.find("servers") == modules.end()) {
      std::cerr << "fatal: " << path << " doesn't contain base/servers\n";
      continue;
    }

    // Store.
    packages_[stripped_path] =
        std::make_shared<botscript::package>(stripped_path, modules, !dir);
  }

  // Generate interface description vector.
  std::vector<std::string> interface;
  for (const auto& p : packages_) {
    interface.emplace_back(p.second->interface());
  }

  return interface;
}

void bot::init(const std::string& config, const error_cb& cb) {
  // Get shared pointer to keep alive.
  std::shared_ptr<bot> self = shared_from_this();

  // Read JSON.
  rapidjson::Document document;
  if (document.Parse<0>(config.c_str()).HasParseError()) {
    return cb(self, "invalid JSON");
  }

  // Check if all necessary configuration values are available.
  if (!document.HasMember("modules") ||
      !document["modules"].IsObject() ||
      !document["modules"].HasMember("base") ||
      !document["modules"]["base"].HasMember("wait_time_factor") ||
      !document["modules"]["base"]["wait_time_factor"].IsString() ||
      !document["modules"]["base"].HasMember("proxy") ||
      !document["modules"]["base"]["proxy"].IsString() ||
      !document.HasMember("username") ||
      !document.HasMember("password") ||
      !document.HasMember("package") ||
      !document.HasMember("server") ||
      !document["username"].IsString() ||
      !document["password"].IsString() ||
      !document["package"].IsString() ||
      !document["server"].IsString()) {
    return cb(self, "invalid configuration");
  }

  // Read basic configuration values.
  username_ = document["username"].GetString();
  password_ = document["password"].GetString();
  package_ = document["package"].GetString();
  server_ = document["server"].GetString();

  // Do not load a bot with a non existent package.
  if (packages_.find(package_) == packages_.end()) {
    return cb(self, "package not found");
  }

  // Generate identifier.
  identifier_ = identifier(username_, package_, server_);

  // Do not create a bot that already exists.
  if (lua_connection::contains(identifier_)) {
    return cb(self, "bot already registered");
  }
  lua_connection::add(shared_from_this());

  // Read and set wait time factor.
  std::string wtf = document["modules"]["base"]["wait_time_factor"].GetString();
  wtf = wtf.empty() ? "1.00" : wtf;
  execute("base_set_wait_time_factor", wtf);

  // Read proxy settings.
  std::string proxy = document["modules"]["base"]["proxy"].GetString();

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

      // Base module had been handled previously.
      if (module == "base") {
        continue;
      }

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

  // Instantiate browser.
  browser_ = std::make_shared<bot_browser>(io_service_, self);

  // Set proxy if available.
  if (!proxy.empty()) {
    browser_->set_proxy_list(proxy, [this, commands, cb, self](int success) {
      if (!success) {
        return cb(self, "no working proxy found");
      } else {
        log(BS_LOG_NFO, "base", "login: 1. try");
        std::shared_ptr<state_wrapper> s = std::make_shared<state_wrapper>();
        login_cb_ = boost::bind(&bot::handle_login, this,
                                self, s, _1, cb, commands, true, 2);
        lua_connection::login(
            s->get(), self,
            packages_[package_]->modules().find("base")->second,
            &login_cb_);
      }
    });
  } else {
    log(BS_LOG_NFO, "base", "login: 1. try");
    std::shared_ptr<state_wrapper> s = std::make_shared<state_wrapper>();
    login_cb_ = boost::bind(&bot::handle_login, this,
                            self, s, _1, cb, commands, true, 2);
    lua_connection::login(s->get(), self,
                          packages_[package_]->modules().find("base")->second,
                          &login_cb_);
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

  // Write module configuration values.
  rapidjson::Value modules(rapidjson::kObjectType);
  for(const auto& m : modules_) {
    // Initialize module JSON object and add name property.
    std::string module_name = m->name();
    rapidjson::Value module(rapidjson::kObjectType);

    // Add module settings.
    for(const auto& j : status_) {
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

  rapidjson::Value base_module(rapidjson::kObjectType);

  // Write pseudo module "base" containig the wait time factor and proxy.
  base_module.AddMember("wait_time_factor",
                        status_["base_wait_time_factor"].c_str(), allocator);

  // Write proxy.
  std::map<std::string, std::string>::const_iterator i =
      status_.find("base_proxy");
  std::string proxy = (i == status_.end()) ? "" : i->second;
  rapidjson::Value proxy_value(proxy.c_str(), allocator);
  base_module.AddMember("proxy", proxy_value, allocator);

  // Add base module to modules.
  modules.AddMember("base", base_module, allocator);

  // Add modules to configuration.
  document.AddMember("modules", modules, allocator);

  // Convert to string.
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);

  return buffer.GetString();
}

void bot::handle_login(std::shared_ptr<bot> self,
                       std::shared_ptr<state_wrapper> state_wr,
                       const std::string& err,
                       const error_cb& cb,
                       const command_sequence& init_commands, bool load_mod,
                       int tries) {
  if (!err.empty()) {
    login_result_stored_ = false;

    log(BS_LOG_NFO, "base", std::string("login failed: ") + err);

    if (tries == 0) {
      cb(self, err);
      login_cb_ = nullptr;
      return;
    } else {
      browser_->change_proxy();
      std::shared_ptr<state_wrapper> next_state = std::make_shared<state_wrapper>();
      login_cb_ = boost::bind(&bot::handle_login, this,
                              self, next_state, _1, cb, init_commands,
                              load_mod, tries - 1);
      std::string t =  boost::lexical_cast<std::string>(4 - tries);
      log(BS_LOG_NFO, "base", std::string("login: ") + t + ". try");
      lua_connection::login(next_state->get(), shared_from_this(),
                            packages_[package_]->modules().find("base")->second,
                            &login_cb_);
    }
  } else {
    lua_State* state = state_wr->get();

    if (!login_result_stored_) {
      assert(lua_isboolean(state, -1));
      login_result_stored_ = true;
      login_result_ = static_cast<bool>(lua_toboolean(state, -1));
      lua_pop(state, 1);
    } else {
      login_result_stored_ = false;
      if (!login_result_ && tries == 0) {
        cb(self, "Login -> not logged in (wrong login data?)");
        login_cb_ = nullptr;
        return;
      } else if (!login_result_ && tries > 0) {
        browser_->change_proxy();
        std::shared_ptr<state_wrapper> next_state
            = std::make_shared<state_wrapper>();
        login_cb_ = boost::bind(&bot::handle_login, this,
                                self, next_state, _1, cb, init_commands,
                                load_mod, tries - 1);
        std::string t =  boost::lexical_cast<std::string>(4 - tries);
        log(BS_LOG_NFO, "base", std::string("login: ") + t + ". try");
        lua_connection::login(
            next_state->get(), shared_from_this(),
            packages_[package_]->modules().find("base")->second,
            &login_cb_);
      } else /* if (login_result_) */ {
        if (load_mod) {
          load_modules(init_commands);
        }
        cb(self, "");
        login_cb_ = nullptr;
        return;
      }
    }
  }
}

void bot::load_modules(const command_sequence& init_commands) {
  // Load modules from package folder:
  const std::string& base_script = packages_[package_]->modules().find("base")->second;
  std::shared_ptr<bot> self = shared_from_this();
  for (const auto& m : packages_[package_]->modules()) {
    // base and servers ain't no modules.
    if (m.first == "base" || m.first == "servers") {
      continue;
    }

    // Create module.
    auto new_module = std::make_shared<module>(m.first,
                                               base_script, m.second,
                                               self, io_service_);

    // Check load success.
    if (new_module->load_success()) {
      modules_.push_back(new_module);
    } else {
      log(BS_LOG_ERR, "base", m.first + " could not be loaded");
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
  std::string print_package = package;
  std::size_t slash_pos = print_package.find("/");
  if (slash_pos != std::string::npos) {
    print_package = print_package.substr(slash_pos + 1);
  }
  std::string identifier = print_package + "_";
  identifier += packages_[package]->tag(server);
  identifier += "_";
  identifier += username;
  return identifier;
}

int bot::random(int a, int b) {
  // Generate non-random number. That's enough for our purposes.
  // With many bots it's already very hard (impossilbe?) to predict the result.
  static unsigned int seed = 6753;
  seed *= 31;
  seed %= 32768;
  double r = seed / static_cast<double>(32768);
  int wait_time = a + static_cast<int>(std::round(r * (b - a) * wait_time_factor_));
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

void bot::refresh_status(const std::string& key) {
  auto i = status_.find(key);
  if (i != status_.end()) {
    callback_(identifier_, i->first, i->second);
  }
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
  io_service_->post(boost::bind(&bot::internal_exec, this,
                                command, argument, shared_from_this()));
}

void bot::internal_exec(const std::string& command, const std::string& argument,
                        std::shared_ptr<bot> self) {
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
    if (proxy_check_active_) {
      log(BS_LOG_ERR, "base", "another proxy check is currently active");
      return;
    }

    proxy_check_active_ = true;
    browser_->set_proxy_list(argument, [this, self](int success) {
      if (!success) {
        proxy_check_active_ = false;
        log(BS_LOG_ERR, "base", "no new working proxy found");
      } else {
        log(BS_LOG_NFO, "base", "login: 1. try");
        command_sequence commands;
        auto cb = [this](std::shared_ptr<bot>, std::string err) {
          if (!err.empty()) log(BS_LOG_ERR, "base", err);
          proxy_check_active_ = false;
        };
        std::shared_ptr<state_wrapper> state = std::make_shared<state_wrapper>();
        login_cb_ = boost::bind(&bot::handle_login, this,
                                self, state, _1, cb, commands, false, 2);
        lua_connection::login(
            state->get(), self,
            packages_[package_]->modules().find("base")->second,
            &login_cb_);
      }
    });
  }

  // Forward all other commands to modules.
  for (auto module : modules_) {
    module->execute(command, argument);
  }
}

}  // namespace botscript
