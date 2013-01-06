// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include <node.h>

#include "./js_bot_motor.h"
#include "./js_bot.h"

void InitAll(v8::Handle<v8::Object> target) {
  using botscript::node_bot::js_bot_motor;
  using botscript::node_bot::js_bot;
  js_bot_motor::Init(target);
  js_bot::Init(target);
}

NODE_MODULE(addon, InitAll)
