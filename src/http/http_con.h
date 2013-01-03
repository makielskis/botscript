// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_CON_H_
#define HTTP_CON_H_

#include <memory>
#include <functional>

#include "boost/asio/io_service.hpp"
#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

#include "./coroutine/coroutine.hpp"
#include "./url.h"
#include "./http_source.h"

namespace http {

/// The http_con class abstracts a HTTP connection and is responsible for HTTP
/// requests. If the response is gziped it will unzip it.
///
/// To ensure that the http_con object won't be deleted after starting the
/// asynchronous request, it keeps a std::shared_ptr to itself while requesting.
/// Therefore, it is derived from std::enable_shared_from_this<http_con>.
class http_con : public std::enable_shared_from_this<http_con> {
 public:
  /// Callback function definition. If the error code is 'Success', the string
  /// will contain the response.
  typedef std::function<void (std::shared_ptr<http_con>,  // Pointer to "self"
                              std::string,                // The response
                              boost::system::error_code   // The error code
                             )> callback;

  /// Constructor. Does not establish the connection itself. The connection will
  /// be established on the first call to operator()(...).
  ///
  /// \param io_service points to the Asio io_service object to use for requests
  /// \param host the host to connect to
  /// \param port the port to connect to
  /// \param timeout the request timeout
  http_con(boost::asio::io_service* io_service,
           std::string host, std::string port,
           boost::posix_time::time_duration timeout);

  /// \return the http_source
  const http_source& http_src() const { return *src_; }

  /// Does the actual HTTP request. Sends the request_str to the host and calls
  /// the given callback on request finish.
  ///
  /// \param request_str the request to send (i.e. "GET / HTTP/1.1\r\n...")
  /// \param cb the callback to call on request finish
  void operator()(std::string request_str, callback cb);

 private:
  /// Starts the request.
  ///
  /// Calls the http_source request method directly if the socket is already
  /// connected. Otherwise, it connects the socket to host_:port_ by using
  /// a chained operation:
  ///
  /// request -> resolve -> connect -> on_connect
  ///         -> http_source() -> request_finish
  ///
  /// \param self shared pointer to self
  /// \param request_str the request string to send
  /// \param cb the callback to call on request finish
  void request(std::shared_ptr<http_con> self,
               std::string request_str, callback cb);

  /// Part of the chained operation described in \sa http_con::request
  void resolve(std::shared_ptr<http_con> self,
               std::string request_str, callback cb);

  /// Part of the chained operation described in \sa http_con::request
  void connect(std::shared_ptr<http_con> self,
               std::string request_str, callback cb,
               boost::system::error_code ec,
               boost::asio::ip::tcp::resolver::iterator iterator);

  /// Part of the chained operation described in \sa http_con::request
  void on_connect(std::shared_ptr<http_con> self,
                  std::string request_str, callback cb,
                  boost::system::error_code ec,
                  boost::asio::ip::tcp::resolver::iterator iterator);

  /// Callback called by the request timeout timer.
  /// Disconnects the socket if the timeout expired.
  void timer_callback(std::shared_ptr<http_con> self,
                      const boost::system::error_code& ec);

  /// Part of the chained operation described in \sa http_con::request
  void request_finish(std::shared_ptr<http_con> self, callback cb,
                      std::shared_ptr<http_source> http_src,
                      boost::system::error_code ec);

  /// Points to the Asio io_service object to use for requests.
  boost::asio::io_service* io_service_;

  /// Asio resolver object used to resolve tcp endpoints.
  boost::asio::ip::tcp::resolver resolver_;

  /// The request socket.
  boost::asio::ip::tcp::socket socket_;

  /// Timeout timer that will stop the request if a timeout occured.
  boost::asio::deadline_timer req_timeout_timer_;

  /// Shared pointer to the http_source that does the actual request.
  std::shared_ptr<http_source> src_;

  /// The host and port this http_con is connected to.
  std::string host_, port_;

  /// Flag indicating whether the socket is already connected to host_:port_.
  bool connected_;
};

}  // namespace http

#endif  // HTTP_H_
