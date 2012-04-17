/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 11. April 2012  makielskis@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WEBCLIENT_H_
#define WEBCLIENT_H_

#include <string>
#include <map>
#include <sstream>
#include <utility>
#include <vector>
#include <exception>

#include "boost/utility.hpp"
#include "boost/foreach.hpp"
#include "boost/thread.hpp"
#include "tidy/tidy.h"
#include "tidy/buffio.h"
#include "pugixml/pugixml.hpp"

#include "./http.h"

#define MAX_REDIRECTS 3

namespace botscript {

class element_not_found_exception : public std::exception {
 public:
  explicit element_not_found_exception(const std::string& what)
    : error(what) {
  }

  ~element_not_found_exception() throw() {
  }

  virtual const char* what() const throw() {
    return error.c_str();
  }

 private:
  std::string error;
};

class webclient : boost::noncopyable {
 public:
  webclient() : headers_(randomHeaders()) {
  }

  webclient(const std::map<std::string, std::string>& headers,
            const std::string& proxy_host, const std::string& proxy_port)
    : proxy_host_(proxy_host),
      proxy_port_(proxy_port),
      headers_(headers) {
  }

  void proxy(const std::string host, const std::string port) {
    boost::lock_guard<boost::mutex> lock(mutex);
    proxy_host_ = host;
    proxy_port_ = port;
  }

  std::string proxy_host() const {
    return proxy_host_;
  }

  std::string proxy_port() const {
    return proxy_port_;
  }

  std::string request_get(std::string url)
  throw(std::ios_base::failure) {
    return request(url, botscript::http_source::GET, NULL, 0);
  }

  std::string request_post(std::string url,
                           const void* content, const size_t content_length)
  throw(std::ios_base::failure) {
    return request(url, botscript::http_source::POST, content, content_length);
  }

  std::string request(std::string url, const int method,
                      const void* content, const size_t content_length)
  throw(std::ios_base::failure) {
    // Lock complete request because of cookies r/w access.
    boost::lock_guard<boost::mutex> lock(mutex);

    int redirect_count = 0;
    while (true) {
      // Extract protocol, port, host address and path from the URL.
      boost::regex url_regex("(.*)://([a-zA-Z0-9\\.\\-]*)(:[0-9]*)?(.*)");
      boost::match_results<std::string::const_iterator> what;
      boost::regex_search(url, what, url_regex);

      std::string prot = what[1].str();
      std::string host = what[2].str();
      std::string port = what[3].str();
      std::string path = what[4].str();

      // Set port and path to default values of not set explicitly.
      port = port.length() == 0 ? prot : port.substr(1, port.length() - 1);
      path = path.length() == 0 ? "/" : path;

      // Do web request.
      botscript::request r(host, proxy_port_.empty() ? port : proxy_port_,
                           path, method, headers_,
                           content, content_length, proxy_host_);
      std::string response = r.do_request();
      std::map<std::string, std::string> cookies = r.cookies();
      storeCookies(cookies);

      // Check for redirect (new location given).
      std::string location = url;
      url = r.location();
      if (url.empty() || redirect_count == MAX_REDIRECTS) {
        // Insert location as meta-tag in the head.
        size_t head_start = response.find("<head>");
        if (head_start != std::string::npos) {
          std::string location_metatag = "\n<meta name=\"location\" content=\""
                                         + location + "\" />\n";
          response = response.substr(0, head_start + 6) + location_metatag
                     + response.substr(head_start + 7, response.length());
        }
        response = tidy(response);
        return response;
      }

      redirect_count++;
    }
  }

  std::string submit(const std::string& xpath, const std::string& page,
                     std::map<std::string, std::string> input_params,
                     const std::string& action)
  throw(element_not_found_exception, std::ios_base::failure) {
    // Determine XML element from given XPath.
    pugi::xml_document doc;
    doc.load(page.c_str());
    pugi::xml_node form = doc.select_single_node(xpath.c_str()).node();
    if (form.empty()) {
      throw element_not_found_exception("element does not exist");
    }

    // Store submit element.
    pugi::xml_node submit = form;

    if (std::strcmp(form.name(), "form") != 0) {
      // Node is not a form, so it has to be a submit element.
      if (std::strcmp(form.attribute("type").value(), "submit") != 0) {
        // Neither a form nor a submit? We're out.
        throw element_not_found_exception("element is not a form/submit");
      }

      // The node is the submit element. Find corresponding form element.
      while (std::strcmp(form.name(), "form") != 0) {
        form = form.parent();

        if (form == form.root()) {
          // Root reached, no form found.
          throw element_not_found_exception("submit element not in a form");
        }
      }
    }

    // Create parameters string.
    std::vector<std::pair<std::string, std::string> > form_params =
            getParameters(form, submit, false);
    std::string params_str;
    typedef std::pair<std::string, std::string> str_pair;
    BOOST_FOREACH(str_pair param, form_params) {
      std::string value;
      std::map<std::string, std::string>::const_iterator i =
              input_params.find(param.first);
      if (i == input_params.end()) {
        // Fill in default parameter.
        value = param.second;
      } else {
        // Fill in given parameter.
        value = i->second;
        input_params.erase(param.first);
      }

      // Store URL encoded key and value.
      params_str.append(urlEncode(param.first));
      params_str.append("=");
      params_str.append(urlEncode(value));

      params_str.append("&");
    }

    // Check if all input parameters were found.
    if (!input_params.empty()) {
      throw element_not_found_exception("parameters did not match");
    }

    // Remove last '&'.
    if (params_str.length() > 1) {
      params_str = params_str.substr(0, params_str.length() - 1);
    }

    // Set action.
    std::string url = getBaseURL(page);
    if (action.empty()) {
      const char* form_action = form.attribute("action").value();
      if (strstr(form_action, "http") != NULL) {
        url = std::string(form_action);
      } else {
        url.append(form_action);
      }
    } else {
      url.append(action);
    }

    return request(url, botscript::http_source::POST,
                   params_str.c_str(), params_str.length());
  }

 private:
  std::map<std::string, std::string> randomHeaders() {
    std::map<std::string, std::string> headers;
    headers["User-Agent"]      = "Mozilla/5.0 (Windows; U; Windows NT 6.1; de;"\
                                 "rv:1.9.2.8) Gecko/20100722 Firefox/3.6.8";
    headers["Accept"]          = "text/html,image/*,application/xhtml+xml,"\
                                 "application/xml;q=0.9,*/*;q=0.8";
    headers["Accept-Language"] = "de-de,de;q=0.8,en-us;q=0.5,en;q=0.3";
    headers["Accept-Encoding"] = "gzip,deflate";
    headers["Accept-Charset"]  = "ISO-8859-1,utf-8;q=0.7,*;q=0.7";
    headers["Keep-Alive"]      = "115";
    headers["Connection"]      = "keep-alive";
    return headers;
  }

  void storeCookies(const std::map<std::string, std::string>& cookies) {
      if (cookies.size() == 0) {
        return;
      }

      // Store new cookies.
      cookies_.insert(cookies.begin(), cookies.end());

      // Build and set new cookies string.
      std::stringstream cookies_str;
      typedef std::pair<std::string, std::string> str_pair;
      BOOST_FOREACH(str_pair cookie, cookies_) {
        cookies_str << cookie.first << "=" << cookie.second << "; ";
      }
      std::string cookie = cookies_str.str();
      cookie = cookie.substr(0, cookie.length() - 2);
      headers_["Cookie"] = cookie;
  }

  std::string tidy(std::string page) {
    // Fix disabled inputs.
    static boost::regex select_regex("<input([^>]*)disabled([^>]*)>");
    boost::sregex_iterator select_i(page.begin(), page.end(), select_regex);
    boost::sregex_iterator end;
    std::map<std::string, std::string> replace_map;
    for (; select_i != end; ++select_i) {
      std::string inner1 = (*select_i)[1];
      std::string inner2 = (*select_i)[2];
      std::stringstream buf;
      buf << "<input disabled=\"true\"" << inner1 << inner2 << ">\n";
      std::string replace = (*select_i).str();
      replace_map[replace] = buf.str();
    }
    typedef std::pair<std::string, std::string> str_pair;
    BOOST_FOREACH(str_pair r, replace_map) {
      page.replace(page.find(r.first), r.first.length(), r.second);
    }

    // Tidy html!
    TidyBuffer output = {0};
    TidyBuffer errbuf = {0};
    int rc = -1;
    Bool ok;

    TidyDoc tdoc = tidyCreate();

    ok = tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
    if (ok) {
      ok = tidyOptSetBool(tdoc, TidyShowWarnings, no);
    }
    if (ok) {
      ok = tidyOptSetInt(tdoc, TidyWrapLen, 10000);
    }
    if (ok) {
      ok = tidyOptSetValue(tdoc, TidyInCharEncoding, "utf8");
    }
    if (ok) {
      ok = tidyOptSetValue(tdoc, TidyOutCharEncoding, "ascii");
    }
    if (ok) {
      rc = tidySetErrorBuffer(tdoc, &errbuf);
    }
    if (rc >= 0) {
      rc = tidyParseString(tdoc, page.c_str());
    }
    if (rc >= 0) {
      rc = tidyCleanAndRepair(tdoc);
    }
    if (rc >= 0) {
      rc = tidyRunDiagnostics(tdoc);
    }
    if (rc > 1) {
      rc = (tidyOptSetBool(tdoc, TidyForceOutput, yes) ? rc : -1);
    }
    if (rc >= 0) {
      rc = tidySaveBuffer(tdoc, &output);
    }

    if (rc >= 0) {
      page = std::string(reinterpret_cast<char*>(output.bp), output.allocated);
    }

    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);

    return page;
  }

  std::vector<std::pair<std::string, std::string> > getParameters(
          const pugi::xml_node& node,
          const pugi::xml_node& submit,
          bool submit_found) {
    // Parameters store.
    std::vector<std::pair<std::string, std::string> > parameters;

    // Iteratate all XML child nodes.
    for (pugi::xml_node::iterator iter = node.begin();
         iter != node.end(); iter++) {
      if (std::strcmp(iter->name(), "input") == 0
          || std::strcmp(iter->name(), "button") == 0) {
        // We found a Form input element. Store input parameters.
        if (std::strcmp(iter->attribute("type").value(), "image") == 0) {
          // Store image inputs
          std::string name = std::string(iter->attribute("name").value());
          if (name != "") {
            parameters.push_back(
                    std::pair<std::string, std::string > (name + ".x", ""));
            parameters.push_back(
                    std::pair<std::string, std::string > (name + ".y", ""));
          }
        } else {
          // Extract key and value for storage.
          std::string name = std::string(iter->attribute("name").value());
          std::string value = std::string(iter->attribute("value").value());

          if (name != ""
              && (strcmp(iter->attribute("type").value(), "submit") != 0
              || *iter == submit
              || (node == submit && !submit_found))) {
            // Store parameter if it has a name and
            // 1. it is a normal input (type != submit) or
            // 2. it is the submit element or
            // 3. no submit element is set and we did not already find one.
            parameters.push_back(
                    std::pair<std::string, std::string>(name, value));
            submit_found = submit_found
                || std::strcmp(iter->attribute("type").value(), "submit") == 0;
          }
        }
      } else if (std::strcmp(iter->name(), "select") == 0) {
        // Select input element. Special treatment: get selected option.
        std::string name = std::string(iter->attribute("name").value());
        if (name != "") {
          for (pugi::xml_node::iterator options = iter->begin();
               options != iter->end(); options++) {
            if (options->attribute("selected").as_bool()) {
              parameters.push_back(
                      std::pair<std::string, std::string > (name,
                              options->attribute("value").value()));
            }
          }
        }
      } else if (iter->first_child()) {
        // Normal element. Find inputs in its children (recursive call).
        std::vector<std::pair<std::string, std::string> > sub_parameters =
          getParameters(*iter, (submit == node) ? *iter : submit, submit_found);
        parameters.insert(parameters.end(), sub_parameters.begin(),
                          sub_parameters.end());
      }
    }
    return parameters;
  }

  std::string urlEncode(const std::string &s) {
    const std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"\
            "abcdefghijklmnopqrstuvwxyz0123456789-_.~";

    std::string escaped = "";
    for (size_t i = 0; i < s.length(); i++) {
      if (unreserved.find_first_of(s[i]) != std::string::npos) {
        escaped.push_back(s[i]);
      } else if (s[i] == ' ') {
        escaped.append("+");
      } else {
        escaped.append("%");
        char buf[3];
        snprintf(buf, sizeof(buf), "%.2X", (unsigned char) s[i]);
        escaped.append(buf);
      }
    }

    return escaped;
  }

  static std::string getLocation(const pugi::xml_document &doc) {
    pugi::xpath_node tool = doc.select_single_node(
                                    "/html/head/meta[@name = 'location']");
    return tool.node().empty() ? "" : tool.node().attribute("content").value();
  }

  static std::string getLocation(const std::string& page) {
    pugi::xml_document doc;
    doc.load(page.c_str());
    return getLocation(doc);
  }

  static std::string getBaseURL(const std::string& page) {
      std::string url = getLocation(page);
      if (url.empty()) {
        return url;
      } else {
        boost::regex url_regex("((.*://[a-zA-Z0-9\\.\\-]*)(:[0-9]*)?)");
        boost::match_results<std::string::const_iterator> what;
        boost::regex_search(url, what, url_regex);
        return what[1].str();
      }
  }

  boost::mutex mutex;
  std::string proxy_host_;
  std::string proxy_port_;
  std::map<std::string, std::string> headers_;
  std::map<std::string, std::string> cookies_;
};

}  // namespace botscript

#endif  // WEBCLIENT_H_

