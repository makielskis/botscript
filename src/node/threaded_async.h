// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef THREADED_ASYNC_H_
#define THREADED_ASYNC_H_

#include <node.h>

#include <vector>

#include "boost/thread.hpp"

template<class T>
class threaded_async_action {
 public:
  threaded_async_action()
    : loop_(uv_default_loop()) {
    uv_async_init(loop_, &async_, asyncAfter);
    async_.data = this;
  }

  ~threaded_async_action() {
    uv_close((uv_handle_t*) &async_, nullptr);
  }

  void push(T data) {
    {
      boost::lock_guard<boost::mutex> lock(uv_lock_);
      queue_.push_back(data);
    }
    uv_async_send(&async_);
  }

  /// Override this with foreground work.
  virtual void foreground(std::vector<T> work) = 0;

 protected:
  std::vector<T> queue_;

 private:
  static void asyncAfter(uv_async_t* handle, int /* status */) {
    threaded_async_action<T>* action =
        static_cast<threaded_async_action<T>*>(handle->data);

    std::vector<T> queue_copy;
    {
      boost::lock_guard<boost::mutex> lock(action->uv_lock_);
      queue_copy = action->queue_;
      action->queue_.clear();
    }
    action->foreground(queue_copy);
  }

  uv_loop_t* loop_;
  uv_async_t async_;
  boost::mutex uv_lock_;
};

#endif  // THREADED_ASYNC_H_
