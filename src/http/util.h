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

typedef std::vector<std::pair<std::string, std::string>> parameters;

/// Builds a HTTP request using the provided information.
///
/// \param u          URL to request
/// \param method     HTTP method to use (util::GET or util::POST)
/// \param content    request content to send (request body)
/// \param head       HTTP headers to send
/// \param use_proxy  flag indicating whether this is a request to a proxy
std::string build_request(const url& u, const int method,
                          const std::string& content,
                          const std::map<std::string, std::string>& head,
                          bool use_proxy);

/// Stores the given URL as location meta tag in the given page.
///
/// \param page  page to store the location in
/// \param url   URL to store
/// \return the page with the stored URL
std::string store_location(std::string page, const std::string& url);

/// \param doc the parsed XML document with a stored location
/// \return the location stored by util::store_location
std::string location(const pugi::xml_document& doc);

/// \param page the page to store the location meta information in it
/// \return the page containing the stored location meta information
std::string location(const std::string& page);

/// \param page the page to store the location meta information in it
/// \return the base URL of the stored location
std::string base_url(const std::string& page);

/// \param page the page to tidy
/// \return the tidied page
std::string tidy(std::string page);

/// \param s the string to URL encode
/// \return the URL encoded string
std::string url_encode(const std::string &s);

/// \param node the node to extract the submit information from
/// \return the extracted parameters
parameters extract_parameters(const pugi::xml_node& node,
                              const pugi::xml_node& submit, bool found);

}  // namespace util

}  // namespace http

#endif  // HTTP_UTIL_H_
