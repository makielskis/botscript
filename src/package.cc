// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./package.h"

#if defined _WIN32 || defined _WIN64
#else  // defined _WIN32 || defined _WIN64
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#endif  // defined _WIN32 || defined _WIN64

#include <sstream>
#include <fstream>
#include <stdexcept>
#include "boost/filesystem.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_stream.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/device/back_inserter.hpp"

#include "./lua/lua_connection.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace json = rapidjson;

namespace botscript {

typedef std::map<std::string, std::string> mod_map;

package::package(std::string name, std::map<std::string, std::string> modules)
    : name_(std::move(name)),
      modules_(unzip(std::move(modules))),
      servers_(lua_connection::server_list(modules_["servers"])),
      interface_(json_description(modules_, servers_, name_)) {
}

package::package(const std::string& path)
    : name_(name_from_path(path)),
      modules_(unzip_if_no_directory(read_modules(path), path)),
      servers_(lua_connection::server_list(modules_["servers"])),
      interface_(json_description(modules_, servers_, name_)) {
}

const std::string& package::name() const {
  return name_;
}

const std::string& package::tag(const std::string& url) const {
  auto i = servers_.find(url);
  if (i == servers_.end()) {
    return url;
  } else {
    return i->second;
  }
}

const std::map<std::string, std::string>& package::modules() const {
  return modules_;
}

const std::string& package::interface_desc() const {
  return interface_;
}

std::string package::name_from_path(const std::string& path) {
  // Discover package name.
  std::string stripped_path = path;
  std::size_t dot_pos = path.rfind(".package");
  if (dot_pos != std::string::npos && dot_pos != 0) {
    stripped_path = path.substr(0, dot_pos);
  }

  // Strip until last slash.
  std::size_t slash_pos = stripped_path.find_last_of("/");
  if (slash_pos != std::string::npos) {
    stripped_path = stripped_path.substr(slash_pos + 1);
  }

  return stripped_path;
}

std::map<std::string, std::string> package::read_modules(const std::string& path) {
  // Load modules (either from lib or from file).
  std::map<std::string, std::string> modules;
  bool dir = boost::filesystem::is_directory(path);
  if (dir) {
    modules = package::from_folder(path);
  } else {
    modules = package::from_lib(path);
  }

  // Check whether base and servers "modules" are contained.
  bool no_base = modules.find("base") == modules.end();
  bool no_servers = modules.find("servers") == modules.end();
  if (no_base || no_servers) {
    throw std::runtime_error(path + "  doesn't contain base/servers");
  }

  return modules;
}

std::map<std::string, std::string> package::unzip_if_no_directory(
    std::map<std::string, std::string> modules,
    const std::string& path) {
  if (!boost::filesystem::is_directory(path)) {
    return unzip(modules);
  }
  return modules;
}

std::map<std::string, std::string> package::unzip(
    std::map<std::string, std::string> modules) {
  for (auto& module : modules) {
    std::vector<char> zip(module.second.begin(), module.second.end());
    std::vector<char> unzipped = unzip(zip);
    module.second = std::string(unzipped.begin(), unzipped.end());
  }
  return modules;
}

std::string package::json_description(
    const std::map<std::string, std::string>& modules,
    const std::map<std::string, std::string>& servers,
    const std::string& package_name) {
  // Initialize JSON document object.
  rapidjson::Document a;
  rapidjson::Document::AllocatorType& alloc = a.GetAllocator();
  a.SetObject();

  // Write package name.
  rapidjson::Value name(package_name.c_str(), alloc);
  a.AddMember("name", name, alloc);

  // Write servers from package:
  // Lua table -> map -> JSON array
  rapidjson::Value l(rapidjson::kArrayType);
  for (const auto& server : servers) {
    rapidjson::Value server_name(server.first.c_str(), alloc);
    l.PushBack(server_name, alloc);
  }
  a.AddMember("servers", l, alloc);

  // Write pseudo module "base" interface description.
  rapidjson::Value base(rapidjson::kObjectType);

  // Add bot setting wait time factor as base module setting.
  rapidjson::Value wtf_input(rapidjson::kObjectType);
  wtf_input.AddMember("input_type", "slider", alloc);
  wtf_input.AddMember("display_name", "Wartezeiten Faktor", alloc);
  wtf_input.AddMember("value_range", "0.2,3.0", alloc);
  base.AddMember("wait_time_factor", wtf_input, alloc);

  // Add bot setting proxy as base module setting.
  rapidjson::Value proxy_input(rapidjson::kObjectType);
  proxy_input.AddMember("input_type", "textarea", alloc);
  proxy_input.AddMember("display_name", "Proxy", alloc);
  base.AddMember("proxy", proxy_input, alloc);

  // Set base module name.
  base.AddMember("module", "Basis Konfiguration", alloc);

  // Add base module to modules.
  a.AddMember("base", base, alloc);

  // Write interface descriptions from all modules.
  for (const auto& module : modules) {
    // Write interface description for real modules
    // (not server listing or base module containing the login function).
    if (module.first != "servers" && module.first != "base") {
      // Write value to package information.
      auto iface = lua_connection::iface(module.second, module.first, &alloc);
      rapidjson::Value module_name(module.first.c_str(), alloc);
      a.AddMember(module_name, *iface.get(), alloc);
    }
  }

  // Convert to string.
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  a.Accept(writer);

  return buffer.GetString();
}

std::map<std::string, std::string> package::from_folder(const std::string& p) {
  std::map<std::string, std::string> modules;

  // Return if the specified path is not a directory.
  if (!boost::filesystem::is_directory(p)) {
    return modules;
  }

  // Iterate over files contained in the directory.
  using boost::filesystem::directory_iterator;
  for (auto i = directory_iterator(p); i != directory_iterator(); ++i) {
    // Get file path and filename.
    std::string path = i->path().relative_path().generic_string();
    std::string name = i->path().filename().string();

    // Don't read hidden files.
    if (boost::starts_with(name, ".")) {
      continue;
    }

    // Remove file ending (i.e. ".lua")
    std::size_t dot_pos = name.find(".");
    if (dot_pos != std::string::npos) {
      name = name.substr(0, dot_pos);
    }

    // Read file content.
    std::ifstream file;
    file.open(path.c_str(), std::ios::in);
    std::stringstream content;
    boost::iostreams::copy(file, content);
    file.close();

    // Store file content.
    modules[name] = content.str();
  }

  return modules;
}

#if defined _WIN32 || defined _WIN64
std::map<std::string, std::string> package::from_lib(
    const std::string& p) {
  // Define module map and get_modules to simplify code.
  typedef void* (__cdecl *get_modules)(void);

  // Discover package name.
  std::string name = boost::filesystem::path(p).filename().generic_string();
  std::size_t dot_pos = name.find(".");
  if (dot_pos != std::string::npos) {
    name = name.substr(0, dot_pos);
  }

  // Load library.
  HINSTANCE lib = LoadLibrary(p.c_str());
  if (nullptr == lib) {
    return std::map<std::string, std::string>();
  }

  // Get address to load function.
  std::string fun_name = (std::string("load_") + name);
  get_modules lib_fun = (get_modules) GetProcAddress(lib, fun_name.c_str());
  if (nullptr == lib_fun) {
    FreeLibrary(lib);
    return std::map<std::string, std::string>();
  }

  // Call load function.
  mod_map* m = static_cast<mod_map*>((*lib_fun)());
  std::unique_ptr<mod_map> ptr(static_cast<mod_map*>(m));
  FreeLibrary(lib);

  return *m;
}
#else  // defined _WIN32 || defined _WIN64
  std::map<std::string, std::string> package::from_lib(const std::string& p) {
  // Discover package name.
  using boost::filesystem::path;
  std::string name = path(p).filename().generic_string();
  std::size_t dot_pos = name.find(".");
  if (dot_pos != std::string::npos) {
    name = name.substr(0, dot_pos);
  }

  // Load library.
  void* lib = dlopen(p.c_str(), RTLD_LAZY);
  if (nullptr == lib) {
    return std::map<std::string, std::string>();
  }

  // Get pointer to the load function.
  void* (*lib_fun)(void);
  *(void **) (&lib_fun) = dlsym(lib, (std::string("load_") + name).c_str());
  if (nullptr == lib_fun) {
    return std::map<std::string, std::string>();
  }

  // Read modules.
  mod_map* m = static_cast<mod_map*>((*lib_fun)());
  std::unique_ptr<mod_map> ptr(static_cast<mod_map*>(m));

  // Close shared library.
  dlclose (lib);

  return *m;
}
#endif  // defined _WIN32 || defined _WIN64

std::vector<char> package::unzip(const std::vector<char>& data) {
  std::stringstream s, response;
  s << std::string(data.begin(), data.end());
  boost::iostreams::filtering_streambuf<boost::iostreams::input> filter;
  filter.push(boost::iostreams::gzip_decompressor());
  filter.push(s);
  boost::iostreams::copy(filter, response);
  std::string res_str = response.str();
  return std::vector<char>(res_str.begin(), res_str.end());
}

}  // namespace botscript
