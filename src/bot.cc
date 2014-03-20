// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./bot.h"

#include <sstream>
#include <stdexcept>

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"

#include "./rapidjson_with_exception.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "./lua/lua_connection.h"
#include "./mem_bot_config.h"

#if defined(ANDROID) || defined(STATIC_PACKAGES)
#include "./pg.h"
#include "./kv.h"
#endif

// using Visual Studio 2012 or older
#if (defined _MSC_VER && _MSC_VER <= 1700) || ANDROID
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
std::map<std::string, std::shared_ptr<botscript::package>> bot::packages_;

bot::bot(boost::asio::io_service* io_service)
    : io_service_(io_service),
      wait_time_factor_(1.0f),
      login_result_stored_(false),
      login_result_(false),
      proxy_check_active_(false) {
}

bot::~bot() {
}

void bot::shutdown() {
  configuration_ = std::make_shared<mem_bot_config>();

  lua_connection::remove(identifier_);
  for (const auto m : modules_) {
    m->execute("global_set_active", "0");
  }
  modules_.clear();

  std::stringstream msg;
  msg << "login_callback " << (nullptr == login_cb_ ? "not " : "") << "set";
  log(BS_LOG_DBG, "base", msg.str());

  update_callback_ = nullptr;
}

std::shared_ptr<bot_config> bot::config() { return configuration_; }

bot_browser* bot::browser() { return browser_.get(); }

void bot::load_packages(const std::string& p) {
#if defined(ANDROID) || defined(STATIC_PACKAGES)
  typedef std::map<std::string, std::string> mod_map;
  std::unique_ptr<mod_map> pg(static_cast<mod_map*>(load_pg()));
  std::unique_ptr<mod_map> kv(static_cast<mod_map*>(load_kv()));
  packages_["kv"] = std::make_shared<package>("kv", *kv.get());
  packages_["pg"] = std::make_shared<package>("pg", *pg.get());
#else
  // Check that the specified path is a directory.
  if (!boost::filesystem::is_directory(p)) {
    std::cout << p << " isn't a directory\n";
    return;
  }

  // Iterate specified directory.
  packages_.clear();
  using boost::filesystem::directory_iterator;
  for (auto i = directory_iterator(p); i != directory_iterator(); ++i) {
    // Don't load hidden files.
    if (boost::starts_with(i->path().filename().string(), ".")) {
      continue;
    }

    // Store.
    std::string module_path = i->path().generic_string();
    try {
      auto module = std::make_shared<package>(module_path);
      packages_[module->name()] = module;
    } catch (const std::runtime_error& e) {
      std::cout << "Unable to load module at " << module_path
                << ", error: " << e.what() << std::endl;
    }
  }
#endif
}

void bot::init(std::shared_ptr<bot_config> configuration, const error_cb& cb) {
  // Get shared pointer to keep alive.
  std::shared_ptr<bot> self = shared_from_this();

  // Set configuration.
  configuration_ = configuration;

  // Check package information.
  const auto package_it = packages_.find(configuration_->package());
  if (package_it == packages_.end()) {
    throw std::runtime_error("package not found");
  }
  package_ = package_it->second;

  // Set identifier.
  identifier_ = identifier(configuration_->username(),
                           configuration_->package(),
                           configuration_->server());

  // Check whether bot already exists.
  if (lua_connection::contains(identifier_)) {
    return cb(self, "bot already registered");
  }

  // Add bot to lua connection.
  lua_connection::add(shared_from_this());

  // Instantiate browser.
  browser_ = std::make_shared<bot_browser>(io_service_, self);

  // Set proxy if available.
  std::string proxy = configuration_->module_settings()["base"]["proxy"];
  command_sequence commands = configuration_->init_command_sequence();
  if (!proxy.empty()) {
    browser_->set_proxy_list(proxy,
                             [this, commands, cb, self, proxy](int success) {
      if (!success) {
        configuration_->set("base", "proxy", proxy);
        return cb(self, "no working proxy found");
      } else {
        log(BS_LOG_NFO, "base", "login: 1. try");
        std::shared_ptr<state_wrapper> s = std::make_shared<state_wrapper>();
        login_cb_ = boost::bind(&bot::handle_login, this,
                                self, s, _1, cb, commands, true, 2);
        lua_connection::login(
            s->get(), self,
            package_->modules().find("base")->second,
            &login_cb_);
      }
    });
  } else {
    log(BS_LOG_NFO, "base", "login: 1. try");
    std::shared_ptr<state_wrapper> s = std::make_shared<state_wrapper>();
    login_cb_ = boost::bind(&bot::handle_login, this,
                            self, s, _1, cb, commands, true, 2);
    lua_connection::login(s->get(), self,
                          package_->modules().find("base")->second,
                          &login_cb_);
  }
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
      auto next_state = std::make_shared<state_wrapper>();
      login_cb_ = boost::bind(&bot::handle_login, this,
                              self, next_state, _1, cb, init_commands,
                              load_mod, tries - 1);
      std::string t =  boost::lexical_cast<std::string>(4 - tries);
      log(BS_LOG_NFO, "base", std::string("login: ") + t + ". try");
      lua_connection::login(next_state->get(), shared_from_this(),
                            package_->modules().find("base")->second,
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
        auto next_state = std::make_shared<state_wrapper>();
        login_cb_ = boost::bind(&bot::handle_login, this,
                                self, next_state, _1, cb, init_commands,
                                load_mod, tries - 1);
        std::string t = boost::lexical_cast<std::string>(4 - tries);
        log(BS_LOG_NFO, "base", std::string("login: ") + t + ". try");
        lua_connection::login(
            next_state->get(), shared_from_this(),
            package_->modules().find("base")->second,
            &login_cb_);
      } else /* if (login_result_) */ {
        if (load_mod) {
          load_modules(init_commands, self);
        }
        cb(self, "");
        login_cb_ = nullptr;
        return;
      }
    }
  }
}

void bot::load_modules(const command_sequence& init_commands,
                       std::shared_ptr<bot> self) {
  // Load modules from package folder:
  const std::string& base_script = package_->modules().find("base")->second;
  for (const auto& m : package_->modules()) {
    // base and servers ain't no modules.
    if (m.first == "base" || m.first == "servers") {
      continue;
    }

    // Create module.
    try {
      auto new_module = std::make_shared<module>(m.first,
                                                 base_script, m.second,
                                                 self, io_service_);

      // Check load success.
      if (new_module->load_success()) {
        modules_.push_back(new_module);
      } else {
        log(BS_LOG_ERR, "base", m.first + " could not be loaded");
      }
    } catch (...) {
      log(BS_LOG_ERR, "base", m.first + " could not be loaded!");
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

  const auto package_it = packages_.find(package);
  if (package_it == packages_.end()) {
    std::string error = std::string("package ") + package + " not available";
    throw std::runtime_error(std::move(error));
  }

  std::string identifier = print_package + "_";
  identifier += package_it->second->tag(server);
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
  if (update_callback_ != nullptr) {
    update_callback_(identifier_, "log", msg.str());
  }
}

std::string bot::log_msgs() {
  std::stringstream log;
  for(const std::string& msg : log_msgs_) {
    log << msg;
  }
  return log.str();
}

void bot::refresh_status(const std::string& key) {
  std::string value = configuration_->value_of(key);
  if (update_callback_ != nullptr) {
    update_callback_(identifier_, key, value);
  }
}

void bot::status(const std::string& key, const std::string& value) {
  configuration_->set(key, value);
  if (update_callback_ != nullptr) {
    update_callback_(identifier_, key, value);
  }
}

void bot::execute(const std::string& command, const std::string& argument) {
  auto self = shared_from_this();
  io_service_->post([=]() {
    // Handle wait time factor command.
    if (command == "base_set_wait_time_factor") {
      std::string new_wait_time_factor = argument;
      if (new_wait_time_factor.find(".") == std::string::npos) {
        new_wait_time_factor += ".0";
      }

      try {
        float new_wtf = boost::lexical_cast<float>(new_wait_time_factor);
        if (wait_time_factor_ <= 0) {
          log(BS_LOG_ERR, "base",
              std::string("invalid value for wait time factor"));
          return;
        }
        wait_time_factor_ = new_wtf;
        std::stringstream ss;
        ss << std::setprecision(3) << new_wait_time_factor;
        std::string wtf = ss.str();
        status("base_wait_time_factor", ss.str());
        log(BS_LOG_NFO, "base", std::string("set wait time factor to ") + wtf);
        return;
      } catch(const boost::bad_lexical_cast& e) {
        log(BS_LOG_ERR, "base", std::string("could not read wait time factor"));
        status("base_wait_time_factor",
            boost::lexical_cast<std::string>(wait_time_factor_));
        return;
      }
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
          refresh_status("base_proxy");
        } else {
          log(BS_LOG_NFO, "base", "login: 1. try");
          command_sequence commands;
          auto cb = [this](std::shared_ptr<bot>, std::string err) {
            if (!err.empty()) log(BS_LOG_ERR, "base", err);
            proxy_check_active_ = false;
            refresh_status("base_proxy");
          };
          auto state = std::make_shared<state_wrapper>();
          login_cb_ = boost::bind(&bot::handle_login, this,
                                  self, state, _1, cb, commands, false, 2);
          lua_connection::login(
              state->get(), self,
              package_->modules().find("base")->second,
              &login_cb_);
        }
      });
    }

    // Forward all other commands to modules.
    for (auto module : modules_) {
      module->execute(command, argument);
    }
  });
}

}  // namespace botscript
