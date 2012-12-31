// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef URL_H_
#define URL_H_

#include <string>
#include <stdexcept>

#include "boost/regex.hpp"

namespace http {

/// This class represents an URL.
/// It is used to model a URL as well as to split a URL string
/// into the different parts (host, port, path).
class url {
 public:
  /// Takes the three parts of a URL: host, port and path.
  ///
  /// \param host the host
  /// \param port the port
  /// \param path the path
  url(std::string host, std::string port, std::string path);

  /// Takes the URL string and splits it into three parts: host, port and path.
  ///
  /// \param url the url to split
  explicit url(const std::string& url) throw(std::invalid_argument);

  /// \return the host address
  std::string host() const { return host_; }

  /// \return the port
  std::string port() const { return port_; }

  /// \return the path
  std::string path() const { return path_; }

 private:
  /// Regular expression that matches URLs.
  static boost::regex url_regex_;

  /// The URL host.
  std::string host_;

  /// The URL port.
  std::string port_;

  /// The URL path.
  std::string path_;
};

}  // namespace http

#endif  // URL_H_
