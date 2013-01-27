// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef BS_NODE_UTIL_H_
#define BS_NODE_UTIL_H_

namespace botscript {
namespace node_bot {

/// Converts a V8 string to a std::string.
/// Warning: Check whether the given value is a V8 string before using this fun!
///
/// \param input the V8 string to convert
/// \return the std::string
std::string v8String2stdString(const v8::Local<v8::Value>& input) {
  assert(input->IsString());
  v8::Local<v8::String> v8_string = v8::Local<v8::String>::Cast(input);
  std::string output(v8_string->Utf8Length(), 0);
  v8_string->WriteUtf8(const_cast<char*>(output.c_str()));
  return output;
}

}  // namespace node_bot
}  // namespace botscript

#endif  // BS_NODE_UTIL_H_
