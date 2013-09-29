// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef CONFIG_H_
#define CONFIG_H_

#include <vector>
#include <string>
#include <map>
#include <utility>

namespace botscript {

class config {
 public:
  typedef std::map<std::string, std::string> string_map;
  typedef std::vector<std::pair<std::string, std::string>> command_sequence;

  config();

  config(const std::string& json_config);

  config(const std::string& identifier,
         const std::string& username,
         const std::string& password,
         const std::string& package,
         const std::string& server,
         const std::map<std::string, string_map>& module_settings);

  command_sequence init_command_sequence() const;
  std::string to_json(bool with_password) const;
  std::string value_of(const std::string& key) const;

  void inactive(bool flag);
  bool inactive() const;

  const std::string& identifier() const;
  const std::string& username() const;
  const std::string& password() const;
  const std::string& package() const;
  const std::string& server() const;
  std::map<std::string, string_map> module_settings() const;

  void set(const std::string& module,
           const std::string& key,
           const std::string& value);
  void set(const std::string& key,
           const std::string& value);

 private:
  bool inactive_;
  std::string identifier_, username_, password_, package_, server_;
  std::map<std::string, string_map> module_settings_;
};

}  // namespace botscript

#endif  // CONFIG_H_
