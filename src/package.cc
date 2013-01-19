// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./package.h"

#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_stream.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/device/back_inserter.hpp"

#include "./lua/lua_connection.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace json = rapidjson;

namespace botscript {

package::package(const std::string& package_name,
                 std::map<std::string, std::string> modules, bool zipped)
    : modules_(std::move(modules)) {
  // Unzip.
  if (zipped) {
    for (auto& module : modules_) {
      std::vector<char> zip(module.second.begin(), module.second.end());
      std::vector<char> unzipped = unzip(zip);
      module.second = std::string(unzipped.begin(), unzipped.end());
    }
  }

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
  bool success = lua_connection::server_list(modules_["servers"], &servers_);
  for(const auto& server : servers_) {
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
  for (const auto& module : modules_) {
    // Write interface description for real modules
    // (not server listing or base module containing the login function).
    if (module.first != "servers" && module.first != "base") {
      // Write value to package information.
      jsonval_ptr iface = lua_connection::iface(module.second,
                                                module.first, &alloc);
      rapidjson::Value module_name(module.first.c_str(), alloc);
      a.AddMember(module_name, *iface.get(), alloc);
    }
  }

  // Convert to string.
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  a.Accept(writer);

  interface_ = buffer.GetString();
}

std::vector<char> package::unzip(const std::vector<char>& data) {
  namespace io = boost::iostreams;
  std::vector<char> decompressed;
  io::filtering_ostream os;
  os.push(io::gzip_decompressor());
  os.push(io::back_inserter(decompressed));
  io::write(os, &data[0], data.size());
  return decompressed;
}

}  // namespace botscript
