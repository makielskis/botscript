// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./config.h"

#include <stdexcept>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace json = rapidjson;

namespace botscript {

config::config(const std::string& json_config) {
  // Read JSON.
  json::Document document;
  if (document.Parse<0>(json_config.c_str()).HasParseError()) {
    throw std::runtime_error("invalid JSON");
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
    throw std::runtime_error("invalid configuration");
  }

  // Read basic configuration values.
  username_ = document["username"].GetString();
  password_ = document["password"].GetString();
  package_ = document["package"].GetString();
  server_ = document["server"].GetString();

  // Read and set wait time factor.
  std::string wtf = document["modules"]["base"]["wait_time_factor"].GetString();
  wtf = wtf.empty() ? "1.00" : wtf;
  module_settings_["base"]["wait_time_factor"] = wtf;

  // Read and set proxy.
  std::string proxy = document["modules"]["base"]["proxy"].GetString();
  module_settings_["base"]["proxy"] = proxy;

  // Read module settings
  const json::Value& modules = document["modules"];
  json::Value::ConstMemberIterator i = modules.MemberBegin();
  for (; i != modules.MemberEnd(); ++i) {
    const json::Value& m = i->value;

    // Ignore modules that are no objects.
    if (!m.IsObject()) {
      throw std::runtime_error("module not an object");
    }

    // Extract module name.
    std::string module_name = i->name.GetString();

    // Base module had been handled previously.
    if (module_name == "base") {
      continue;
    }

    // Iterate module settings.
    json::Value::ConstMemberIterator it = m.MemberBegin();
    for (; it != m.MemberEnd(); ++it) {
      // Read property name.
      std::string key = it->name.GetString();

      // Ignore "name" because the module name is not relevant for the state.
      if (key == "name") {
        continue;
      }

      // Set module status variable.
      std::string value = it->value.GetString();
      module_settings_[module_name][key] = value;
    }
  }
}

config::config(const std::string& username,
               const std::string& password,
               const std::string& package,
               const std::string& server,
               const std::map<std::string, string_map>& module_settings)
  : username_(username),
    password_(password),
    package_(package),
    server_(server),
    module_settings_(module_settings) {
}

std::string config::to_json(bool with_password) {
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

const std::string& config::username() {
  return username_;
}

const std::string& config::password() {
  return password_;
}

const std::string& config::package() {
  return package_;
}

const std::string& config::server() {
  return server_;
}

const std::map<std::string, config::string_map>& config::module_settings() {
  return module_settings_;
}

}  // namespace botscript
