#include "./bot_browser.h"

#include <sstream>

#include "boost/regex.hpp"

namespace botscript {

bot_browser::bot_browser(boost::asio::io_service* io_service, bot* b)
    : http::webclient(io_service),
      bot_(b),
      current_proxy_(-1) {
  check_fun_ = std::bind(&bot_browser::check_proxy_response, this,
                         std::placeholders::_1);
}

bool bot_browser::change_proxy() {
  if (good_.empty()) {
    return false;
  }

  if (good_.size() == 1) {
    proxy& p = good_[0];
    bot_->log(bot::BS_LOG_NFO, "browser",
              std::string("set proxy to ") + p.str());
    set_proxy(p.host(), p.port());
    return false;
  }

  if (current_proxy_ >= good_.size() - 1) {
    current_proxy_ = 0;
  } else {
    ++current_proxy_;
  }

  proxy& p = good_[current_proxy_];
  bot_->log(bot::BS_LOG_NFO, "browser", std::string("set proxy to ") + p.str());
  set_proxy(p.host(), p.port());
  return true;
}

void bot_browser::set_proxy_list(const std::string& proxies,
                                 std::function<void(int)> callback) {
  // Extract proxies with regular expression.
  std::vector<std::string> proxy_list;
  boost::regex regex(
      "((\\d{1, 3}\\.\\d{1, 3}\\.\\d{1, 3}\\.\\d{1, 3}):(\\d{1, 5}))");
  boost::sregex_iterator regex_iter(proxies.begin(), proxies.end(), regex), end;
  for (; regex_iter != end; ++regex_iter) {
    proxy_list.push_back((*regex_iter)[1].str());
  }
  return set_proxy_list(proxy_list, callback);
}

void bot_browser::set_proxy_list(std::vector<std::string> proxy_list,
                                 std::function<void(int)> callback) {
  // Check whether there are already checks active.
  if (!proxy_checks_.empty()) {
    bot_->log(bot::BS_LOG_ERR, "browser", "proxy check already active");
    return callback(0);
  }

  // Build test request.
  using http::util::build_request;
  std::string base_url = bot_->server() + "/index.html";
  std::string check_request = build_request(http::url(base_url),
                                            http::util::GET, "", headers_,
                                            true);

  // Log proxy count.
  std::string size_str = boost::lexical_cast<std::string>(proxy_list.size());
  bot_->log(bot::BS_LOG_NFO, "browser",
            std::string("checking ") + size_str + " proxies");

  // Start proxy checks.
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
  // Log check result.
  std::stringstream msg;
  msg << std::left << std::setw(21) << check->get_proxy().str()
      << ": " << ec.message();
  bot_->log(ec ? bot::BS_LOG_ERR : bot::BS_LOG_NFO, "browser", msg.str());

  // Remove check and add proxy to good_ if it passed the test.
  proxy_checks_.erase(check->get_proxy().str());
  if (!ec) {
    good_.push_back(check->get_proxy());
  }

  // Call callback if this was the last check that finished.
  if (proxy_checks_.empty()) {
    if (good_.size() != 0) {
      change_proxy();
    } else {
      bot_->log(bot::BS_LOG_ERR, "browser", "no working proxy found");
    }
    callback(good_.size());
  }
}

bool bot_browser::check_proxy_response(const std::string& page) {
  return page.find("Farbflut") != std::string::npos;
}

}  // namespace botscript
