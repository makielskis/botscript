// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "webclient.h"

#include <utility>
#include <sstream>

#include "boost/regex.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind.hpp"

#include "./url.h"
#include "./util.h"
#include "./useragents.h"
#include "./http_con.h"

namespace http {

error::http_category webclient::cat_;

webclient::webclient(boost::asio::io_service* io_service)
    : headers_(useragents::random_ua()),
      io_service_(io_service) {
}

webclient::~webclient() {
}

void webclient::set_proxy(std::string host, std::string port) {
  boost::lock_guard<boost::mutex> lock(proxy_mutex_);
  proxy_host_ = std::move(host);
  proxy_port_ = std::move(port);
}

void webclient::request(const url& u, int method, std::string body, callback cb,
                        int remaining_redirects,
                        boost::posix_time::time_duration timeout) {
  std::map<std::string, std::string> headers;
  {
    // Lock because of read access to the cookies map.
    boost::lock_guard<boost::mutex> lock(cookies_mutex_);
    headers = headers_;
  }

  std::string proxy_host, proxy_port;
  {
    // Lock because of read access to the proxy info.
    boost::lock_guard<boost::mutex> lock(proxy_mutex_);
    proxy_host = proxy_host_;
    proxy_port = proxy_port_;
  }

  // Get host and port to connect to.
  bool use_proxy = !proxy_host_.empty();
  std::string host = use_proxy ? proxy_host_ : u.host();
  std::string port = use_proxy ? proxy_port_ : u.port();

  // Start request.
  typedef std::shared_ptr<http_con> http_ptr;
  http_ptr c = std::make_shared<http_con>(io_service_, host, port, timeout);
  std::string req = util::build_request(u, method, body, headers_, use_proxy);
  c->operator()(req, boost::bind(&webclient::request_finish, this,
                                 u, timeout, remaining_redirects, cb,
                                 _1, _2, _3));
}

void webclient::request_finish(const url& request_url,
                               boost::posix_time::time_duration timeout,
                               int remaining_redirects, callback cb,
                               std::shared_ptr<http_con> con_ptr,
                               std::string response,
                               boost::system::error_code ec) {
  if (!ec) {
    // Store cookies.
    store_cookies(con_ptr->http_src().header("set-cookie"));

    // Check for redirect (new location given).
    std::string u = con_ptr->http_src().header("location");
    if (u.empty() || !remaining_redirects) {
      if (!boost::ends_with(request_url.str(), ".xml")) {
        response = util::tidy(response);
        response = util::store_location(response, request_url.str());
      }
      return cb(response, ec);
    }

    // Fix relative location declaration.
    if (!boost::starts_with(u, "http:")) {
      u = "http://" + request_url.host() + u;
    }

    // Redirect with HTTP GET
    return request(url(u), util::GET, "", cb, remaining_redirects - 1, timeout);
  } else {
    return cb("", ec);
  }
}

void webclient::submit(const std::string& xpath, const std::string& page,
                       std::map<std::string, std::string> input_params,
                       const std::string& action,
                       callback cb, boost::posix_time::time_duration timeout,
                       boost::system::error_code& ec) {
  // Load document.
  pugi::xml_document doc;
  doc.load(page.c_str());
  pugi::xml_node form;

  // Determine XML element from given XPath.
  try {
    form = doc.select_single_node(xpath.c_str()).node();
    if (form.empty()) {
      ec = boost::system::error_code(error::NO_FORM_OR_SUBMIT, cat_);
      return;
    }
  } catch(pugi::xpath_exception) {
    ec = boost::system::error_code(error::INVALID_XPATH, cat_);
    return;
  }

  // Store submit element.
  pugi::xml_node submit = form;

  if (std::strcmp(form.name(), "form") != 0) {
    // Node is not a form, so it has to be a submit element.
    if (std::strcmp(form.attribute("type").value(), "submit") != 0) {
      // Neither a form nor a submit? We're out.
      ec = boost::system::error_code(error::NO_FORM_OR_SUBMIT, cat_);
      return;
    }

    // The node is the submit element. Find corresponding form element.
    while (std::strcmp(form.name(), "form") != 0) {
      form = form.parent();

      if (form == form.root()) {
        // Root reached, no form found.
        ec = boost::system::error_code(error::SUBMIT_NOT_IN_FORM, cat_);
        return;
      }
    }
  }

  // Create parameters string.
  auto form_params = util::extract_parameters(form, submit, false);
  std::string params_str;
  for (auto param : form_params) {
    std::string value;

    auto it = input_params.find(param.first);
    if (input_params.end() == it) {
      // Fill in default parameter.
      value = param.second;
    } else {
      // Fill in given parameter.
      value = it->second;
      input_params.erase(param.first);
    }

    // Store URL encoded key and value.
    params_str.append(util::url_encode(param.first));
    params_str.append("=");
    params_str.append(util::url_encode(value));

    params_str.append("&");
  }

  // Check if all input parameters were found.
  if (!input_params.empty()) {
    ec = boost::system::error_code(error::PARAM_MISMATCH, cat_);
    return;
  }

  // Remove last '&'.
  if (params_str.length() > 1) {
    params_str = params_str.substr(0, params_str.length() - 1);
  }

  // Set action.
  std::string u = util::base_url(page);
  if (action.empty()) {
    const char* form_action = form.attribute("action").value();
    if (strstr(form_action, "http") != nullptr) {
      u = std::string(form_action);
    } else {
      u.append(form_action);
    }
  } else {
    u.append(action);
  }

  request(url(u), util::POST, params_str, cb, MAX_REDIRECT, timeout);
  ec = boost::system::error_code();
}

void webclient::store_cookies(const std::string& new_cookies) {
  std::map<std::string, std::string> cookies;

  // Prepare for search: results store, flags, start, end and compiled regex
  boost::match_results<std::string::const_iterator> what;
  boost::match_flag_type flags = boost::match_default;
  std::string::const_iterator start = new_cookies.begin();
  std::string::const_iterator end = new_cookies.end();
  boost::regex r("(([^\\=]+)\\=([^;]+); ?)");

  // Execute regex search.
  while (boost::regex_search(start, end, what, r, flags)) {
    // Store cookie information.
    std::string key = what[2].str();
    std::string value = what[3].str();
    cookies[key] = value;

    // Update search position and flags.
    start = what[0].second;
    flags |= boost::match_prev_avail;
    flags |= boost::match_not_bob;
  }

  // If there are no new cookies: nothing to do.
  if (cookies.size() == 0) {
    return;
  }

  // Store new cookies.
  {
    boost::lock_guard<boost::mutex> lock(cookies_mutex_);
    cookies_.insert(cookies.begin(), cookies.end());

    // Build and set new cookies string.
    std::stringstream cookies_str;
    for(auto cookie : cookies_) {
      cookies_str << cookie.first << "=" << cookie.second << "; ";
    }
    std::string cookie = cookies_str.str();
    cookie = cookie.substr(0, cookie.length() - 2);
    headers_["Cookie"] = cookie;
  }
}

}  // namespace http
