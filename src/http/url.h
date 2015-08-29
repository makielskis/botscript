// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_URL_H_
#define HTTP_URL_H_

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
  explicit url(const std::string& url);

  /// Returns the input URL:
  ///
  ///  * http://host:port/path if the first constructor was used
  ///  * the url constructor parameter of the second constructor was used
  ///
  /// \return the input url
  std::string str()  const { return str_; }

  /// \return the host address
  std::string host() const { return host_; }

  /// \return the port
  std::string port() const { return port_; }

  /// \return the path
  std::string path() const { return path_; }

 private:
  /// Regular expression that matches URLs.
  static boost::regex url_regex_;

  /// The original URL.
  std::string str_;

  /// The URL host.
  std::string host_;

  /// The URL port.
  std::string port_;

  /// The URL path.
  std::string path_;
};

}  // namespace http

#endif  // HTTP_URL_H_
