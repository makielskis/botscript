// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef BOT_BROWSER_H_
#define BOT_BROWSER_H_

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

#include "boost/asio/io_service.hpp"

#include "./http/util.h"
#include "./http/url.h"
#include "./http/webclient.h"
#include "./proxy_check.h"
#include "./bot.h"

namespace botscript {

/// The bot_browser extens the webclient class with the following features:
///   - Asynchronous set_proxy_list function to set a proxy list
///   - Counts fails in the last N seconds and changes the proxy if the current
///     proxy failed more than X times in this time period
class bot_browser : public http::webclient {
 public:
  /// \param io_service the Asio io_service to use
  /// \param b the bot that owns this bot_browser
  bot_browser(boost::asio::io_service* io_service, bot* b)
    : http::webclient(io_service),
      bot_(b) {
    check_fun_ = std::bind(&bot_browser::check_proxy_response, this,
                           std::placeholders::_1);
    
    using http::util::build_request;
    std::string base_url = bot_->server();
    check_requst_ = build_request(http::url(base_url), http::util::GET, "",
                                  headers_, true);
  }

  void set_proxy_list(std::vector<std::string> proxy_list,
                      std::function<void(int)> callback) {
    if (!proxy_checks_.empty()) {
      bot_->log(bot::BS_LOG_ERR, "webclient", "proxy check already active");
      return callback(0);
    }

    for (const auto& p : proxy_list) {
      if (proxy_checks_.find(p) == proxy_checks_.end()) {
        typedef std::shared_ptr<proxy_check> check_ptr;
        check_ptr c = std::make_shared<proxy_check>(io_service_, proxy(p),
                                                    check_requst_, &check_fun_);
        proxy_checks_.push_back(c);
        c->check(std::bind(&bot_browser::proxy_check_callback, this,
                           std::placeholders::_1, std::placeholders::_2));
      }
    }
  }

  void proxy_check_callback(std::shared_ptr<proxy_check> check,
                            boost::system::error_code ec) {
  }

  bool check_proxy_response(const std::string& page) {
    return page.find("Farbflut") != std::string::npos;
  }

 private:
   bot* bot_;
   std::vector<std::string> checked_proxies_;
   std::map<std::string, proxy_check> proxy_checks_;
   std::string check_request_;
   std::function<bool(std::string)> check_fun_;
};

}  // namespace botscript

#endif  // BOT_BROWSER_H_
