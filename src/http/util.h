// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_UTIL_H_
#define HTTP_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "pugixml.hpp"

#include "./url.h"

namespace http {

namespace util {

enum { GET, POST };

using parameters = std::vector<std::pair<std::string, std::string>>;

std::string build_request(const url& u, const int method,
                          const std::string& content,
                          const std::map<std::string, std::string>& head,
                          bool use_proxy);
std::string store_location(std::string page, const std::string& url);
std::string location(const pugi::xml_document& doc);
std::string location(const std::string& page);
std::string base_url(const std::string& page);
std::string tidy(std::string page);
std::string url_encode(const std::string &s);
parameters extract_parameters(const pugi::xml_node& node,
                              const pugi::xml_node& submit, bool found);

}  // namespace util

}  // namespace http

#endif  // HTTP_UTIL_H_
