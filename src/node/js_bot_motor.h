// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef JS_BOT_MOTOR_H
#define JS_BOT_MOTOR_H

#include <node.h>

#include <string>

#include "boost/asio/io_service.hpp"
#include "boost/thread.hpp"

namespace botscript {
namespace node_bot {

/// Motor class containing a boost::asio::io_service fueled by a thread group.
class js_bot_motor : public node::ObjectWrap {
 public:
  /// C++ constructor - creates the Motor.
  js_bot_motor() : work_(io_service_) {
  }

  /// Destructor - stops the io_service and the threads.
  ~js_bot_motor() {
    io_service_.stop();
    worker_threads_.join_all();
  }

  /**
   * Registers the BotMotor in Node.js
   */
  static void Init(v8::Handle<v8::Object> target) {
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
    tpl->SetClassName(v8::String::NewSymbol("BotMotor"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    v8::Persistent<v8::Function> constructor =
        v8::Persistent<v8::Function>::New(tpl->GetFunction());
    target->Set(v8::String::NewSymbol("BotMotor"), constructor);
  }

  /**
   * \return the boost::asio::io_service
   */
  boost::asio::io_service* io_service() {
    return &io_service_;
  }

 private:
  static v8::Handle<v8::Value> New(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Let's extract some arguments.
    assert(args[0]->IsNumber());
    double thread_count = args[0]->NumberValue();

    js_bot_motor* motor = new js_bot_motor();
    motor->init_motor(static_cast<size_t>(thread_count));
    motor->Wrap(args.Holder());

    return args.Holder();
  }

  void init_motor(size_t thread_count) {
    for (unsigned int i = 0; i < thread_count; ++i) {
      worker_threads_.create_thread(
         boost::bind(&boost::asio::io_service::run, &io_service_));
    }
  }

  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::thread_group worker_threads_;
};

}  // namespace node_bot
}  // namespace botscript

#endif  // JS_BOT_MOTOR_H
