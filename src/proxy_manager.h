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

#include <string>
#include <vector>

#include "boost/foreach.hpp"

#include "client/dbclient.h"

#include "./bot.h"
#include "./exceptions/invalid_proxy_exception.h"

namespace botscript {

class proxy_manager {
 public:
  static std::vector<std::string> get_proxies() throw(invalid_proxy_exception) {
    // Create the result set.
    std::vector<std::string> proxies(20);

    // Create the result cursor ptr.
    mongo::DBClientConnection db;
    db.connect("localhost");
    std::auto_ptr<mongo::DBClientCursor> cursor =
        db.query("test.proxies", QUERY("inuse" << false << "good" << true));

    // Iterate results.
    int count = 0;
    while(cursor->more()) {
      mongo::BSONObj p = cursor->next();
      proxies[count] = p.getStringField("address");

      count++;
      if (count == 20) {
        break;
      }
    }

    // Throw exception if there were no proxies found.
    if (count == 0) {
      throw invalid_proxy_exception();
    }

    return proxies;
  }

  static void return_proxy(const std::string& proxy) {
/*
    boost::lock_guard<boost::mutex> lock(dbmutex_);
    db.update("test.proxies", BSON("address" << proxy),
               BSON("$set" << BSON("inuse" << false)));
*/
  }
};

}  // namespace botscript

#endif  // PROXY_MANAGER_H_
