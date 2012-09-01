/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 8. April 2012  makielskis@gmail.com
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
 * along with this program.  If not, see .
 */

#include <map>
#include <set>
#include <string>
#include <iostream>
#include <fstream>

#include "boost/asio/io_service.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/utility.hpp"
#include "boost/asio/io_service.hpp"

#include "./http/webclient.h"

int main(int argc, char* argv[]) {
  using http::http_source;
  typedef boost::reference_wrapper<http_source> http_stream_ref;

  std::string host = "pugixml.googlecode.com";
  std::string port = "80";
  std::string path = "/files/pugixml-0.1.zip";
  int method = http_source::GET;
  std::map<std::string, std::string> headers;
  boost::asio::io_service io_service;
  int timeout = 20;

  std::ofstream response;
  response.open("test.zip");

  http_source src_(host, port, path, method, headers, NULL, 0, "", &io_service);
  boost::iostreams::stream<http_stream_ref> s_(boost::ref(src_));
  src_.read(timeout);
  boost::iostreams::copy(s_, response);

  io_service.run();

  return 0;
}
