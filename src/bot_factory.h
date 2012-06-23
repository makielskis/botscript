/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 23. June 2012  makielskis@gmail.com
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

#ifndef BOT_FACTORY_H_
#define BOT_FACTORY_H_

#include "boost/asio/io_service.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread.hpp"
#include "boost/make_shared.hpp"

namespace botscript {

class bot_factory {
 public:
  explicit bot_factory(const size_t thread_count) : work_(io_service_) {
    for (unsigned int i = 0; i < thread_count; ++i) {
      worker_threads_.create_thread(
         boost::bind(&boost::asio::io_service::run, &io_service_));
    }
  }

  ~bot_factory() {
    io_service_.stop();
    worker_threads_.join_all();
  }

  boost::shared_ptr<bot> create_bot(const std::string& username,
                                    const std::string& password,
                                    const std::string& package,
                                    const std::string& server,
                                    const std::string& proxy) {
    return boost::make_shared<bot>(username, password, package, server, proxy,
                                   &io_service_);
  }

  boost::shared_ptr<bot> create_bot(const std::string& configuration) {
    return boost::make_shared<bot>(configuration, &io_service_);
  }

 private:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::thread_group worker_threads_;
};

}  // namespace botscript

#endif  // BOT_FACTORY_H_
