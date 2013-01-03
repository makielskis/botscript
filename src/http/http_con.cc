// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "http_con.h"

#include <utility>

#include "boost/asio.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/bind.hpp"

namespace asio = boost::asio;

namespace http {

http_con::http_con(boost::asio::io_service* io_service,
                   std::string host, std::string port)
    : io_service_(io_service),
      resolver_(*io_service_),
      socket_(*io_service),
      req_timeout_timer_(*io_service),
      src_(std::make_shared<http_source>(&socket_)),
      host_(std::move(host)),
      port_(std::move(port)),
      connected_(false) {
}

void http_con::operator()(std::string request_str, callback cb) {
  request(shared_from_this(), std::move(request_str), std::move(cb),
          boost::system::error_code());
}

void http_con::request(std::shared_ptr<http_con> self,
                       std::string request_str, callback cb,
                       boost::system::error_code ec) {
  if (!ec) {
    if (!connected_) {
      resolve(std::move(self), std::move(request_str), std::move(cb));
    } else {
      src_->operator()(std::move(request_str),
                       std::bind(&http_con::request_finish, this,
                                 self, cb,
                                 std::placeholders::_1, std::placeholders::_2));
    }
  } else {
    cb(self, "", ec);
  }
}

void http_con::resolve(std::shared_ptr<http_con> self,
                       std::string request_str, callback cb) {
  asio::ip::tcp::resolver::query query(host_, port_);
  resolver_.async_resolve(query, boost::bind(&http_con::connect, this,
                                             self, request_str, cb,
                                             _1, _2));
}

void http_con::connect(std::shared_ptr<http_con> self,
                       std::string request_str, callback cb,
                       boost::system::error_code ec,
                       asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    asio::async_connect(socket_, iterator,
                        boost::bind(&http_con::on_connect, this,
                                    self, request_str, cb,
                                    _1, _2));
  } else {
    cb(self, "", ec);
  }
}

void http_con::on_connect(std::shared_ptr<http_con> self,
                          std::string request_str, callback cb,
                          boost::system::error_code ec,
                          asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    connected_ = true;
    src_->operator()(std::move(request_str),
                     boost::bind(&http_con::request_finish, this,
                                 std::move(self), std::move(cb),
                                 _1, _2));
  } else {
    cb(self, "", ec);
  }
}

void http_con::request_finish(std::shared_ptr<http_con> self, callback cb,
                              std::shared_ptr<http_source> http_src,
                              boost::system::error_code ec) {

  if (!ec || ec == asio::error::eof) {

    if (ec == asio::error::eof) {
      connected_ = false;
    }

    typedef boost::reference_wrapper<http::http_source> http_stream_ref;
    boost::iostreams::stream<http_stream_ref> s(boost::ref(*http_src));

    std::stringstream response;
    if (http_src->header("content-encoding") == "gzip") {
      boost::iostreams::filtering_streambuf<boost::iostreams::input> filter;
      filter.push(boost::iostreams::gzip_decompressor());
      filter.push(s);
      boost::iostreams::copy(filter, response);
    } else {
      boost::iostreams::copy(s, response);
    }

    cb(self, response.str(),
       ec != asio::error::eof ? ec : boost::system::error_code());
  } else {
    cb(self, "", ec);
  }
}

}  // namespace http
