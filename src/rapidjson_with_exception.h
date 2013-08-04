// Copyright (c) 2013, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef BOTSCRIPT_SERVER_MESSAGES_RAPIDJSON_WITH_EXCEPTION_H_
#define BOTSCRIPT_SERVER_MESSAGES_RAPIDJSON_WITH_EXCEPTION_H_

#include <stdexcept>

namespace botscript_server {

class rapidjson_exception : public std::runtime_error {
 public:
  rapidjson_exception() : std::runtime_error("json schema invalid") {
  }
};

}  // namespace botscript_server

#define RAPIDJSON_ASSERT(x) {\
  if(x) { \
  } else { throw botscript_server::rapidjson_exception(); } \
}

#include <rapidjson/document.h>

#endif  // BOTSCRIPT_SERVER_MESSAGES_RAPIDJSON_WITH_EXCEPTION_H_
