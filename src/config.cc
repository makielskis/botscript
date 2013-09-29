// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./config.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "./rapidjson_with_exception.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "./bot.h"

namespace json = rapidjson;
using namespace std;

namespace botscript {

config::config() {
}

config::config(const string& json_config) {
  // Read JSON.
  json::Document document;
  if (document.Parse<0>(json_config.c_str()).HasParseError()) {
    throw runtime_error("invalid JSON");
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
    throw runtime_error("invalid configuration");
  }

  // Read basic configuration values.
  username_ = document["username"].GetString();
  password_ = document["password"].GetString();
  package_ = document["package"].GetString();
  server_ = document["server"].GetString();
  identifier_ = bot::identifier(username_, package_, server_);

  // Read inactive flag.
  if (document.HasMember("inactive")) {
    if (!document["inactive"].IsBool()) {
      throw runtime_error("invalid configuration: inactive flag must be bool");
    }
    inactive_ = document["inactive"].GetBool();
  } else {
    inactive_ = false;
  }

  // Read and set wait time factor.
  string wtf = document["modules"]["base"]["wait_time_factor"].GetString();
  wtf = wtf.empty() ? "1.00" : wtf;
  module_settings_["base"]["wait_time_factor"] = wtf;

  // Read and set proxy.
  string proxy = document["modules"]["base"]["proxy"].GetString();
  module_settings_["base"]["proxy"] = proxy;

  // Read module settings
  const json::Value& modules = document["modules"];
  json::Value::ConstMemberIterator i = modules.MemberBegin();
  for (; i != modules.MemberEnd(); ++i) {
    const json::Value& m = i->value;

    // Ignore modules that are no objects.
    if (!m.IsObject()) {
      throw runtime_error("module not an object");
    }

    // Extract module name.
    string module_name = i->name.GetString();

    // Base module had been handled previously.
    if (module_name == "base") {
      continue;
    }

    // Iterate module settings.
    json::Value::ConstMemberIterator it = m.MemberBegin();
    for (; it != m.MemberEnd(); ++it) {
      // Read property name.
      string key = it->name.GetString();

      // Ignore "name" because the module name is not relevant for the state.
      if (key == "name") {
        continue;
      }

      // Set module status variable.
      string value = it->value.GetString();
      module_settings_[module_name][key] = value;
    }
  }
}

config::config(const string& identifier,
               const string& username,
               const string& password,
               const string& package,
               const string& server,
               const map<string, string_map>& module_settings)
  : inactive_(false),
    identifier_(identifier),
    username_(username),
    password_(password),
    package_(package),
    server_(server),
    module_settings_(module_settings) {
}

config::command_sequence config::init_command_sequence() const {
  command_sequence commands;

  // Base settings first.
  const string& wtf = module_settings_.at("base").at("wait_time_factor");
  auto wtf_init = make_pair("base_set_wait_time_factor", wtf);
  commands.emplace_back(move(wtf_init));

  // Iterate modules.
  for (const auto& module : module_settings_) {
    const string& module_name = module.first;

    // Don't handle base twice.
    if (module_name == "base") {
      continue;
    }

    // Iterate module configuration values
    for (const auto& module_config : module.second) {
      // Module activation has to be done after the configuration.
      if (module_config.first == "active") {
        continue;
      }

      string command = module_name + "_set_" + module_config.first;
      commands.emplace_back(make_pair(command, module_config.second));
    }

    // Last step: module activation.
    const auto it = module.second.find("active");
    if (it != module.second.end()) {
      commands.emplace_back(make_pair(module_name + "_set_active", it->second));
    } else {
      commands.emplace_back(make_pair(module_name + "_set_active", "0"));
    }
  }

  return commands;
}

string config::to_json(bool with_password) const {
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
  document.AddMember("inactive", inactive_, allocator);

  // Write module configuration values.
  rapidjson::Value modules(rapidjson::kObjectType);
  for(const auto& module : module_settings_) {
    // Initialize module JSON object and add name property.
    rapidjson::Value m(rapidjson::kObjectType);

    // Add module settings.
    for(const auto& module_config_val : module.second) {
      rapidjson::Value key_attr(module_config_val.first.c_str(), allocator);
      rapidjson::Value val_attr(module_config_val.second.c_str(), allocator);
      m.AddMember(key_attr, val_attr, allocator);
    }
    rapidjson::Value name_attr(module.first.c_str(), allocator);
    modules.AddMember(name_attr, m, allocator);
  }

  // Add modules to configuration.
  document.AddMember("modules", modules, allocator);

  // Convert to string.
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);

  return buffer.GetString();
}

string config::value_of(const string& key) const {
  auto pos = key.find("_");
  if (pos != string::npos) {
    string module = key.substr(0, pos);
    string setting = key.substr(pos + 1);

    const auto it1 = module_settings_.find(module);
    if (it1 != module_settings_.cend()) {
      const auto it2 = it1->second.find(setting);
      if (it2 != it1->second.end()) {
        return it2->second;
      }
    }
  }

  return "";
}

bool config::inactive() const {
  return inactive_;
}

void config::inactive(bool flag) {
  inactive_ = flag;
}

const string& config::identifier() const {
  return identifier_;
}

const string& config::username() const {
  return username_;
}

const string& config::password() const {
  return password_;
}

const string& config::package() const {
  return package_;
}

const string& config::server() const {
  return server_;
}

map<string, config::string_map> config::module_settings() const {
  return module_settings_;
}


void config::set(const string& module, const string& key, const string& value) {
  module_settings_[module][key] = value;
}

void config::set(const string& key, const string& value) {
  auto pos = key.find("_");
  if (pos != string::npos) {
    string module = key.substr(0, pos);
    string setting = key.substr(pos + 1);
    module_settings_[module][setting] = value;
  }
}

}  // namespace botscript
