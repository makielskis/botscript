// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>
#include <map>

namespace botscript {

class config {
 public:
  typedef std::map<std::string, std::string> string_map;

  config(const std::string& json_config);

  config(const std::string& username,
         const std::string& password,
         const std::string& package,
         const std::string& server,
         const std::map<std::string, string_map>& module_settings);

  std::string to_json(bool with_password);
  const std::string& username();
  const std::string& password();
  const std::string& package();
  const std::string& server();
  const std::map<std::string, string_map>& module_settings();

 private:
  std::string username_, password_, package_, server_;
  std::map<std::string, string_map> module_settings_;
};

}  // namespace botscript

#endif  // CONFIG_H_
