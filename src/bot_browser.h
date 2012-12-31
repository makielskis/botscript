#ifndef BOT_BROWSER_H_
#define BOT_BROWSER_H_

#include "./http/webclient.h"

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
  }

  void set_proxy_list(std::vector<std::string> proxy_list,
                      std::function<void(bool)> callback) {
    
  }

 private:
  boost::asio::deadline_timer timer_;
};

}  // namespace botscript

#endif  // BOT_BROWSER_H_
