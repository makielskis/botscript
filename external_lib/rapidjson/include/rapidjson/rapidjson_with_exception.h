// Copyright (c) 2013, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef RAPIDJSON_WITH_EXCEPTION_H_
#define RAPIDJSON_WITH_EXCEPTION_H_

#include <stdexcept>

namespace botscript_server {

class rapidjson_exception : public std::runtime_error {
 public:
  rapidjson_exception() : std::runtime_error("json schema invalid") {
  }
};

}  // namespace botscript_server

#ifdef RAPIDJSON_ASSERT
#undef RAPIDJSON_ASSERT
#endif

#define RAPIDJSON_ASSERT(x) {\
  if(x) { \
  } else { throw botscript_server::rapidjson_exception(); } \
}

#include <rapidjson/document.h>

#endif  // RAPIDJSON_WITH_EXCEPTION_H_
