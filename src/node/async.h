// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef ASYNC_H_
#define ASYNC_H_

#include <node.h>

#include "boost/thread.hpp"

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
    boost::lock_guard<boost::mutex> lock(uv_lock_);

    action->work_.data = action;
    uv_queue_work(uv_default_loop(), &action->work_, asyncWork, asyncAfter);
  }

  /// Override this with background work.
  virtual void background() = 0;

  /// Override this with foreground work.
  virtual void foreground() = 0;

 private:
  static void asyncWork(uv_work_t* work) {
    async_action* action = reinterpret_cast<async_action*>(work->data);
    action->background();
  }

  static void asyncAfter(uv_work_t* work) {
    async_action* action = reinterpret_cast<async_action*>(work->data);
    action->foreground();
    delete action;
  }

  uv_work_t work_;
  static boost::mutex uv_lock_;
};

boost::mutex async_action::uv_lock_;

}  // namespace node_bot
}  // namespace botscript

#endif  // ASYNC_H_
