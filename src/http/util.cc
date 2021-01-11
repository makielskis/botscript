// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include "./util.h"

#include <buffio.h>
#include <tidy.h>

#include <cstdio>
#include <sstream>

#include "boost/lexical_cast.hpp"
#include "boost/regex.hpp"

namespace http {

namespace util {

std::string build_request(const url& u, const int method,
                          const std::string& content,
                          const std::map<std::string, std::string>& head,
                          bool use_proxy) {
  // Write the request line.
  std::stringstream request_stream;
  switch (method) {
    case GET:
      request_stream << "GET";
      break;

    case POST:
      request_stream << "POST";
      break;

    default:
      assert(false);
  }
  std::string path =
      (use_proxy ? ("https://" + u.host() + u.path()) : u.path());
  request_stream << " " << path;
  request_stream << " HTTP/1.1\r\n";
  request_stream << "Host: " << u.host() << "\r\n";

  // Write the headers.
  for (auto header : head) {
    request_stream << header.first << ": " << header.second << "\r\n";
  }

  // Set content length if available.
  if (!content.empty()) {
    std::string len_str = boost::lexical_cast<std::string>(content.length());
    request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
    request_stream << "Content-Length: " + len_str + "\r\n";
  }

  // Dirtyhack to trick knastvoegel servers.
  // TODO(felix) find a generic solution.
  if (u.host().find("knastvoegel.de") != std::string::npos) {
    request_stream
        << "Referer: http://www.knastvoegel.de/overview/profile.html\r\n";
  }

  // Finish headers.
  request_stream << "\r\n";

  // Write content.
  request_stream << content;

  return request_stream.str();
}

std::string location(const pugi::xml_document& doc) {
  auto tool = doc.select_single_node("/html/head/meta[@name = 'location']");
  return tool.node().empty() ? "" : tool.node().attribute("content").value();
}

std::string location(const std::string& page) {
  pugi::xml_document doc;
  doc.load(page.c_str());
  return location(doc);
}

std::string base_url(const std::string& page) {
  std::string url = location(page);
  if (url.empty()) {
    return url;
  } else {
    boost::regex url_regex("((.*://[a-zA-Z0-9\\.\\-]*)(:[0-9]*)?)");
    boost::match_results<std::string::const_iterator> what;
    boost::regex_search(url, what, url_regex);
    return what[1].str();
  }
}

std::string store_location(std::string p, const std::string& url) {
  // Find header start.
  std::size_t head = p.find("<head>");

  // Insert location meta tag if head tag was found.
  if (head != std::string::npos) {
    std::string tag = "\n<meta name=\"location\" content=\"" + url + "\" />\n";
    p = p.substr(0, head + 6) + tag + p.substr(head + 7, p.length());
  }

  return p;
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
  for (auto r : replace_map) {
    page.replace(page.find(r.first), r.first.length(), r.second);
  }

  // Tidy html!
  TidyBuffer output = {0, 0, 0, 0, 0};
  TidyBuffer errbuf = {0, 0, 0, 0, 0};
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
    ok = tidyOptSetValue(tdoc, TidyOutCharEncoding, "utf8");
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
    page = std::string(reinterpret_cast<char*>(output.bp), output.size);
  }

  tidyBufFree(&output);
  tidyBufFree(&errbuf);
  tidyRelease(tdoc);

  return page;
}

std::string url_encode(const std::string& s) {
  const std::string unreserved =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz0123456789-_.~";

  std::string escaped;
  for (size_t i = 0; i < s.length(); i++) {
    if (unreserved.find_first_of(s[i]) != std::string::npos) {
      escaped += s[i];
    } else if (s[i] == ' ') {
      escaped += "+";
    } else {
      escaped += "%";
      char buf[3];
      std::sprintf(buf, "%.2X", static_cast<unsigned char>(s[i]));
      escaped.append(buf);
    }
  }

  return escaped;
}

parameters extract_parameters(const pugi::xml_node& node,
                              const pugi::xml_node& submit, bool found) {
  // Parameters store.
  parameters p;

  // Iteratate all XML child nodes.
  for (pugi::xml_node::iterator iter = node.begin(); iter != node.end();
       iter++) {
    if (std::strcmp(iter->name(), "input") == 0 ||
        std::strcmp(iter->name(), "button") == 0) {
      // We found a Form input element. Store input parameters.
      if (std::strcmp(iter->attribute("type").value(), "image") == 0) {
        // Store image inputs
        std::string name = std::string(iter->attribute("name").value());
        if (name != "") {
          p.push_back(std::make_pair(name + ".x", ""));
          p.push_back(std::make_pair(name + ".y", ""));
        }
      } else {
        // Extract key and value for storage.
        std::string name = std::string(iter->attribute("name").value());
        std::string value = std::string(iter->attribute("value").value());

        if (name != "" &&
            (strcmp(iter->attribute("type").value(), "submit") != 0 ||
             *iter == submit || (node == submit && !found))) {
          // Store parameter if it has a name and
          // 1. it is a normal input (type != submit) or
          // 2. it is the submit element or
          // 3. no submit element is set and we did not already find one.
          p.push_back(std::make_pair(name, value));
          int cmp = std::strcmp(iter->attribute("type").value(), "submit");
          found = found || cmp == 0;
        }
      }
    } else if (std::strcmp(iter->name(), "select") == 0) {
      // Select input element. Special treatment: get selected option.
      std::string name = std::string(iter->attribute("name").value());
      if (name != "") {
        for (pugi::xml_node::iterator opt = iter->begin(); opt != iter->end();
             ++opt) {
          if (opt->attribute("selected") != nullptr) {
            p.push_back(std::make_pair(name, opt->attribute("value").value()));
          }
        }
      }
    } else if (iter->first_child()) {
      // Normal element. Find inputs in its children (recursive call).
      auto next_p =
          extract_parameters(*iter, (submit == node) ? *iter : submit, found);
      p.insert(p.end(), next_p.begin(), next_p.end());
    }
  }

  return p;
}

}  // namespace util

}  // namespace http
