// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef HTTP_SOURCE_H_
#define HTTP_SOURCE_H_

#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

#include "boost/asio.hpp"
#include "boost/iostreams/stream.hpp"

#include "./coroutine/coroutine.hpp"

namespace http {

/// The http_source class is responsible for sending and receiving HTTP traffic.
/// Furthermore, it enables to be used as a stream (using Boost Iostreams).
///
/// To keep a reference to itself doing async. work, http_source is derived from
/// std::enable_shared_from_this<http_source>.
///
/// http_source is using the concept of stackless coroutines to "chain"
/// asynchronous operations.
class http_source : coroutine,
                    public std::enable_shared_from_this<http_source> {
 public:
  typedef char char_type;
  typedef boost::iostreams::source_tag category;

  /// Callback function to be called on request finish.
  typedef std::function<void (std::shared_ptr<http_source>, /// self
                              boost::system::error_code)    /// the error code
                       > callback;

  /// \param socket the socket to use for requests.
  ///        This needs to be already connected to the remote host.
  http_source(boost::asio::ip::tcp::socket* socket);

  /// Starts the asynchronous operation.
  ///
  /// \param request  request to send
  /// \param cb       callback to call on finish
  void operator()(std::string request, callback cb);

  /// \sa Boost Iostreams
  std::streamsize read(char_type* s, std::streamsize n);

  /// \return a reference to the response
  const std::vector<char>& response() const { return response_; }

  /// \return the header with the specified key
  std::string header(const std::string& h) const {
    return (header_.find(h) != header_.end()) ?  header_.at(h) : "";
  }

 private:
  /// Reentrant transfer function.
  ///
  /// \param ec  error code produced by the last operation
  /// \param cb  callback to call on finish
  void transfer(boost::system::error_code ec,
                std::function<void(boost::system::error_code)> cb);

  /// Reads the headers from the response_buffer_.
  void read_header();

  /// Reads content from the buf_ to the response_ vector.
  std::size_t copy_content(std::size_t buffer_size);

  /// Parsed the content length header.
  void read_content_length() throw(std::bad_cast);

  /// Regular expression to detect a chunk size declaration.
  static boost::regex chunk_size_rx_;

  /// Points to the socket to use for the communication.
  boost::asio::ip::tcp::socket* socket_;

  /// Request buffer that will be sent.
  std::string request_;

  /// The response buffer.
  boost::asio::streambuf buf_;

  /// The final response.
  std::vector<char> response_;

  /// Response stream to read from buf_.
  std::istream response_stream_;

  /// The HTTP response status code.
  int status_code_;

  /// The HTTP response Content-Length.
  std::size_t length_;

  /// The HTTP response header.
  std::map<std::string, std::string> header_;
};

}  // namespace http

#endif  // HTTP_SOURCE_H_
