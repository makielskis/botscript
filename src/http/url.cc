// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./url.h"

#include <utility>
#include <iostream>

namespace http {

boost::regex url::url_regex_("(.*)://([a-zA-Z0-9\\.\\-]*)(:[0-9]*)?(.*)");

url::url(std::string host, std::string port, std::string path)
  : str_(std::string("http://") + host + ":" + port + path),
    host_(std::move(host)),
    port_(std::move(port)),
    path_(std::move(path)) {
}

url::url(const std::string& url) throw(std::invalid_argument)
    : str_(url) {
  // Extract protocol, port, host address and path from the URL.
  boost::match_results<std::string::const_iterator> what;
  bool matches = boost::regex_search(url, what, url_regex_);

  // Throw invalid_argument exception if the regular expression didn't match.
  if (!matches) {
    throw std::invalid_argument(url + " is not a valid URL");
  }

  // Extract match.
  host_ = what[2].str();
  port_ = what[3].str();
  path_ = what[4].str();

  // Set port and path to default values if not set explicitly.
  // Also cut leading ':' from port if set explicitly.
  std::string prot = what[1].str();
  port_ = port_.empty() ? prot : port_.substr(1, port_.length() - 1);
  path_ = path_.empty() ? "/" : path_;
}

}  // namespace http
