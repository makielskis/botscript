/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 24. June 2012  makielskis@gmail.com
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

#ifndef JS_BOT_MOTOR_H
#define JS_BOT_MOTOR_H

#include <string>

#include "boost/asio/io_service.hpp"
#include "boost/thread.hpp"
#include "node.h"

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

#endif  // JS_BOT_MOTOR_H
