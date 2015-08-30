#include "./bot_browser.h"

#include <sstream>
#include <algorithm>

#include "boost/regex.hpp"

#include "http/useragents.h"

namespace botscript {

bot_browser::bot_browser(boost::asio::io_service* io_service,
                         const std::shared_ptr<bot>& b)
    : http::webclient(io_service,
                      http::useragents::random_ua(
                        b->config()->package() == "pg"
                          ? http::useragents::ua_type::PG
                          : http::useragents::ua_type::KV)),
      bot_(b),
      current_proxy_(-1),
      server_(b->config()->server()) {
  check_fun_ = std::bind(&bot_browser::check_proxy_response, this,
                         std::placeholders::_1);
}

bot_browser::~bot_browser() {
}

void bot_browser::cookies(std::map<std::string, std::string> const& cookies) {
  cookies_ = cookies;
  set_cookies_header();
}

bool bot_browser::change_proxy() {
  if (good_.empty()) {
    return false;
  }

  if (good_.size() == 1) {
    proxy& p = good_[0];
    log(bot::BS_LOG_NFO, std::string("set proxy to ") + p.str());
    set_proxy(p.host(), p.port());
    return false;
  }

  if (current_proxy_ >= good_.size() - 1) {
    current_proxy_ = 0;
  } else {
    ++current_proxy_;
  }

  proxy& p = good_[current_proxy_];
  log(bot::BS_LOG_NFO, std::string("set proxy to ") + p.str());
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
  int count = 0;
  for (; regex_iter != end; ++regex_iter) {
    proxy_list.push_back((*regex_iter)[1].str());
    if (++count > 10) {
      break;
    }
  }
  if (proxy_list.empty()) {
    log(bot::BS_LOG_ERR, "no valid proxies");
    return callback(0);
  }

  return set_proxy_list(proxy_list, callback);
}

void bot_browser::set_proxy_list(std::vector<std::string> proxy_list,
                                 std::function<void(int)> callback) {
  // Check whether there are already checks active.
  if (!proxy_checks_.empty()) {
    log(bot::BS_LOG_ERR, "proxy check already active");
    return callback(0);
  }

  // Build test request.
  using http::util::build_request;
  std::string base_url = server_ + "/";
  std::string check_request = build_request(http::url(base_url),
                                            http::util::GET, "", headers_,
                                            true);

  // Map proxy string to proxy object.
  std::vector<proxy> new_proxies(proxy_list.size());
  std::transform(begin(proxy_list), end(proxy_list), begin(new_proxies),
                 [](std::string const& s) { return proxy(s); });

  // Remove proxies that are not in the new list.
  good_.erase(
    std::remove_if(begin(good_), end(good_), [&new_proxies](proxy const& p) {
      return std::none_of(
          begin(new_proxies),
          end(new_proxies),
          [&p](proxy const& n) { return p == n; });
    }),
    end(good_));
  update_bot_proxy_status();

  // Start proxy checks.
  int proxy_checks = 0;
  for (const auto& pr : new_proxies) {
    if (proxy_checks_.find(pr.str()) == proxy_checks_.end() &&
        std::find(good_.begin(), good_.end(), pr) == good_.end()) {
      ++proxy_checks;
      auto c = std::make_shared<proxy_check>(io_service_, pr,
                                             check_request, &check_fun_);
      proxy_checks_[pr.str()] = c;
      c->check(std::bind(&bot_browser::proxy_check_callback, this, callback,
                         std::placeholders::_1, std::placeholders::_2));
    }
  }

  // Check if a single proxy check was started. Call callback if not.
  if (proxy_checks == 0) {
    log(bot::BS_LOG_NFO, "no new proxies, nothing to check");
    return callback(0);
  }

  // Log proxy count.
  std::string size_str = boost::lexical_cast<std::string>(proxy_checks);
  log(bot::BS_LOG_NFO, std::string("checking ") + size_str + " proxies");
}

void bot_browser::submit_with_retry(
    const std::string& xpath, const std::string& page,
    std::map<std::string, std::string> input_params,
    const std::string& action, callback cb,
    boost::posix_time::time_duration timeout, int tries,
    boost::system::error_code& ec) {
  std::function<void(int)> retry = boost::bind(&bot_browser::submit_with_retry, this,
                                               xpath, page,
                                               input_params, action,
                                               cb, timeout * 2, _1,
                                               boost::system::error_code());
  callback req_cb = boost::bind(&bot_browser::request_cb, this,
      shared_from_this(), tries, retry, cb, _1, _2);
  return webclient::submit(xpath, page, input_params, action,
                           req_cb, timeout, ec);
}

void bot_browser::request_with_retry(
    const http::url& u, int method, std::string body,
    callback cb,
    boost::posix_time::time_duration timeout, int tries) {
  std::function<void(int)> retry = boost::bind(&bot_browser::request_with_retry,
                                               this,
                                               u, method, body,
                                               cb, timeout * 2, _1);
  callback req_cb = boost::bind(&bot_browser::request_cb, this,
                            shared_from_this(), tries, retry, cb, _1, _2);
  webclient::request(u, method, body, req_cb, MAX_REDIRECT, timeout);
}


void bot_browser::request_cb(std::shared_ptr<bot_browser> /* self */, int tries,
                             std::function<void(int)> retry_fun, callback cb,
                             std::string response,
                             boost::system::error_code ec) {
  std::shared_ptr<bot> bot_lock = bot_.lock();
  if (bot_lock) {
    bot_lock->config()->cookies(cookies_);
  }

  if (!ec || tries <= 0) {
    if (ec) {
      log_error();
    }
    return cb(response, ec);
  } else {
    std::string msg = std::string("error: '") + ec.message() + "', ";
    if (tries == 1) {
      msg += "last try";
    } else {
      msg += boost::lexical_cast<std::string>(tries - 1) + " tries remaining";
    }
    log(bot::BS_LOG_ERR, msg);
    return retry_fun(tries - 1);
  }
}

void bot_browser::log_error() {
  log(bot::BS_LOG_DBG, "logging connection error");

  // Remove entries older than one hour.
  std::time_t now = std::time(NULL);
  std::time_t t = now - 3600;
  error_log_.remove_if([&t](std::time_t entry) { return entry < t; });

  // Add current entry.
  error_log_.push_back(now);

  // Change proxy if proxy failed >3 times last hour.
  if (error_log_.size() > 3) {
    log(bot::BS_LOG_DBG, "proxy failed too often - changing");
    change_proxy();
  }
}

void bot_browser::proxy_check_callback(std::function<void(int)> callback,
                                       std::shared_ptr<proxy_check> check,
                                       boost::system::error_code ec) {
  // Log check result.
  std::stringstream msg;
  msg << std::left << std::setw(21) << check->get_proxy().str()
      << ": " << (ec ? ec.message() : "connection successful");
  log(ec ? bot::BS_LOG_ERR : bot::BS_LOG_NFO, msg.str());

  // Remove check and add proxy to good_ if it passed the test.
  proxy_checks_.erase(check->get_proxy().str());
  if (!ec) {
    good_.push_back(check->get_proxy());
  }

  update_bot_proxy_status();

  // Call callback if this was the last check that finished.
  if (proxy_checks_.empty()) {
    if (good_.size() != 0) {
      change_proxy();
    } else {
      log(bot::BS_LOG_ERR, "no working proxy found");
    }

    // Last proxy - time to call back.
    return callback(good_.size());
  }
}

void bot_browser::update_bot_proxy_status() {
  // Convert good_ to proxy list string and set bot status.
  std::stringstream proxy_list;
  for (const auto& p : good_) {
    proxy_list << p.str() << "\n";
  }

  // Update bot status.
  std::shared_ptr<bot> bot_lock = bot_.lock();
  if (bot_lock != std::shared_ptr<bot>()) {
    bot_lock->status("base_proxy", proxy_list.str());
  }
}

bool bot_browser::check_proxy_response(const std::string& /* page */) {
  return true;
}

void bot_browser::log(int level, const std::string& message) {
  std::shared_ptr<bot> bot_lock = bot_.lock();
  if (bot_lock != std::shared_ptr<bot>()) {
    bot_lock->log(level, "browser", message);
  }
}

}  // namespace botscript
