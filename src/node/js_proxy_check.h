// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include <node.h>

#include <string>
#include <memory>

#include "../http/useragents.h"
#include "../http/util.h"
#include "../http/url.h"
#include "../proxy_check.h"

#include "./util.h"
#include "./threaded_async.h"
#include "./js_bot_motor.h"

namespace botscript {
namespace node_bot {

class proxy_cb {
 public:
  proxy_cb(std::shared_ptr<botscript::proxy_check> proxy_check,
           boost::system::error_code error,
           v8::Persistent<v8::Function> callback)
      : proxy_check_(proxy_check),
        error_(error),
        cb_(callback) {
  }

  std::shared_ptr<botscript::proxy_check> proxy_check() const {
    return proxy_check_;
  }
  boost::system::error_code error()             const { return error_; }
  v8::Persistent<v8::Function> callback()       const { return cb_; }

 private:
  std::shared_ptr<botscript::proxy_check> proxy_check_;
  boost::system::error_code error_;
  v8::Persistent<v8::Function> cb_;
};

/// Class providing functionality to asynchronously call the proxy callback.
class async_proxy_callback : public threaded_async_action<proxy_cb> {
 public:
  void foreground(std::vector<proxy_cb> queue) {
    v8::HandleScope scope;

    // Call each callback with its result.
    for (const proxy_cb& cb : queue) {
      boost::system::error_code ec = cb.error();

      if (ec) {
        // Error occured when testing the proxy:
        // Call callback with error message.
        v8::Local<v8::Value> args[1] = {
          v8::String::New(ec.message().c_str())
        };
        cb.callback()->Call(v8::Undefined().As<v8::Object>(), 1, args);
      } else {
        // No error; call callback with undefined
        v8::Local<v8::Value> args[1] = {
          v8::Local<v8::Value>::New(v8::Undefined())
        };
        cb.callback()->Call(v8::Undefined().As<v8::Object>(), 1, args);
      }
    }
  }
};

/// NodeJS wrapper class for the botscript::proxy_check class.
class js_proxy_check : public node::ObjectWrap {
 public:
  /// \param check the check this object manages.
  js_proxy_check(std::shared_ptr<botscript::proxy_check> proxy_check)
       : proxy_check_(proxy_check) {
  }

  std::shared_ptr<botscript::proxy_check> proxy_check() {
    return proxy_check_;
  }

  /// Init function registers the bot object and its member functions at Node.js
  static void Init(v8::Handle<v8::Object> target) {
    // Register ProxyCheck symbol.
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
    tpl->SetClassName(v8::String::NewSymbol("ProxyCheck"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Register check member function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("check"),
        v8::FunctionTemplate::New(check)->GetFunction());

    // Register ProxyCheck constructor.
    v8::Persistent<v8::Function> constructor =
        v8::Persistent<v8::Function>::New(tpl->GetFunction());
    target->Set(v8::String::NewSymbol("ProxyCheck"), constructor);
  }

 private:
  static v8::Handle<v8::Value> New(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check argument count.
    if (args.Length() != 4) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong number of arguments (exactly 4 requirered)")));
      return scope.Close(v8::Undefined());
    }

    // Check aruguments types:
    // [0] = [Object] : Bot Motor
    // [1] = [String] : IP string
    // [2] = [String] : port string
    // [3] = [String] : check URL
    if (!args[0]->IsObject() ||
        !args[1]->IsString() ||
        !args[2]->IsString() ||
        !args[3]->IsString()) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Extract io_service.
    v8::Local<v8::Object> motor_holder = v8::Local<v8::Object>::Cast(args[0]);
    js_bot_motor* motor_ptr =
        node::ObjectWrap::Unwrap<js_bot_motor>(motor_holder);
    boost::asio::io_service* io_service_ptr = motor_ptr->io_service();

    // Extract arguments.
    std::string ip = v8String2stdString(args[1]);
    std::string port = v8String2stdString(args[2]);
    std::string check_url = v8String2stdString(args[3]);

    // Create HTTP GET request for check URL.
    using http::util::build_request;
    std::string check_request = build_request(http::url(check_url),
                                              http::util::GET, "",
                                              http::useragents::random_ua(),
                                              true);

    // Create proxy check.
    botscript::proxy proxy(ip, port);
    auto proxy_check_ptr = std::make_shared<botscript::proxy_check>(
        io_service_ptr, proxy, check_request);
    js_proxy_check* js_proxy_check_ptr = new js_proxy_check(proxy_check_ptr);

    // Return wrapped proxy check.
    js_proxy_check_ptr->Wrap(args.This());

    return args.This();
  }

  static v8::Handle<v8::Value> check(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments.
    if (!args[0]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("Wrong arguments")));
    }

    // Unwrap proxy_check and start check asynchronously.
    v8::Persistent<v8::Function> callback = v8::Persistent<v8::Function>::New(
                                                args[0].As<v8::Function>());
    auto check = node::ObjectWrap::Unwrap<js_proxy_check>(args.This());
    check->proxy_check()->check(
        [callback](std::shared_ptr<botscript::proxy_check> proxy_check,
                   boost::system::error_code ec) {
      proxy_cb_.push(proxy_cb(proxy_check, ec, callback));
    });

    return scope.Close(v8::Undefined());
  }

 private:
  /// Points to the managed proxy check object.
  std::shared_ptr<botscript::proxy_check> proxy_check_;

  /// Callback
  static async_proxy_callback proxy_cb_;
};

async_proxy_callback js_proxy_check::proxy_cb_;

}  // namespace node_bot
}  // namespace botscript
