#include "./bot_browser.h"

namespace botscript {

bot_browser::bot_browser(boost::asio::io_service* io_service, bot* b)
    : http::webclient(io_service),
      bot_(b) {
  check_fun_ = std::bind(&bot_browser::check_proxy_response, this,
                         std::placeholders::_1);
}

void bot_browser::set_proxy_list(std::vector<std::string> proxy_list,
                                 std::function<void(int)> callback) {
  if (!proxy_checks_.empty()) {
    bot_->log(bot::BS_LOG_ERR, "webclient", "proxy check already active");
    return callback(0);
  }

  using http::util::build_request;
  std::string base_url = bot_->server();
  std::string check_request = build_request(http::url(base_url),
                                            http::util::GET, "", headers_,
                                            true);

  for (const std::string& p : proxy_list) {
    proxy pr(p);
    if (proxy_checks_.find(p) == proxy_checks_.end() &&
        std::find(good_.begin(), good_.end(), pr) == good_.end()) {
      auto c = std::make_shared<proxy_check>(io_service_, pr,
                                             check_request, &check_fun_);
      proxy_checks_[p] = c;
      c->check(std::bind(&bot_browser::proxy_check_callback, this, callback,
                         std::placeholders::_1, std::placeholders::_2));
    }
  }
}

void bot_browser::submit(const std::string& xpath, const std::string& page,
                         std::map<std::string, std::string> input_params,
                         const std::string& action, callback cb) {
  webclient::submit(xpath, page, input_params, action, cb);
}

void bot_browser::request(const http::url& u, int method, std::string body,
                          callback cb, int remaining_redirects) {
  webclient::request(u, method, body, cb, remaining_redirects);
}

void bot_browser::proxy_check_callback(std::function<void(int)> callback,
                                       std::shared_ptr<proxy_check> check,
                                       boost::system::error_code ec) {
  proxy_checks_.erase(check->get_proxy().str());
  if (!ec) {
    good_.push_back(check->get_proxy());
  }
  if (proxy_checks_.empty()) {
    callback(good_.size());
  }
}

bool bot_browser::check_proxy_response(const std::string& page) {
  return page.find("Farbflut") != std::string::npos;
}

}  // namespace botscript
