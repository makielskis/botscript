// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "http_con.h"

#include <utility>

#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"

#include "./webclient.h"

namespace asio = boost::asio;

namespace http {

http_con::http_con(boost::asio::io_service* io_service, std::string host,
                   std::string port, boost::posix_time::time_duration timeout)
    : io_service_(io_service),
      resolver_(*io_service_),
      ssl_ctx_(asio::ssl::context::sslv23),
      socket_(*io_service, ssl_ctx_),
      req_timeout_timer_(*io_service, std::move(timeout)),
      src_(std::make_shared<http_source>(&socket_)),
      host_(std::move(host)),
      port_(std::move(port)),
      connected_(false) {
  socket_.set_verify_mode(asio::ssl::verify_none);
}

void http_con::operator()(std::string request_str, callback cb) {
  req_timeout_timer_.async_wait(
      boost::bind(&http_con::timer_callback, this, shared_from_this(), _1));
  return request(shared_from_this(), std::move(request_str), std::move(cb));
}

void http_con::request(std::shared_ptr<http_con> self, std::string request_str,
                       callback cb) {
  if (!connected_) {
    return resolve(std::move(self), std::move(request_str), std::move(cb));
  } else {
    return src_->operator()(
        std::move(request_str),
        std::bind(&http_con::request_finish, this, std::move(self),
                  std::move(cb), std::placeholders::_1, std::placeholders::_2));
  }
}

void http_con::resolve(std::shared_ptr<http_con> self, std::string request_str,
                       callback cb) {
  asio::ip::tcp::resolver::query query(host_, port_);
  return resolver_.async_resolve(
      query, boost::bind(&http_con::connect, this, std::move(self),
                         std::move(request_str), std::move(cb), _1, _2));
}

void http_con::connect(std::shared_ptr<http_con> self, std::string request_str,
                       callback cb, boost::system::error_code ec,
                       asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    return asio::async_connect(
        socket_.lowest_layer(), iterator,
        boost::bind(&http_con::on_connect, this, std::move(self),
                    std::move(request_str), std::move(cb), _1));
  } else {
    return cb(self, "", ec);
  }
}

void http_con::on_connect(std::shared_ptr<http_con> self,
                          std::string request_str, callback cb,
                          boost::system::error_code ec) {
  if (!ec) {
    return socket_.async_handshake(
        asio::ssl::stream_base::client,
        boost::bind(&http_con::on_ssl_handshake, this, std::move(self),
                    std::move(request_str), std::move(cb), _1));
  } else {
    return cb(self, "", ec);
  }
}

void http_con::on_ssl_handshake(std::shared_ptr<http_con> self,
                                std::string request_str, callback cb,
                                boost::system::error_code ec) {
  if (!ec) {
    connected_ = true;
    return src_->operator()(
        std::move(request_str),
        boost::bind(&http_con::request_finish, this, std::move(self),
                    std::move(cb), _1, _2));
  } else {
    return cb(self, "", ec);
  }
}

void http_con::timer_callback(std::shared_ptr<http_con> /* self */,
                              const boost::system::error_code& ec) {
  if (boost::asio::error::operation_aborted != ec) {
    connected_ = false;
    boost::system::error_code ignored;
    socket_.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                                    ignored);
    socket_.lowest_layer().close(ignored);
  }
}

void http_con::request_finish(std::shared_ptr<http_con> self, callback cb,
                              std::shared_ptr<http_source> http_src,
                              boost::system::error_code ec) {
  req_timeout_timer_.cancel();

  if (!ec || ec == asio::error::eof) {
    if (ec == asio::error::eof) {
      connected_ = false;
    }

    typedef boost::reference_wrapper<http::http_source> http_stream_ref;
    boost::iostreams::stream<http_stream_ref> s(boost::ref(*http_src));

    std::stringstream response;
    if (http_src->header("content-encoding") == "gzip") {
      try {
        boost::iostreams::filtering_streambuf<boost::iostreams::input> filter;
        filter.push(boost::iostreams::gzip_decompressor());
        filter.push(s);
        boost::iostreams::copy(filter, response);
      } catch (const boost::iostreams::gzip_error& e) {
        return cb(self, "", boost::system::error_code(error::GZIP_FAILURE,
                                                      webclient::cat_));
      }
    } else {
      boost::iostreams::copy(s, response);
    }

    return cb(self, response.str(), boost::system::error_code());
  } else {
    return cb(self, "", ec);
  }
}

}  // namespace http
