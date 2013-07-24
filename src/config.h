// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef CONFIG_H_
#define CONFIG_H_

#include <vector>
#include <string>
#include <map>
#include <utility>

#ifdef BS_MULTI_THREADED
#include <boost/mutex.hpp>
#endif

namespace botscript {

class config {
 public:
  typedef std::map<std::string, std::string> string_map;
  typedef std::vector<std::pair<std::string, std::string>> command_sequence;

  config();

  config(const std::string& json_config);

  config(const std::string& username,
         const std::string& password,
         const std::string& package,
         const std::string& server,
         const std::map<std::string, string_map>& module_settings);

  command_sequence init_command_sequence() const;
  std::string to_json(bool with_password) const;
  std::string value_of(const std::string& key) const;

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
  std::string username_, password_, package_, server_;
  std::map<std::string, string_map> module_settings_;

#ifdef BS_MULTI_THREADED
  mutable boost::mutex mutex_;
#endif
};

}  // namespace botscript

#endif  // CONFIG_H_
