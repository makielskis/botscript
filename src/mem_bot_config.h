// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef MEM_CONFIG_H_
#define MEM_CONFIG_H_

#include <vector>
#include <string>
#include <map>
#include <utility>

#include "./bot_config.h"

namespace botscript {

class mem_bot_config : public bot_config {
 public:
  mem_bot_config();
  mem_bot_config(const std::string& json_config);
  mem_bot_config(const std::string& identifier,
                 const std::string& username,
                 const std::string& password,
                 const std::string& package,
                 const std::string& server,
                 const std::map<std::string, string_map>& module_settings);

  virtual command_sequence init_command_sequence() const override;
  virtual std::string to_json(bool with_password) const override;
  virtual std::string value_of(const std::string& key) const override;

  virtual void inactive(bool flag) override;
  virtual bool inactive() const override;

  virtual std::string identifier() const override;
  virtual std::string username() const override;
  virtual std::string password() const override;
  virtual std::string package() const override;
  virtual std::string server() const override;
  virtual std::map<std::string, string_map> module_settings() const override;

  virtual std::map<std::string, std::string> cookies() const override;
  virtual void cookies(std::map<std::string, std::string> const&) override;

  virtual void set(const std::string& module,
                   const std::string& key,
                   const std::string& value) override;
  virtual void set(const std::string& key,
                   const std::string& value) override;

 private:
  bool inactive_;
  std::string identifier_, username_, password_, package_, server_;
  std::map<std::string, string_map> module_settings_;
  std::map<std::string, std::string> cookies_;
};

}  // namespace botscript

#endif  // MEM_CONFIG_H_
