// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include <node.h>

#include "./js_bot_motor.h"
#include "./js_bot.h"
#include "./js_proxy_check.h"

void InitAll(v8::Handle<v8::Object> target) {
  botscript::node_bot::js_bot_motor::Init(target);
  botscript::node_bot::js_bot::Init(target);
  botscript::node_bot::js_proxy_check::Init(target);
}

NODE_MODULE(addon, InitAll)
