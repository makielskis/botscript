// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./http_source.h"

#include <iomanip>

#include <cctype>
#include <cstdlib>
#include <string>
#include <algorithm>

#include "boost/lexical_cast.hpp"
#include "boost/regex.hpp"

namespace asio = boost::asio;

namespace http {

boost::regex http_source::chunk_size_rx_("\r?\n?[0-9a-fA-F]+\r\n");

http_source::http_source(asio::io_service* io_service,
                         asio::ip::tcp::resolver::iterator server,
                         std::string request)
  : socket_(*io_service),
    request_(std::move(request)),
    server_(server),
    status_code_(0),
    response_stream_(&buf_) {
}

std::streamsize http_source::read(char_type* s, std::streamsize n) {
  std::size_t ret = std::min(static_cast<std::size_t>(n), response_.size());
  std::memcpy(s, &(response_[0]), ret);
  std::memmove(&(response_[0]), &(response_[ret]), response_.size() - ret);
  response_.resize(response_.size() - ret);
  return ret;
}

void http_source::operator()(http_callback cb) {
  transfer(boost::system::error_code(),
           std::bind(cb, shared_from_this(), std::placeholders::_1));
}

#include "./coroutine/yield.hpp"
void http_source::transfer(boost::system::error_code ec,
                           std::function<void(boost::system::error_code)> cb) {
  if (ec == asio::error::eof) {
    return cb(ec);
  }

  if (!ec) {
    using std::placeholders::_1;
    auto re = std::bind(&http_source::transfer, this, _1, cb);
    std::size_t read, chunk_size, buffered, chunk_bytes, to_transfer, original;

    reenter(this) {
      yield asio::async_connect(socket_, server_, re);
      yield asio::async_write(socket_, asio::buffer(request_), re);
      yield asio::async_read_until(socket_, buf_, "\r\n\r\n", re);

      read_header();

      if (header_.find("content-length") != header_.end()) {
        read = copy_content(buf_.size());

        try {
          read_content_length();
        } catch(std::bad_cast) {
          boost::system::error_code error(
              boost::system::errc::illegal_byte_sequence,
              boost::system::system_category());
          return cb(error);
        }
        response_.resize(length_);

        yield asio::async_read(socket_,
                               asio::buffer(&(response_[read]), length_ - read),
                               asio::transfer_at_least(length_ - read), re);
        return cb(ec);
      } else if (header_.find("transfer-encoding") != header_.end()) {
        while(true) {
          yield asio::async_read_until(socket_, buf_, chunk_size_rx_, re);

          chunk_size = 0;
          response_stream_ >> std::hex >> chunk_size;
          buf_.consume(2);

          if (chunk_size == 0) {
            break;
          }

          buffered = buf_.size();
          chunk_bytes = std::min(buffered, chunk_size);
          copy_content(chunk_bytes);

          to_transfer = chunk_size - chunk_bytes;
          original = response_.size();
          response_.resize(response_.size() + to_transfer);

          yield asio::async_read(socket_,
              asio::buffer(&(response_[original]), to_transfer),
              asio::transfer_at_least(to_transfer), re);
        }
        return cb(ec);
      } else {
        while (true) {
          copy_content(buf_.size());
          yield asio::async_read(socket_, buf_, asio::transfer_at_least(1), re);
        }
        return cb(ec);
      }
    }
  } else {
    return cb(ec);
  }
}
#include "./coroutine/unyield.hpp"

void http_source::read_header() {
  std::string http_version;
  response_stream_ >> http_version;
  response_stream_ >> status_code_;
  response_stream_.ignore(128, '\n');

  std::string header;
  while (std::getline(response_stream_, header) && header != "\r") {
    header = header.substr(0, header.length() - 1);
    std::size_t seperator_pos = header.find(": ");
    if (seperator_pos == std::string::npos) {
      continue;
    }

    std::string header_to_lower;
    header_to_lower.resize(header.size());
    std::transform(header.begin(), header.end(),
                   header_to_lower.begin(), ::tolower);

    std::string header_key = header_to_lower.substr(0, seperator_pos);
    std::string header_value = header_to_lower.substr(seperator_pos + 2);
    header_[header_key] = header_value;
  }
}

std::size_t http_source::copy_content(std::size_t buffer_size) {
  if (buffer_size > 0) {
    const char* buf = asio::buffer_cast<const char*>(buf_.data());
    std::size_t original = response_.size();
    response_.resize(response_.size() + buffer_size);
    std::memcpy(&(response_[original]), buf, buffer_size);
    buf_.consume(buffer_size);
  }
  return buffer_size;
}

void http_source::read_content_length() throw(std::bad_cast) {
  length_ = boost::lexical_cast<int>(header_["content-length"]);
  if (length_ < 0) {
    throw std::bad_cast();
  }
}

}  // namespace http
