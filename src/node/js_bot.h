// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef JS_BOT_H
#define JS_BOT_H

#include <node.h>

#include <memory>
#include <string>
#include <deque>
#include <sstream>

#include "boost/asio/io_service.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/tuple/tuple.hpp"

#include "../bot.h"
#include "./async.h"
#include "./threaded_async.h"
#include "./js_bot_motor.h"

namespace botscript {
namespace node_bot {

/// Class providing functionality to asynchronously ask for a bot configuration.
class async_get_config : public async_action {
 public:
  /// \param callback  the callback to call with the configuration as parameter
  /// \param bot the   bot to get the configuration for
  async_get_config(v8::Handle<v8::Value> callback,
                   std::shared_ptr<botscript::bot> bot)
    : callback_(v8::Persistent<v8::Function>::New(callback.As<v8::Function>())),
      bot_(bot) {
  }

  void background() {
    configuration_ = bot_->configuration(true);
  }

  void foreground() {
    v8::HandleScope scope;

    v8::Local<v8::Value> args[2] = {
        v8::Local<v8::Value>::New(v8::Undefined()),
        v8::Local<v8::Value>::New(v8::String::New(configuration_.c_str()))
    };
    callback_->Call(v8::Undefined().As<v8::Object>(), 2, args);
  }

 private:
  v8::Persistent<v8::Function> callback_;
  std::shared_ptr<botscript::bot> bot_;
  std::string configuration_;
};

/// Class providing functionality to asynchronously create an identifier
class async_create_identifier : public async_action {
 public:
  /// \param callback  the callback to be called with the created identifier
  /// \param username  the username
  /// \param package   the package
  /// \param server    the server address
  async_create_identifier(v8::Handle<v8::Value> callback,
                          const std::string& username,
                          const std::string& package,
                          const std::string& server)
    : callback_(v8::Persistent<v8::Function>::New(callback.As<v8::Function>())),
      username_(username),
      package_(package),
      server_(server) {
  }

  void background() {
    identifier_ = botscript::bot::identifier(username_, package_, server_);
  }

  void foreground() {
    v8::HandleScope scope;

    v8::Local<v8::Value> args[2] = {
        v8::Local<v8::Value>::New(v8::Undefined()),
        v8::Local<v8::Value>::New(v8::String::New(identifier_.c_str()))
    };
    callback_->Call(v8::Undefined().As<v8::Object>(), 2, args);
  }

 private:
  v8::Persistent<v8::Function> callback_;
  std::string username_;
  std::string package_;
  std::string server_;
  std::string identifier_;
};

/// Class providing functionality to asynchronously load package information.
class async_load_packages : public async_action {
 public:
  /// \param callback  the callback to call
  /// \param path      the path to load the package information from
  async_load_packages(v8::Handle<v8::Value> callback,
                      const std::string& path)
    : callback_(v8::Persistent<v8::Function>::New(callback.As<v8::Function>())),
      path_(path) {
  }

  void background() {
    packages_ = botscript::bot::load_packages(path_);
  }

  void foreground() {
    v8::HandleScope scope;
    // Create and fill array with all packages.
    int index = 0;
    v8::Local<v8::Array> array = v8::Array::New(packages_.size());
    for (const auto& p : packages_) {
      array->Set(index++, v8::String::New(p.c_str()));
    }

    // Call callback function with array as argument.
    v8::Local<v8::Value> args[1] = { array };
    callback_->Call(v8::Undefined().As<v8::Object>(), 1, args);
  }

 private:
  v8::Persistent<v8::Function> callback_;
  std::string path_;
  std::vector<std::string> packages_;
};

/// Update message: identifier, key, value.
typedef boost::tuples::tuple<std::string, std::string, std::string> upd_msg;

/// Class providing functionality to asynchronously use an update callback.
class async_upd_callback : public threaded_async_action<upd_msg> {
 public:
  void callback(v8::Handle<v8::Value> callback) {
    cb_ = v8::Persistent<v8::Function>::New(callback.As<v8::Function>());
  }

  void foreground(std::vector<upd_msg> queue) {
    v8::HandleScope scope;

    // Create and fill array with all update messages.
    int index = 0;
    v8::Local<v8::Array> array = v8::Array::New(queue.size());
    for (const upd_msg& msg : queue) {
      // Create new Javascript object.
      v8::Handle<v8::Object> jsObject = v8::Object::New();

      // Set id, key and value of the object.
      jsObject->Set(v8::String::New("id"),
                    v8::String::New(msg.get<0>().c_str()));
      jsObject->Set(v8::String::New("key"),
                    v8::String::New(msg.get<1>().c_str()));
      jsObject->Set(v8::String::New("value"),
                    v8::String::New(msg.get<2>().c_str()));

      // Store message object to the array.
      array->Set(index++, jsObject);
    }

    // Call callback function with array as argument.
    v8::Local<v8::Value> args[1] = { array };
    cb_->Call(v8::Undefined().As<v8::Object>(), 1, args);
  }

 private:
  v8::Persistent<v8::Function> cb_;
};

class load_cb {
 public:
  load_cb(std::shared_ptr<botscript::bot> bot, std::string error,
          v8::Persistent<v8::Function> callback)
      : bot_(bot),
        error_(error),
        cb_(callback) {
  }

  std::shared_ptr<botscript::bot> bot()   const { return bot_; }
  std::string error()                     const { return error_; }
  v8::Persistent<v8::Function> callback() const { return cb_; }

 private:
  std::shared_ptr<botscript::bot> bot_;
  std::string error_;
  v8::Persistent<v8::Function> cb_;
};

/// Class providing functionality to asynchronously call the load callback.
class async_load_callback : public threaded_async_action<load_cb> {
 public:
  void foreground(std::vector<load_cb> queue) {
    v8::HandleScope scope;

    // Call each callback with its result.
    for (const load_cb& cb : queue) {
      v8::Local<v8::Value> args[3] = {
          v8::String::New(cb.error().c_str()),
          v8::Local<v8::Value>::New(v8::Boolean::New(cb.error().empty())),
          v8::String::New(cb.bot()->configuration(true).c_str())
      };
      cb.callback()->Call(v8::Undefined().As<v8::Object>(), 3, args);
    }
  }
};

/// js_bot wraps the botscript::bot class for Node.js
class js_bot : public node::ObjectWrap {
 public:
  /// Creates a new wrapped bot.
  ///
  /// \param bot the bot to wrap
  explicit js_bot(std::shared_ptr<botscript::bot> bot) : bot_(bot) {
  }

  /// Destructor (currently for debugging purposes only).
  virtual ~js_bot() {
    std::stringstream msg;
    msg << "js_bot::~js_bot: (" << bot_->identifier()
        << ": " << bot_.use_count() << ")";
    bot_->log(botscript::bot::BS_LOG_DBG, "base", msg.str());
  }

  /// Init function registers the bot object and its member functions at Node.js
  static void Init(v8::Handle<v8::Object> target) {
    // Register isolated createIdentifier() function.
    target->Set(v8::String::NewSymbol("createIdentifier"),
        v8::FunctionTemplate::New(createIdentifier)->GetFunction());

    // Register isolated loadPackages() function.
    target->Set(v8::String::NewSymbol("loadPackages"),
        v8::FunctionTemplate::New(loadPackages)->GetFunction());

    // Register isolated setUpdateCallback() getter function.
    target->Set(v8::String::NewSymbol("setUpdateCallback"),
        v8::FunctionTemplate::New(setUpdateCallback)->GetFunction());

    // Register Bot symbol.
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
    tpl->SetClassName(v8::String::NewSymbol("Bot"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Register loadConfiguration member function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("load"),
        v8::FunctionTemplate::New(bot_load)->GetFunction());

    // Register shutdown member function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("shutdown"),
        v8::FunctionTemplate::New(bot_shutdown)->GetFunction());

    // Register execute member function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("execute"),
        v8::FunctionTemplate::New(bot_execute)->GetFunction());

    // Register identifier getter.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("identifier"),
        v8::FunctionTemplate::New(bot_identifier)->GetFunction());

    // Register configuration getter.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("configuration"),
        v8::FunctionTemplate::New(bot_configuration)->GetFunction());

    // Register log_msgs getter function.
    tpl->InstanceTemplate()->Set(v8::String::NewSymbol("log"),
        v8::FunctionTemplate::New(bot_log)->GetFunction());

    // Register Bot constructor.
    v8::Persistent<v8::Function> constructor =
        v8::Persistent<v8::Function>::New(tpl->GetFunction());
    target->Set(v8::String::NewSymbol("Bot"), constructor);
  }

 private:
  static v8::Handle<v8::Value> createIdentifier(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments types:
    // [0] = username (String)
    // [1] = package (String)
    // [2] = server (String)
    // [3] = function callback (Function)
    if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString() ||
        !args[3]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Create identifier asynchronously.
    async_create_identifier* create_identifier =
        new async_create_identifier(args[3],
                                    v8String2stdString(args[0]),
                                    v8String2stdString(args[1]),
                                    v8String2stdString(args[2]));
    async_action::invoke(create_identifier);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> loadPackages(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments types:
    // [0] = path (String)
    // [1] = function callback (Function)
    if (!args[0]->IsString() || !args[1]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Create identifier asynchronously.
    async_load_packages* load_packages =
        new async_load_packages(args[1], v8String2stdString(args[0]));
    async_action::invoke(load_packages);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> setUpdateCallback(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check argument count.
    if (args.Length() != 1) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong number of arguments")));
      return scope.Close(v8::Undefined());
    }

    // Check aruguments types:
    // [0] = callback (Function)
    if (!args[0]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Set update callback.
    callback_.callback(args[0]);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> New(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check argument count.
    if (args.Length() != 1) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong number of arguments")));
      return scope.Close(v8::Undefined());
    }

    // Check aruguments types:
    // [0] = io_service (Object)
    if (!args[0]->IsObject()) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong arguments")));
      return scope.Close(v8::Undefined());
    }

    // Extract io_service.
    v8::Local<v8::Object> motor_holder = v8::Local<v8::Object>::Cast(args[0]);
    js_bot_motor* motor_ptr =
        node::ObjectWrap::Unwrap<js_bot_motor>(motor_holder);
    boost::asio::io_service* io_service_ptr = motor_ptr->io_service();

    // Create bot.
    std::shared_ptr<botscript::bot> bot_ptr =
        std::make_shared<botscript::bot>(io_service_ptr);
    js_bot* js_bot_ptr = new js_bot(bot_ptr);

    // Extract logging callback.
    bot_ptr->callback_ = boost::bind(&js_bot::call_callback, js_bot_ptr,
                                     _1, _2, _3);

    // Return wrapped bot.
    js_bot_ptr->Wrap(args.This());
    return args.This();
  }

  static v8::Handle<v8::Value> bot_execute(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments.
    if (!args[0]->IsString() || !args[1]->IsString()) {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("Wrong arguments")));
    }

    // Get command and argument.
    std::string command = v8String2stdString(args[0]);
    std::string argument = v8String2stdString(args[1]);

    // Unwrap bot and execute command.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());
    jsbot_ptr->bot()->execute(command, argument);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> bot_load(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments.
    if (!args[0]->IsString() || !args[1]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("Wrong arguments")));
    }

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Execute command asynchronously.
    v8::Persistent<v8::Function> callback = v8::Persistent<v8::Function>::New(args[1].As<v8::Function>());
    jsbot_ptr->bot()->init(v8String2stdString(args[0]),
                           [&load_cb_, callback](std::shared_ptr<botscript::bot> bot,
                                                 const std::string& error) {
                             load_cb_.push(load_cb(bot, error, callback));
                           });

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> bot_shutdown(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Unwrap bot and call shutdown.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());
    jsbot_ptr->bot()->shutdown();

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> bot_identifier(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Return identifier.
    return scope.Close(v8::String::New(jsbot_ptr->bot()->identifier().c_str()));
  }

  static v8::Handle<v8::Value> bot_configuration(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments.
    if (!args[0]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("Wrong argument")));
    }

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Read configuration asynchronously
    async_get_config* get_config = new async_get_config(args[0],
                                                        jsbot_ptr->bot());
    async_action::invoke(get_config);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> bot_log(const v8::Arguments& args) {
    v8::HandleScope scope;
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());
    return scope.Close(v8::String::New(jsbot_ptr->bot()->log_msgs().c_str()));
  }

  static std::string v8String2stdString(const v8::Local<v8::Value>& input) {
    assert(input->IsString());
    v8::Local<v8::String> v8_string = v8::Local<v8::String>::Cast(input);
    std::string output(v8_string->Utf8Length(), 0);
    v8_string->WriteUtf8(const_cast<char*>(output.c_str()));
    return output;
  }

  botscript::bot* bot_ptr() {
    return bot_.get();
  }

  std::shared_ptr<botscript::bot> bot() {
    return bot_;
  }

  void call_callback(std::string id, std::string k, std::string v) {
    callback_.push(upd_msg(id, k, v));
  }

  std::shared_ptr<botscript::bot> bot_;
  static async_upd_callback callback_;
  static async_load_callback load_cb_;
};

async_upd_callback js_bot::callback_;
async_load_callback js_bot::load_cb_;

}  // namespace node_bot
}  // namespace botscript

#endif  // JS_BOT_H
