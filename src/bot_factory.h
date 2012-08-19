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

#include "bot.h"

namespace botscript {

/// Factory containing a asio io_service fueled by threads to create bots.
class bot_factory {
 public:
  /// Constructor.
  bot_factory() : work_(io_service_) {
  }

  /// Destructor - stops the service and joins all threads.
  ~bot_factory() {
    io_service_.stop();
    worker_threads_.join_all();
  }

  /// Starts the service.
  /**
   * \param thread_count the count of threads to start
   */
  void init(size_t thread_count) {
    for (unsigned int i = 0; i < thread_count; ++i) {
      worker_threads_.create_thread(
         boost::bind(&boost::asio::io_service::run, &io_service_));
    }
  }

  /**
   * Creats a bot with the given configuration.
   *
   * \param configuration JSON configuration string
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   * \return the bot wrapped by a shared pointer
   */
  boost::shared_ptr<bot> create_bot(const std::string& configuration)
  throw(lua_exception, bad_login_exception, invalid_proxy_exception) {
    boost::shared_ptr<bot> bot = boost::make_shared<bot>(&io_service_);
    bot->loadConfiguration(configuration);
    return bot;
  }

 private:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::thread_group worker_threads_;
};

}  // namespace botscript

#endif  // BOT_FACTORY_H_
