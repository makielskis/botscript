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
