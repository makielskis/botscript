// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef BOT_BROWSER_H_
#define BOT_BROWSER_H_

#include <ctime>
#include <list>
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

class bot;

/// The bot_browser extens the webclient class with the following functionality:
///   - Asynchronous set_proxy_list function to set a proxy list
///   - Counts fails in the last hour and changes the proxy if the current
///     proxy failed more than 8 times in this time period
///   - Retries failed requests up to three times with increasing timeout values
///     (first try 15sec, second try 30sec, last try 60sec)
class bot_browser : public http::webclient,
                    public std::enable_shared_from_this<bot_browser> {
 public:
  /// \param io_service the Asio io_service to use
  /// \param b the bot that owns this bot_browser
  bot_browser(boost::asio::io_service* io_service, bot* b);

  bool change_proxy();

  void set_proxy_list(const std::string& proxy_list,
                      std::function<void(int)> callback);
  void set_proxy_list(std::vector<std::string> proxy_list,
                      std::function<void(int)> callback);

  void submit(const std::string& xpath, const std::string& page,
              std::map<std::string, std::string> input_params,
              const std::string& action, callback cb,
              boost::posix_time::time_duration timeout, int tries,
              boost::system::error_code& ec);

  void request(const http::url& u, int method, std::string body, callback cb,
               boost::posix_time::time_duration timeout, int tries);

 private:
  void request_cb(std::shared_ptr<bot_browser> self, int tries,
                  std::function<void(int)> retry_fun, callback cb,
                  std::string response, boost::system::error_code ec);

  void log_error();

  void proxy_check_callback(std::function<void(int)> callback,
                            std::shared_ptr<proxy_check> check,
                            boost::system::error_code ec);

  bool check_proxy_response(const std::string& page);

  bot* bot_;
  std::vector<proxy> good_;
  std::map<std::string, std::shared_ptr<proxy_check>> proxy_checks_;
  std::function<bool(std::string)> check_fun_;
  std::size_t current_proxy_;
  std::list<std::time_t> error_log_;
};

}  // namespace botscript

#endif  // BOT_BROWSER_H_
