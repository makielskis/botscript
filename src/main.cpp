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

#include <utility>
#include <string>
#include <algorithm>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#include "boost/thread.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/filesystem.hpp"

#include "./bot.h"
#include "./bot_factory.h"

int main(int argc, char* argv[]) {
  std::map<std::string, std::string> configs;

  using boost::filesystem::directory_iterator;
  for (directory_iterator i = directory_iterator("configs");
      i != directory_iterator(); ++i) {
    std::string path = i->path().relative_path().generic_string();

    std::ifstream file;
    file.open(path.c_str(), std::ios::in);
    std::stringstream content;
    boost::iostreams::copy(file, content);
    file.close();

    configs[path] = content.str();
  }

  typedef boost::shared_ptr<botscript::bot> bot_ptr;
  std::set<bot_ptr> bots;
  botscript::bot_factory factory;
  factory.init(2);
  typedef std::pair<std::string, std::string> str_pair;
  BOOST_FOREACH(str_pair config, configs) {
    try {
      bots.insert(factory.create_bot(config.second));
    } catch(const botscript::bad_login_exception& e) {
      std::cout << config.first << " - bad login\n";
    } catch(const botscript::lua_exception& e) {
      std::cout << config.first << " - " << e.what() << "\n";
    } catch(const botscript::invalid_proxy_exception& e) {
      std::cout << config.first << " - invalid proxy\n";
    }
  }

  while (true) { boost::this_thread::sleep(boost::posix_time::hours(24)); }

  BOOST_FOREACH(bot_ptr b, bots) {
    b->shutdown();
  }

  return 0;
}

