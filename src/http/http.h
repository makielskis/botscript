/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 6. April 2012  makielskis@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HTTP_H_
#define HTTP_H_

#include <string>
#include <map>
#include <utility>
#include <algorithm>
#include <iosfwd>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "boost/iostreams/stream.hpp"
#include "boost/iostreams/categories.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/utility.hpp"
#include "boost/bind.hpp"
#include "boost/regex.hpp"
#include "boost/foreach.hpp"
#include "boost/asio.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/exception/exception.hpp"

namespace http {

/**
 * Boost.Iostreams HTTP source stream.
 * \sa request class for convenience usage
 */
class http_source {
 public:
  /// This is a char stream (to be used with Boost.Iostreams)
  typedef char char_type;

  /// This is a source stream (to be used with Boost.Iostreams)
  typedef boost::iostreams::source_tag category;

  /// Request methods enumeration.
  enum { GET, POST, /* OPTIONS, HEAD, PUT, DELETE not implemented (yet?) */ };

  /**
   * Initializes and connects the http_source to the given host (or proxy_host).
   * Does the request and transfers the response header.
   * The response content will be read using the read() functions.
   *
   * \exception std::ios_base::failure if the connection fails
   * \param host the host to request
   * \param port the port to use (use proxy port if proxy is used)
   * \param path the path to send the request to
   * \param method the method to use http::http_source::GET/POST
   * \param headers the headers to send
   * \param content the request body to send (for POST requests)
   * \param content_length the content length (for content != nullptr)
   * \param proxy_host the proxy host to use
                       (or empty string for direct connection)
   * \param io_service Asio io_service object to use for async function calls
   */
  http_source(const std::string& host, const std::string& port,
              const std::string& path, const int method,
              const std::map<std::string, std::string>& headers,
              const void* content, const size_t content_length,
              const std::string& proxy_host,
              boost::asio::io_service* io_service)
  throw(std::ios_base::failure)
    : io_service_(io_service),
      socket_(*io_service),
      resolver_(*io_service),
      timeout_timer_(*io_service),
      response_status_code_(0),
      content_encoding_gzip_(false),
      transfer_encoding_chunked_(false),
      content_length_(0),
      bytes_transferred_(0),
      requested_bytes_(0),
      transfer_all_(false),
      transfer_finished_(false) {
    buildRequest(host, path, method, content, content_length,
                 headers, !proxy_host.empty());

    boost::asio::ip::tcp::resolver::query query(
            proxy_host.empty() ? host : proxy_host, port);
    resolver_.async_resolve(query,
                            boost::bind(&http_source::handleResolve, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::iterator));
    io_service_->run();
  }

  ~http_source() {
    finishTransfer();
  }

  /// Read function suited to use with boost::iostreams::stream
  /**
   * \param s the buffer to copy to
   * \param n the buffer size
   * \exception std::ios_base::failure if the transfer fails
   * \return the count of bytes copied to s
   */
  std::streamsize read(char_type* s, std::streamsize n)
  throw(std::ios_base::failure) {
    // Check for empty response buffer.
    if (response_content_.size() == 0) {
      return 0;
    }

    if (!transfer_finished_) {
      // Fill buffer.
      requested_bytes_ = n;
      io_service_->reset();
      if (transfer_encoding_chunked_) {
        handleReadChunkSize(boost::system::error_code(), 0);
      } else {
        handleRead(boost::system::error_code(), 0);
      }
      io_service_->run();
    }

    // Copy from buffer to stream.
    size_t to_return = std::min((size_t) n, response_content_.size());
    std::memcpy(s, &(response_content_[0]), to_return);
    std::memmove(&(response_content_[0]), &(response_content_[to_return]),
                 response_content_.size() - to_return);
    bytes_transferred_ = response_content_.size() - to_return;
    response_content_.resize(bytes_transferred_);

    return to_return;
  }

  /// Reads the whole content with one timeout.
  /**
   * The whole transfer is allowed to take timeout seconds.
   * \param timeout the timeout in seconds
   * \return the bytes read in a vector
   * \exception std::ios_base::failure if the transfer fails
   */
  const std::vector<char> read(int timeout)
  throw(std::ios_base::failure) {
    // Don't transfer anything at all if the transfer is already finished.
    // Simply return the response buffer.
    if (transfer_finished_) {
      return response_content_;
    }

    // Nothing to transfer - return empty buffer.
    if (!transfer_encoding_chunked_ && !content_length_) {
      response_content_.clear();
      return response_content_;
    }

    if (!transfer_encoding_chunked_ &&
        response_buffer_.size() == content_length_) {
      // Copy bytes already transferred if this was all.
      size_t buffer_size = response_buffer_.size();
      const char* buf =
              boost::asio::buffer_cast<const char*>(response_buffer_.data());
      response_content_.resize(buffer_size);
      std::memcpy(&(response_content_[bytes_transferred_]), buf, buffer_size);
      bytes_transferred_ += buffer_size;
      response_buffer_.consume(buffer_size);
      content_length_ -= buffer_size;
      return response_content_;
    } else {
      // Do transfer.
      io_service_->reset();
      transfer_all_ = true;
      if (transfer_encoding_chunked_) {
        handleReadChunkSize(boost::system::error_code(), 0);
      } else {
        handleRead(boost::system::error_code(), 0);
      }
    }

    // Set timeout and transfer.
    startTimeout(timeout);
    io_service_->run();

    return response_content_;
  }

  /// Tells whether the content encoding was GZIP or not.
  bool content_encoding_gzip() { return content_encoding_gzip_; }

  /// Returns the cookies set by the server.
  /**
   * The expire date and path information get discarded since this is only
   * a very simple HTTP stream implementation.
   */
  std::map<std::string, std::string>& cookies() { return response_cookies_; }

  /// Returns the location set by the server to redirect the client.
  std::string& location() { return response_location_; }

 private:
  void buildRequest(const std::string host, const std::string path,
                    const int method,
                    const void* content, const size_t content_length,
                    const std::map<std::string, std::string>& headers,
                    bool use_proxy) {
    // Write the request line.
    std::ostream request_stream(&request_buffer_);
    switch (method) {
      case GET:
        request_stream << "GET";
        break;

      case POST:
        request_stream << "POST";
        break;

      default:
        assert(false);
    }
    request_stream << " " << (use_proxy ? ("http://" + host + path) : path);
    request_stream << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << "\r\n";

    // Write the headers.
    typedef std::pair<std::string, std::string> str_pair;
    BOOST_FOREACH(str_pair header, headers) {
      request_stream << header.first << ": " << header.second << "\r\n";
    }

    // Set content length if available.
    if (content != NULL && content_length != 0) {
      std::string size_str = boost::lexical_cast<std::string >(content_length);
      request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
      request_stream << "Content-Length: " + size_str + "\r\n";
    }

    // Finish headers.
    request_stream << "\r\n";

    // Write content if set.
    if (content != NULL && content_length != 0) {
      request_stream << std::string(reinterpret_cast<const char*>(content),
                                    content_length);
    }

    // Finish request with an empty line.
    request_stream << "\r\n";
  }

  void handleResolve(const boost::system::error_code& ec,
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
  throw(std::ios_base::failure) {
    if (!ec) {
      // Connect to the resolved endpoint.
      startTimeout(4);
      boost::asio::async_connect(socket_, endpoint_iterator,
                                 boost::bind(&http_source::handleConnect, this,
                                             boost::asio::placeholders::error));
    } else {
      throw std::ios_base::failure("resolve error");
    }
  }

  void handleConnect(const boost::system::error_code& ec)
  throw(std::ios_base::failure) {
    if (!ec) {
      // Start the timeout timer assuming that the minimum transfer rate
      // is at least 1kb per second
      int timeout = ceil(request_buffer_.size() / static_cast<double>(1024));
      startTimeout(timeout + 2);

      // Write request.
      boost::asio::async_write(socket_, request_buffer_,
                               boost::bind(&http_source::handleWrite, this,
                                           boost::asio::placeholders::error));
    } else {
      throw std::ios_base::failure("connect error");
    }
  }

  void handleWrite(const boost::system::error_code& ec)
  throw(std::ios_base::failure) {
    if (!ec) {
      // Read the status line (with timeout).
      startTimeout(14);
      boost::asio::async_read_until(socket_, response_buffer_, "\r\n",
              boost::bind(&http_source::handleReadStatusLine, this,
                          boost::asio::placeholders::error));
    } else {
      throw std::ios_base::failure("write error");
    }
  }

  void handleReadStatusLine(const boost::system::error_code& ec)
  throw(std::ios_base::failure) {
    if (!ec) {
      // Check the status line
      std::istream response_stream(&response_buffer_);
      std::string http_version;
      response_stream >> http_version;
      response_stream >> response_status_code_;
      response_stream.ignore(128, '\n');

      // Read response headers (with timeout).
      timeout_timer_.expires_from_now(boost::posix_time::seconds(6));
      boost::asio::async_read_until(socket_, response_buffer_, "\r\n\r\n",
              boost::bind(&http_source::handleReadHeaders, this,
                          boost::asio::placeholders::error));
    } else {
      throw std::ios_base::failure("read (statusline) error");
    }
  }

  void handleReadHeaders(const boost::system::error_code& ec)
  throw(std::ios_base::failure) {
    if (!ec) {
      std::istream response_stream(&response_buffer_);
      std::string header;

      while (std::getline(response_stream, header) && header != "\r") {
        // Transform headers tolower since some servers don't care for case.
        // For example amazon.com sends 'Set-cookie' instead of 'Set-Cookie'.
        std::string header_to_lower;
        header_to_lower.resize(header.size());
        std::transform(header.begin(), header.end(),
                       header_to_lower.begin(), ::tolower);

        // Read header
        if (boost::starts_with(header_to_lower, "location: ")) {
          response_location_ = header.substr(10, header.length() - 11);
        } else if (boost::starts_with(header_to_lower, "set-cookie: ")) {
          readCookies(header.substr(12, header.length() - 13));
        } else if (boost::starts_with(header_to_lower, "content-length: ")) {
          content_length_ = std::atoi(header_to_lower.substr(16).c_str());
        } else if (boost::starts_with(header_to_lower, "transfer-encoding: ")) {
          transfer_encoding_chunked_ = header.substr(19, 7) == "chunked";
        } else if (boost::starts_with(header_to_lower, "content-encoding: ")) {
          content_encoding_gzip_ = header.substr(18, 4) == "gzip";
        }
      }
    } else {
      throw std::ios_base::failure("read (headers) error");
    }
  }

  void handleRead(const boost::system::error_code& ec,
                  std::size_t bytes_transferred)
  throw(std::ios_base::failure) {
    if (!ec) {
      // Stop if we transferred enough bytes.
      if (!transfer_all_ && response_content_.size() >= requested_bytes_) {
        return;
      }

      // Copy transferred bytes from response buffer to stream buffer.
      if (response_buffer_.size() > 0) {
        size_t buffer_size = response_buffer_.size();
        const char* buf =
                boost::asio::buffer_cast<const char*>(response_buffer_.data());
        response_content_.resize(bytes_transferred_ + buffer_size + 1);
        std::memcpy(&(response_content_[bytes_transferred_]), buf, buffer_size);
        bytes_transferred_ += buffer_size;
        response_buffer_.consume(buffer_size);
        content_length_ -= buffer_size;
      }

      // Read incoming bytes if any.
      int to_transfer = 0;
      if (transfer_all_) {
        to_transfer = content_length_;
      } else {
        to_transfer = std::min((size_t)content_length_,
                               requested_bytes_ - bytes_transferred_);
      }

      if (to_transfer <= 0) {
        if (content_length_ == 0) {
          finishTransfer();
        }
        return;
      }

      response_content_.resize(bytes_transferred_ + to_transfer);
      boost::asio::async_read(socket_,
          boost::asio::buffer(&(response_content_[bytes_transferred_]),
                              to_transfer),
          boost::asio::transfer_at_least(to_transfer),
          boost::bind(&http_source::handleRead, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
      bytes_transferred_ += to_transfer;
      content_length_ -= to_transfer;
    } else {
      throw std::ios_base::failure("read (content) error");
    }
  }

  void handleReadChunkSize(const boost::system::error_code& ec,
                           std::size_t bytes_transferred)
  throw(std::ios_base::failure) {
    if (!transfer_all_ && response_content_.size() >= requested_bytes_) {
      return;
    }

    if (!ec) {
      while (true) {
        // We are expecting a chunksize declaration of this form: "\r\nABC\r\n"
        // Read chunk size declaration and remove both "\r\n".
        const char* r =
                boost::asio::buffer_cast<const char*>(response_buffer_.data());

        if (response_buffer_.size() == 0) {
          throw std::ios_base::failure("read (chunksize) error");
        }
        if (r[0] == '\r' && r[1] == '\n') {
          response_buffer_.consume(2);
        }
        std::istream response_stream(&response_buffer_);
        std::size_t chunksize = 0;
        response_stream >> std::hex >> chunksize;
        response_buffer_.consume(2);

        // Check stop condition.
        if (chunksize == 0) {
          finishTransfer();
          return;
        }

        // chunk_bytes are the bytes of THIS chunk we already have transferred.
        // They will be copied to the response buffer directly.
        // We'll have to transfer (chunksize - chunk_bytes) bytes to get this
        // chunk to our response buffer. This can be 0 since it's possible
        // that we have already transferred (a part of) the next chunk.
        std::size_t buffered = response_buffer_.size();
        std::size_t chunk_bytes = std::min(buffered, chunksize);

        // Store transferred bytes of this chunk.
        const char* buf =
                boost::asio::buffer_cast<const char*>(response_buffer_.data());
        response_content_.resize(bytes_transferred_ + chunk_bytes);
        std::memcpy(&(response_content_[bytes_transferred_]), buf, chunk_bytes);
        bytes_transferred_ += chunk_bytes;

        if (buffered > chunksize) {
          // We have the next chunk already in the pipe.
          // Remove this chunk from the response buffer and continue reading.
          response_buffer_.consume(chunksize);
          std::size_t buf_length = response_buffer_.size();
          if (buf_length <= 2) {
            handleReadChunked(boost::system::error_code(), 0);
            return;
          }

          // We have not only the last "\r\n" but also some other bytes.
          // Check if this is a full chunk size declaration.
          const char* buf =
                boost::asio::buffer_cast<const char*>(response_buffer_.data());
          if (std::strchr(buf + 2, '\n') == NULL) {
            // Transfer missing bytes
            handleReadChunked(boost::system::error_code(), 0);
            return;
          }

          // There should be a full chunk size declaration and chunk data.
          // Continue reading.
          continue;
        } else if (chunksize - chunk_bytes != 0) {
          // This chunk is missing some bytes.
          // Allocate memory for the missing bytes.
          response_buffer_.consume(chunk_bytes);
          response_content_.resize(
                  bytes_transferred_ + chunksize - chunk_bytes);

          if (!transfer_all_) {
            // Start timeout timer.
            int timeout = ceil(
                chunksize - chunk_bytes / static_cast<double>(1024));
            timeout_timer_.expires_from_now(
                boost::posix_time::seconds(timeout + 3));
          }

          // Transfer them and read the next chunk size declaration.
          boost::asio::async_read(socket_,
              boost::asio::buffer(
                      &(response_content_[bytes_transferred_]),
                      chunksize - chunk_bytes),
              boost::asio::transfer_at_least(chunksize - chunk_bytes),
              boost::bind(&http_source::handleReadChunked, this,
                          boost::asio::placeholders::error,
                          boost::asio::placeholders::bytes_transferred));
          bytes_transferred_ += chunksize - chunk_bytes;
          break;
        } else {
          // Only the chunk data got transferred (without trailing "\r\n").
          return;
        }
      }
    } else {
      throw std::ios_base::failure("read (chunk) error");
    }
  }

  void handleReadChunked(const boost::system::error_code& ec,
                         std::size_t bytes_transferred)
  throw(std::ios_base::failure) {
    if (!ec) {
      // Read until the end of the chunk size declaration.
      if (!transfer_all_) {
        timeout_timer_.expires_from_now(boost::posix_time::seconds(3));
      }
      boost::asio::async_read_until(socket_, response_buffer_,
              boost::regex("\r?\n?[0-9a-fA-F]+\r\n"),
              boost::bind(&http_source::handleReadChunkSize, this,
                          boost::asio::placeholders::error,
                          boost::asio::placeholders::bytes_transferred));
    } else {
      throw std::ios_base::failure("read (chunksize) error");
    }
  }

  void handleTimeout(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }
    if (timeout_timer_.expires_from_now() < boost::posix_time::seconds(0)) {
      finishTransfer();
      return;
    }
  }

  void finishTransfer() {
    if (!transfer_finished_) {
      transfer_finished_ = true;

      // Shutdown socket.
      boost::system::error_code ignored_0;
      socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_0);

      // Close socket.
      boost::system::error_code ignored_1;
      socket_.close(ignored_1);

      timeout_timer_.cancel();
    }
  }

  void readCookies(const std::string& cookies) {
    // Prepare for search: results store, flags, start, end and compiled regex
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;
    std::string::const_iterator start = cookies.begin();
    std::string::const_iterator end = cookies.end();
    boost::regex r("(([^\\=]+)\\=([^;]+); ?)");

    // Execute regex search.
    while (boost::regex_search(start, end, what, r, flags)) {
      // Store cookie information.
      std::string key = what[2].str();
      std::string value = what[3].str();
      response_cookies_[key] = value;

      // Update search position and flags.
      start = what[0].second;
      flags |= boost::match_prev_avail;
      flags |= boost::match_not_bob;
    }
  }

  void startTimeout(int timeout) {
    timeout_timer_.cancel();
    timeout_timer_.expires_from_now(boost::posix_time::seconds(timeout));
    timeout_timer_.async_wait(boost::bind(&http_source::handleTimeout, this,
                                          boost::asio::placeholders::error));
  }

  boost::asio::io_service* io_service_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::streambuf response_buffer_;
  boost::asio::streambuf request_buffer_;
  boost::asio::deadline_timer timeout_timer_;
  int response_status_code_;
  std::map<std::string, std::string> response_cookies_;
  bool content_encoding_gzip_;
  bool transfer_encoding_chunked_;
  int content_length_;
  std::string response_location_;
  size_t bytes_transferred_;
  std::vector<char> response_content_;
  size_t requested_bytes_;
  bool transfer_all_;
  bool transfer_finished_;
};

/**
 * Convenience class for http_source class. Loads the response stream to a
 * string and returns the string.
 */
class request : boost::noncopyable {
 public:
  /**
   * Connects a http_source.
   * \sa http_source::http_source for constructor arguments
   * \exception std::ios_base::failure if the transfer fails
   */
  request(const std::string& host, const std::string& port,
          const std::string& path, const int method,
          const std::map<std::string, std::string>& headers,
          const void* content, const size_t content_length,
          const std::string& proxy_host)
    throw(std::ios_base::failure)
    : src_(host, port, path, method, headers,
           content, content_length, proxy_host, &io_service_),
      s_(boost::ref(src_)) {
  }

  /**
   * Downloads the content. Decompresses if needed.
   * \exception std::ios_base::failure if the transfer fails
   * \return the transferred content
   */
  std::string do_request(int timeout) throw(std::ios_base::failure) {
    src_.read(timeout);
    std::stringstream response;
    if (src_.content_encoding_gzip()) {
      boost::iostreams::filtering_streambuf<boost::iostreams::input> filter;
      filter.push(boost::iostreams::gzip_decompressor());
      filter.push(s_);
      boost::iostreams::copy(filter, response);
    } else {
      boost::iostreams::copy(s_, response);
    }
    return response.str();
  }

  /// Returns the cookies. \sa http_source::cookies()
  std::map<std::string, std::string>& cookies() { return src_.cookies(); }

  /// Returns the (redirect) location. \sa http_source::location()
  std::string& location() { return src_.location(); }

 private:
  typedef boost::reference_wrapper<http::http_source> http_stream_ref;
  boost::asio::io_service io_service_;
  http_source src_;
  boost::iostreams::stream<http_stream_ref> s_;
};

}  // namespace http

#endif  // HTTP_H_

