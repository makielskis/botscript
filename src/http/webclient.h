// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_WEBCLIENT_H_
#define HTTP_WEBCLIENT_H_

#include <functional>
#include <string>
#include <map>

#include "boost/asio/io_service.hpp"
#include "boost/system/error_code.hpp"
#include "boost/thread.hpp"

#include "pugixml.hpp"

#include "./error.h"
#include "./http_con.h"

#define MAX_REDIRECT 3

namespace http {

/// The webclient class provides easy access to HTTP requests. It manages
/// cookies, the proxy configuration, and is able to submit HTML forms.
class webclient {
 public:
  /// Callback function that will be called on request finish. If the error_code
  /// is "Success", the string will contain the response.
  typedef std::function<void (std::string,                // the response string
                              boost::system::error_code)  // the error code
                       > callback;

  /// \param io_service points to the Asio io_service object to use for requests
  webclient(boost::asio::io_service* io_service);

  // Proxy settings
  void set_proxy(std::string host, std::string port);
  std::string proxy_host() const { return proxy_host_; }
  std::string proxy_port() const { return proxy_port_; }

  /// Submits the form that is specified by the given XPath. 
  ///
  /// \param xpath The XPath can either point to the form itself or to an submit
  ///              input contained in the form (preferred).
  /// \param page the page that contains the form specified by the xpath param.
  /// \param input_params the input values to set when submitting
  /// \param action the action attribute of the form to use (empty string to use
  ///               the default form action
  /// \param cb the callback to call on request finish
  virtual void submit(const std::string& xpath, const std::string& page,
                      std::map<std::string, std::string> input_params,
                      const std::string& action,
                      callback cb, boost::posix_time::time_duration timeout);

  /// Does a asynchronous HTTP request.
  ///
  /// \param u the URL to request
  /// \param method the method to use (util::GET or util::POST)
  /// \param body the request content to send (if request type is util::POST)
  /// \param cb the callback to call on request finish
  /// \param remaining_redirects the maximum number of redirects
  virtual void request(const url& u, int method, std::string body, callback cb,
                       int remaining_redirects,
                       boost::posix_time::time_duration timeout);

 protected:
  /// Function that will be called on request finish. Calls the user callback
  /// function provided when calling request / submit.
  ///
  /// \param host the originally requested host
  ///             (to be able to handle relative redirects)
  /// \param remaining_redirects the number of remaining redirects
  /// \param cb the callback to call on request finish
  /// \param con_ptr points to the connection used to do the request
  /// \param response the response
  /// \param ec the error code
  void request_finish(const url& request_url,
                      boost::posix_time::time_duration timeout,
                      int remaining_redirects, callback cb,
                      std::shared_ptr<http_con> con_ptr,
                      std::string response,
                      boost::system::error_code ec);

  /// Stores the cookies to the cookies_ and the headers_ map.
  ///
  /// \param new_cookies the Set-Cookies header provided by the server
  void store_cookies(const std::string& new_cookies);

  /// Error category to express HTTP errors.
  static error::http_category cat_;

  // Proxy settings
  boost::mutex proxy_mutex_;
  std::string proxy_host_;
  std::string proxy_port_;

  /// The headers to send to the server performing HTTP requests.
  std::map<std::string, std::string> headers_;

  /// Cookies.
  boost::mutex cookies_mutex_;
  std::map<std::string, std::string> cookies_;

  /// Points to the Asio io_service object to use for requests
  boost::asio::io_service* io_service_;
};

}  // namespace http

#endif  // HTTP_WEBCLIENT_H_
