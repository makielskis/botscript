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

http_source::http_source(boost::asio::ip::tcp::socket* socket)
    : socket_(socket),
      status_code_(0),
      response_stream_(&buf_),
      length_(0) {
}

std::streamsize http_source::read(char_type* s, std::streamsize n) {
  std::size_t ret = std::min(static_cast<std::size_t>(n), response_.size());
  if (ret != 0) {
    std::memcpy(s, &(response_[0]), ret);
    if (response_.size() != ret) {
      std::memmove(&(response_[0]), &(response_[ret]), response_.size() - ret);
    }
    response_.resize(response_.size() - ret);
  }
  return ret;
}

void http_source::operator()(std::string request, callback cb) {
  response_.resize(0);
  header_.clear();
  request_ = std::move(request);
  std::shared_ptr<http_source> s = shared_from_this();
  transfer(boost::system::error_code(),
           std::bind(cb, s, std::placeholders::_1));
}

#include "./coroutine/yield.hpp"
void http_source::transfer(boost::system::error_code ec,
                           std::function<void(boost::system::error_code)> cb) {
  if (ec == asio::error::eof) {
    coroutine_ref(this) = 0;
    return cb(ec);
  }

  if (!ec) {
    if (is_complete()) {
      coroutine_ref(this) = 0;
    }

    using std::placeholders::_1;
    auto re = std::bind(&http_source::transfer, this, _1, cb);
    std::size_t read, chunk_size, chunk_bytes, to_transfer, original;

    reenter(this) {
      yield asio::async_write(*socket_, asio::buffer(request_), re);
      yield asio::async_read_until(*socket_, buf_, "\r\n\r\n", re);

      read_header();

      if (header_.find("content-length") != header_.end()) {
        read = copy_content(buf_.size());

        try {
          read_content_length();
        } catch(std::bad_cast) {
          using namespace boost::system;
          return cb(error_code(errc::illegal_byte_sequence, system_category()));
        }
        response_.resize(length_);

        if (static_cast<int>(length_) - static_cast<int>(read) > 0) {
          yield asio::async_read(*socket_,
                                 asio::buffer(&(response_[read]),
                                              length_ - read),
                                 asio::transfer_at_least(length_ - read), re);
        }
      } else if (header_.find("transfer-encoding") != header_.end()) {
        while(true) {
          yield asio::async_read_until(*socket_, buf_, chunk_size_rx_, re);

          chunk_size = 0;
          response_stream_ >> std::hex >> chunk_size;
          buf_.consume(2);

          if (chunk_size == 0) {
            break;
          }

          chunk_bytes = std::min(buf_.size(), chunk_size);
          copy_content(chunk_bytes);

          if (static_cast<int>(chunk_size) - static_cast<int>(chunk_bytes) > 0) {
            to_transfer = chunk_size - chunk_bytes;
            original = response_.size();
            response_.resize(response_.size() + to_transfer);
            yield asio::async_read(*socket_,
                asio::buffer(&(response_[original]), to_transfer),
                asio::transfer_at_least(to_transfer), re);
          }
        }
      } else {
        while (true) {
          copy_content(buf_.size());
          yield asio::async_read(*socket_, buf_, asio::transfer_at_least(1), re);
        }
      }
      return cb(ec);
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

    std::string key = header.substr(0, seperator_pos);
    std::string key_to_lower;
    key_to_lower.resize(key.size());
    std::transform(key.begin(), key.end(), key_to_lower.begin(), ::tolower);
    std::string header_value = header.substr(seperator_pos + 2);

    if (key_to_lower == "set-cookie") {
      std::size_t val_end_pos = header_value.find(";");
      if (val_end_pos != std::string::npos) {
        header_["set-cookie"] += header_value.substr(0, val_end_pos + 1);
      }
    } else {
      header_[key_to_lower] = header_value;
    }
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
