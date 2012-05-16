/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 16. May 2012  makielskis@gmail.com
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

#ifndef PROXY_MANAGER_H_
#define PROXY_MANAGER_H_

#include <stack>
#include <string>

#include "client/dbclient.h"

#include "./bot.h"

namespace botscript {

class no_proxies_exception : public std::exception {
};

class proxy_manager {
 public:
  static std::string next_proxy() throw(no_proxies_exception) {
    boost::lock_guard<boost::mutex> lock(db_mutex_);
    std::auto_ptr<mongo::DBClientCursor> cursor =
        db_.query("test.proxies", QUERY("inuse" << false << "good" << true));
    while(cursor->more()) {
      mongo::BSONObj p = cursor->next();
      std::string address = p.getStringField("address");
      std::cout << address << "\n";
      return address;
    }
    throw no_proxies_exception();
  }

 private:
  static mongo::DBClientConnection db_;
  static boost::mutex db_mutex_;
};

// Initialize proxy manager's static variables.
mongo::DBClientConnection proxy_manager::db_;
boost::mutex proxy_manager::db_mutex_;

}  // namespace botscript

#endif  // PROXY_MANAGER_H_
