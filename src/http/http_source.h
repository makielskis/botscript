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

class http_source : coroutine,
                    public std::enable_shared_from_this<http_source> {
 public:
  typedef char char_type;
  typedef boost::iostreams::source_tag category;

  typedef std::function<void (std::shared_ptr<http_source>,
                              boost::system::error_code)
                       > http_callback;

  http_source(boost::asio::io_service* io_service,
              boost::asio::ip::tcp::resolver::iterator server,
              std::string request);

  void operator()(http_callback cb);

  std::streamsize read(char_type* s, std::streamsize n);
  const std::vector<char>& response() const { return response_; }
  std::string header(const std::string& h) { return header_[h]; }

 private:
  void transfer(boost::system::error_code ec,
                std::function<void(boost::system::error_code)> cb);
  void read_header();
  std::size_t copy_content(std::size_t buffer_size);
  void read_content_length() throw(std::bad_cast);

  static boost::regex chunk_size_rx_;
  boost::asio::ip::tcp::socket socket_;
  std::string request_;
  boost::asio::ip::tcp::resolver::iterator server_;
  boost::asio::streambuf buf_;
  std::vector<char> response_;
  std::istream response_stream_;

  int status_code_;
  std::map<std::string, std::string> header_;
  int length_;
};

}  // namespace http

#endif  // HTTP_SOURCE_H_
