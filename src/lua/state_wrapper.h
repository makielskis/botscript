// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#ifndef LUA_STATE_WRAPPER_H_
#define LUA_STATE_WRAPPER_H_

#include <execinfo.h>

#include <iostream>
#include <vector>
#include <string>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

namespace botscript {

/// RAII style lua_State* wrapper.
class state_wrapper {
 public:
  /// Creates a new lua_State using luaL_newstate(). If luaL_newstate() returns
  /// a nullptr, this state will also manage a nullptr. So to check a successful
  /// initialization get() has to be called and checkted to be not nullptr.
  state_wrapper()
      : state_(luaL_newstate()),
        closed_(false) {
  }

  /// Takes control of the provided state. Warning: Don't call lua_close on the
  /// provided state. This will be done by the destructor.
  state_wrapper(lua_State* state)
      : state_(state),
        closed_(false) {
  }

  /// Destructor. Calls lua_close() on the state.
  virtual ~state_wrapper() {
    if (!closed_) for (auto s : stack_) { std::cerr << s << "\n"; }
    assert(closed_);
    if (state_ != nullptr) {
      lua_close(state_);
    }
  }

  void close() {
    if (closed_) for (auto s : stack_) { std::cerr << s << "\n"; }
    assert(!closed_);
    closed_ = true;

    size_t sz;
    void *bt[20];
    char **strings;

    sz = backtrace(bt, 20);
    strings = backtrace_symbols(bt, sz);

    for(unsigned i = 0; i < sz; ++i)
      stack_.push_back(strings[i]);
  }

  /// Returns the lua_State pointer.
  lua_State* get() const { return state_; }

 private:
  /// The managed state.
  lua_State* state_;

  std::vector<std::string> stack_;

  bool closed_;
};

}  // namespace botscript

#endif  // LUA_STATE_WRAPPER_H_
