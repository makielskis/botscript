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

#include <node.h>

#include <string>
#include <deque>

#include "boost/asio/io_service.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/make_shared.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/tuple/tuple.hpp"

#include "../bot.h"
#include "./async.h"
#include "./js_bot_motor.h"

namespace botscript {
namespace node_bot {

/// Class providing functionality to asynchronously load bot configurations
class async_load : public async_action {
 public:
  /// Constructor.
  /// \param callback the callback to call, when the load operation has finsihed
  /// \param bot the bot to load the configuration to
  /// \param configuration the configuration to load
  async_load(v8::Handle<v8::Value> callback,
             boost::shared_ptr<botscript::bot> bot,
             const std::string& configuration)
    : callback_(v8::Persistent<v8::Function>::New(callback.As<v8::Function>())),
      bot_(bot),
      configuration_(configuration),
      success_(false) {
  }

  void background() {
    try {
      bot_->loadConfiguration(configuration_);
      success_ = true;
    } catch(const botscript::lua_exception& e) {
      error_ = e.what();
    } catch(const botscript::bad_login_exception& e) {
      error_ = "Bad Login";
    } catch(const botscript::invalid_proxy_exception& e) {
      error_ = "Proxy Error";
    }
  }

  void foreground() {
    v8::HandleScope scope;

    if (!success_) {
      v8::Local<v8::Value> args[3] = {
          v8::String::New(error_.c_str()),
          v8::Local<v8::Value>::New(v8::Boolean::New(false)),
          v8::String::New(configuration_.c_str())
      };
      callback_->Call(v8::Undefined().As<v8::Object>(), 3, args);
    } else {
      v8::Local<v8::Value> args[3] = {
          v8::Local<v8::Value>::New(v8::Undefined()),
          v8::Local<v8::Value>::New(v8::Boolean::New(true)),
          v8::String::New(configuration_.c_str())
      };
      callback_->Call(v8::Undefined().As<v8::Object>(), 3, args);
    }
  }

 private:
  v8::Persistent<v8::Function> callback_;
  boost::shared_ptr<botscript::bot> bot_;
  std::string configuration_;
  bool success_;
  std::string error_;
};

/// Class providing functionality to asynchronously shutdown a bot
class async_shutdown : public async_action {
 public:
  /// Constructor.
  /// \param bot the bot to shutdown
  /// \param callback callback function to call when the shutdown has finished
  async_shutdown(boost::shared_ptr<botscript::bot> bot,
                 v8::Handle<v8::Value> callback)
    : callback_(v8::Persistent<v8::Function>::New(callback.As<v8::Function>())),
      bot_(bot) {
  }

  void background() {
    bot_->shutdown();
  }

  void foreground() {
    v8::HandleScope scope;
    v8::Local<v8::Value>* args = nullptr;
    callback_->Call(v8::Undefined().As<v8::Object>(), 0, args);
  }

 private:
  v8::Persistent<v8::Function> callback_;
  boost::shared_ptr<botscript::bot> bot_;
};

/// Class providing functionality to asynchronously execute bot commands
class async_execute : public async_action {
 public:
  /// \param bot the bot to execute the command on
  /// \param command the command to execute
  /// \param argument the argument to pass
  async_execute(boost::shared_ptr<botscript::bot> bot,
                const std::string& command, const std::string& argument)
    : bot_(bot),
      command_(command),
      argument_(argument) {
  }

  void background() {
    bot_->execute(command_, argument_);
  }

  void foreground() {
  }

 private:
  boost::shared_ptr<botscript::bot> bot_;
  std::string command_;
  std::string argument_;
};

/// Class providing functionality to asynchronously ask for a bot configuration
class async_get_config : public async_action {
 public:
  /// \param callback the callback to call with the configuration as parameter
  /// \param bot the bot to get the configuration for
  async_get_config(v8::Handle<v8::Value> callback,
                   boost::shared_ptr<botscript::bot> bot)
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
  boost::shared_ptr<botscript::bot> bot_;
  std::string configuration_;
};

/// Class providing functionality to asynchronously create an identifier
class async_create_identifier : public async_action {
 public:
  /// \param callback the callback to be called with the created identifier
  /// \param username the username
  /// \param package the package
  /// \param server the server address
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
    identifier_ = botscript::bot::createIdentifier(username_, package_,
                                                   server_);
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
  /// \param callback the callback to call
  /// \param path the path to load the package information from
  async_load_packages(v8::Handle<v8::Value> callback,
                      const std::string& path)
    : callback_(v8::Persistent<v8::Function>::New(callback.As<v8::Function>())),
      path_(path) {
  }

  void background() {
    packages_ = botscript::bot::loadPackages(path_);
  }

  void foreground() {
    v8::HandleScope scope;

    v8::Local<v8::Value> args[2] = {
        v8::Local<v8::Value>::New(v8::Undefined()),
        v8::Local<v8::Value>::New(v8::String::New(packages_.c_str()))
    };
    callback_->Call(v8::Undefined().As<v8::Object>(), 2, args);
  }

 private:
  v8::Persistent<v8::Function> callback_;
  std::string path_;
  std::string packages_;
};

/// js_bot wraps the botscript::bot class for Node.js
class js_bot : public node::ObjectWrap {
 public:
  /// Update message: identifier, key, value
  typedef boost::tuples::tuple<std::string, std::string, std::string> upd_msg;

  /**
   * Creates a new wrapped bot.
   *
   * \param bot the bot to wrap
   */
  explicit js_bot(boost::shared_ptr<botscript::bot> bot) : bot_(bot) {
  }

  /// Destructor (currently for debugging purposes only).
  ~js_bot() {
    std::cerr << "deleting " << bot_->identifier() << "\n";
  }

  /// Init function registers the bot object and its member functions at Node.js
  static void Init(v8::Handle<v8::Object> target) {
    // Register isolated createIdentifier() function.
    target->Set(v8::String::NewSymbol("createIdentifier"),
        v8::FunctionTemplate::New(createIdentifier)->GetFunction());

    // Register isolated loadPackages() function.
    target->Set(v8::String::NewSymbol("loadPackages"),
        v8::FunctionTemplate::New(loadPackages)->GetFunction());

    // Register isolated updates() getter function.
    target->Set(v8::String::NewSymbol("updates"),
        v8::FunctionTemplate::New(bot_get_updates)->GetFunction());

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

  static v8::Handle<v8::Value> New(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check argument count.
    if (args.Length() != 2) {
      v8::ThrowException(v8::Exception::TypeError(
                             v8::String::New("Wrong number of arguments")));
      return scope.Close(v8::Undefined());
    }

    // Check aruguments types:
    // [0] = io_service (Object)
    // [1] = logging/update callback (Function)
    if (!args[0]->IsObject() || !args[1]->IsFunction()) {
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
    boost::shared_ptr<botscript::bot> bot_ptr =
        boost::make_shared<botscript::bot>(io_service_ptr);
    js_bot* js_bot_ptr = new js_bot(bot_ptr);

    // Extract and logging callback.
    v8::Local<v8::Function> cb_local = v8::Local<v8::Function>::Cast(args[1]);
    js_bot_ptr->callback_ = v8::Persistent<v8::Function>::New(cb_local);
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

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Execute command.
    async_execute* execute = new async_execute(jsbot_ptr->bot(),
                                               command, argument);
    async_action::invoke(execute);

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
    async_load* load = new async_load(args[1], jsbot_ptr->bot(),
                                      v8String2stdString(args[0]));
    async_action::invoke(load);

    return scope.Close(v8::Undefined());
  }

  static v8::Handle<v8::Value> bot_shutdown(const v8::Arguments& args) {
    v8::HandleScope scope;

    // Check aruguments.
    if (!args[0]->IsFunction()) {
      v8::ThrowException(v8::Exception::TypeError(
                            v8::String::New("Wrong argument")));
    }

    // Unwrap bot.
    js_bot* jsbot_ptr = node::ObjectWrap::Unwrap<js_bot>(args.This());

    // Execute command asynchronously.
    async_shutdown* load = new async_shutdown(jsbot_ptr->bot(), args[0]);
    async_action::invoke(load);

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
    std::string output(v8_string->Length(), 0);
    v8_string->WriteAscii(const_cast<char*>(output.c_str()));
    return output;
  }

  botscript::bot* bot_ptr() {
    return bot_.get();
  }

  boost::shared_ptr<botscript::bot> bot() {
    return bot_;
  }

  void call_callback(std::string id, std::string k, std::string v) {
    boost::lock_guard<boost::mutex> lock(update_queue_mutex_);
    update_queue_.push_back(upd_msg(id, k, v));
  }

  static v8::Handle<v8::Value> bot_get_updates(const v8::Arguments& args) {
    v8::HandleScope scope;

    v8::Handle<v8::Array> array = v8::Array::New(update_queue_.size());
    int index = 0;
    boost::lock_guard<boost::mutex> lock(update_queue_mutex_);
    std::for_each(update_queue_.begin(), update_queue_.end(),
                  [&array, &index](const upd_msg& msg) {
                    v8::Handle<v8::Object> jsObject = v8::Object::New();
                    jsObject->Set(v8::String::New("id"),
                                  v8::String::New(msg.get<0>().c_str()));
                    jsObject->Set(v8::String::New("key"),
                                  v8::String::New(msg.get<1>().c_str()));
                    jsObject->Set(v8::String::New("value"),
                                  v8::String::New(msg.get<2>().c_str()));

                    array->Set(index++, jsObject);
                  });
    update_queue_.clear();

    return scope.Close(array);
  }

  boost::shared_ptr<botscript::bot> bot_;
  v8::Persistent<v8::Function> callback_;
  static std::deque<upd_msg> update_queue_;
  static boost::mutex update_queue_mutex_;
};

std::deque<js_bot::upd_msg> js_bot::update_queue_;
boost::mutex js_bot::update_queue_mutex_;

}  // namespace node_bot
}  // namespace botscript

#endif  // JS_BOT_H
