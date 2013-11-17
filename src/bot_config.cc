// Copyright (c) 2013, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./bot_config.h"

namespace botscript {

bool bot_config::valid() {
  if (username().empty() || password().empty() ||
      package().empty() || server().empty()) {
    return false;
  }

  auto modules = module_settings();
  auto base = modules.find("base");
  return base != modules.end() &&
         base->second.find("wait_time_factor") != base->second.end() &&
         base->second.find("proxy") != base->second.end();
}

}  // namespace botscript
