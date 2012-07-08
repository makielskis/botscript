/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 7. July 2012  makielskis@gmail.com
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

#ifndef ASYNC_H_
#define ASYNC_H_

#include "node.h"

namespace botscript {
namespace node_bot {

/**
 * Asynchronous action using libuv.
 * Synchronous work is done in foreground (can access V8).
 * Asynchronous work is done in background (can not access V8).
 */
class async_action {
 public:

  /// Destructor.
  virtual ~async_action() {}

  /// Invokes the given action using the libuv queue.
  static void invoke(async_action* action) {
    action->work_.data = action;
    uv_queue_work(uv_default_loop(), &action->work_, asyncWork, asyncAfter);
  }

  /// Override this with background work.
  virtual void background() = 0;

  /// Override this with foreground work.
  virtual void foreground() = 0;

 private:
  static void asyncWork(uv_work_t* work) {
    async_action* action = (async_action*) work->data;
    action->background();
  }

  static void asyncAfter(uv_work_t* work) {
    async_action* action = (async_action*) work->data;
    action->foreground();
    delete action;
  }

  uv_work_t work_;
};

}  // namespace node_bot
}  // namespace botscript

#endif  // ASYNC_H_
