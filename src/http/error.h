// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_ERROR_H_
#define HTTP_ERROR_H_

#include <string>

#include "boost/system/error_code.hpp"

#include "./noexcept.h"

namespace http {
namespace error {

enum {
  INVALID_XPATH = 201,
  NO_FORM_OR_SUBMIT = 202,
  SUBMIT_NOT_IN_FORM = 203,
  PARAM_MISMATCH = 204,
  GZIP_FAILURE = 205
};

class http_category : public boost::system::error_category {
 public:
  const char* name() const HTTP_NOEXCEPT { return "http"; }
  std::string message(int ev) const {
    switch (ev) {
      case INVALID_XPATH:      return "invalid element specified";
      case NO_FORM_OR_SUBMIT:  return "element not a form or submit";
      case SUBMIT_NOT_IN_FORM: return "submit element not in a form";
      case PARAM_MISMATCH:     return "parameters don't match";
      case GZIP_FAILURE:       return "gzip error";
    }
    return "unkown error";
  }
};

}  // namespace error
}  // namespace http

#endif  // HTTP_ERROR_H_
