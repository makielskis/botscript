// Copyright (c) 2013, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef CONFIG_H_
#define CONFIG_H_

#include <vector>
#include <string>
#include <map>
#include <utility>

namespace botscript {

typedef std::map<std::string, std::string> string_map;
typedef std::vector<std::pair<std::string, std::string>> command_sequence;

class bot_config {
 public:
  bot_config() {
  }
  bot_config(const bot_config&) = delete;
  bot_config& operator=(const bot_config&) = delete;

  virtual ~bot_config() {
  }

  bool valid();

  virtual command_sequence init_command_sequence() const = 0;
  virtual std::string to_json(bool with_password) const = 0;
  virtual std::string value_of(const std::string& key) const = 0;

  virtual void inactive(bool flag) = 0;
  virtual bool inactive() const = 0;

  virtual std::string identifier() const = 0;
  virtual std::string username() const = 0;
  virtual std::string password() const = 0;
  virtual std::string package() const = 0;
  virtual std::string server() const = 0;
  virtual std::map<std::string, string_map> module_settings() const = 0;

  virtual void set(const std::string& module,
                   const std::string& key,
                   const std::string& value) = 0;
  virtual void set(const std::string& key,
                   const std::string& value) = 0;
};

}  // namespace botscript

#endif  // CONFIG_H_
