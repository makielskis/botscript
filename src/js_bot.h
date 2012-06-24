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

#ifndef JS_BOT_H
#define JS_BOT_H

#include <string>

#include "boost/asio/io_service.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/make_shared.hpp"
#include "boost/lambda/lambda.hpp"
#include "node.h"

#include "./bot.h"
#include "./js_bot_motor.h"

/// js_bot wraps the botscript::bot class for Node.js
class js_bot : public node::ObjectWrap {
 public:
  /**
   * Creates a new wrapped bot.
   *
   * \param bot the bot to wrap
   */
  explicit js_bot(boost::shared_ptr<botscript::bot> bot) : bot_(bot) {
  }

  /// Init function registers the bot object and its member functions at Node.js
  static void Init(v8::Handle<v8::Object> target) {
    // Register Bot symbol.
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
    tpl->SetClassName(v8::String::NewSymbol("Bot"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Register execute member function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("execute"),
        v8::FunctionTemplate::New(bot_execute)->GetFunction());

    // Register setCallback member function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("setCallback"),
        v8::FunctionTemplate::New(bot_set_callback)->GetFunction());

    // Register Bot constructor.
    v8::Persistent<v8::Function> constructor =
        v8::Persistent<v8::Function>::New(tpl->GetFunction());
    target->Set(v8::String::NewSymbol("Bot"), constructor);
  }

 private:
  struct baton {
    std::string id;
    std::string k;
    std::string v;
    v8::Persistent<v8::Function> callback;
    uv_work_t request;
  };

  static v8::Handle<v8::Value> New(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Force either 2 or 6 arguments.
    if (!(args.Length() == 2 || args.Length() == 6)) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong number of arguments")));
      return scope.Close(v8::Undefined());
    }

    // Check aruguments: first=io_service(Object), second=configuration(String).
    if (args.Length() == 2 && !(args[0]->IsObject() && args[1]->IsString())) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Check aruguments: first=io_service(Object), all other=Strings.
    if (args.Length() == 6 &&
        !(args[0]->IsObject() && args[1]->IsString() && args[2]->IsString() &&
          args[3]->IsString() && args[4]->IsString() && args[5]->IsString())) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Extract io_service.
    v8::Local<v8::Object> motor_holder = v8::Local<v8::Object>::Cast(args[0]);
    js_bot_motor* motor_ptr =
        node::ObjectWrap::Unwrap<js_bot_motor>(motor_holder);
    boost::asio::io_service* io_service_ptr = motor_ptr->io_service();

    boost::shared_ptr<botscript::bot> bot_ptr;
    try {
      // Create bot from configuration.
      if (args.Length() == 2) {
        // Extract configuration and create bot with configuration.
        std::string configuration = v8String2stdString(args[1]);
        bot_ptr = boost::make_shared<botscript::bot>(configuration,
                                                     io_service_ptr);
      }

      // Create bot from login information.
      if (args.Length() == 6) {
        // Extract arguments.
        std::string username = v8String2stdString(args[1]);
        std::string password = v8String2stdString(args[2]);
        std::string package = v8String2stdString(args[3]);
        std::string server = v8String2stdString(args[4]);
        std::string proxy = v8String2stdString(args[5]);

        // Create bot with login information.
        bot_ptr = boost::make_shared<botscript::bot>(username, password,
                                                     package, server, proxy,
                                                     io_service_ptr);
      }
    } catch(const botscript::lua_exception& e) {
      v8::ThrowException(v8::Exception::Error(v8::String::New(e.what())));
    } catch(const botscript::bad_login_exception& e) {
      v8::ThrowException(v8::Exception::Error(v8::String::New("Bad Login")));
    } catch(const botscript::invalid_proxy_exception& e) {
      v8::ThrowException(v8::Exception::Error(v8::String::New("Proxy Error")));
    }

    // Make Javascript Object from bot.
    js_bot* jsbot_ptr = new js_bot(bot_ptr);
    jsbot_ptr->Wrap(args.This());

    return args.This();
  }

  static v8::Handle<v8::Value> bot_execute(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Get aruguments.
    std::string command;
    std::string argument;
    if (args[0]->IsString() && args[1]->IsString()) {
      command = v8String2stdString(args[0]);
      argument = v8String2stdString(args[1]);
    } else {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("Wrong arguments")));
    }

    // Execute command.
    jsbot_ptr->bot()->execute(command, argument);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> bot_set_callback(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Get function.
    if (!args[0]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("No function")));
    }

    v8::Local<v8::Function> cb_local = v8::Local<v8::Function>::Cast(args[0]);
    jsbot_ptr->set_callback(v8::Persistent<v8::Function>::New(cb_local));
    jsbot_ptr->bot()->callback_ =
        boost::bind(&js_bot::call_callback, jsbot_ptr, _1, _2, _3);

    return scope.Close(v8::Undefined());
  }

  static void do_nothing(uv_work_t* req) {
  }

  static void after_callback(uv_work_t* req) {
    v8::HandleScope scope;

    // Prepare the parameters for the callback function.
    baton* data = static_cast<baton*>(req->data);
    const unsigned argc = 3;
    v8::Handle<v8::Value> argv[argc];
    argv[0] = v8::String::New(data->id.c_str());
    argv[1] = v8::String::New(data->k.c_str());
    argv[2] = v8::String::New(data->v.c_str());

    v8::TryCatch try_catch;
    data->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
    if (try_catch.HasCaught()) {
      node::FatalException(try_catch);
    }
  }

  static std::string v8String2stdString(const v8::Local<v8::Value>& input) {
    assert(input->IsString());
    v8::Local<v8::String> v8_string = v8::Local<v8::String>::Cast(input);
    std::string output(v8_string->Length(), 0);
    v8_string->WriteAscii(const_cast<char*>(output.c_str()));
    return output;
  }

  botscript::bot* bot() {
    return bot_.get();
  }

  void set_callback(v8::Persistent<v8::Function> cb) {
    callback_ = cb;
  }

  void call_callback(std::string id, std::string k, std::string v) {
    baton* req = new baton();
    req->callback = callback_;
    req->id = id;
    req->k = k;
    req->v = v;
    req->request.data = req;
    uv_queue_work(uv_default_loop(), &req->request, do_nothing, after_callback);
  }

  boost::shared_ptr<botscript::bot> bot_;
  v8::Persistent<v8::Function> callback_;
};

#endif  // JS_BOT_H
