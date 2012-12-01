#include <string>
#include <iostream>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "boost/function.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/bind.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/asio/basic_deadline_timer.hpp"

#include "./http/webclient.h"

http::webclient* webclient;
boost::asio::io_service* io_service;
boost::asio::deadline_timer* timer;

static int http_request(lua_State*);
static void http_response(lua_State*, std::string, bool);
static void run_script(lua_State*);
static void start_timer(int, lua_State*);

class async_execute : boost::shared_from_this {
 public:
  typedef boost::function<void (std::string, bool)> callback;

  async_execute(callback cb)
    : cb_(cb) {
  }

  virtual ~async_execute() {
    lua_close(state);
    std::cout << "~async_execute()\n";
  }

  void init() throw(std::exception) {
    lua_State* state = luaL_newstate();
    if (nullptr == state) {
      throw std::exception();
    }

    luaL_openlibs(state);
    openhttp(state);
    lua_register(state, "start_timer", start_timer);

    int ret;
    if (0 != (ret = luaL_dofile(state, script))) {
      std::string error;

      const char* s = lua_tostring(state, -1);

      if (s == NULL) {
        switch (ret) {
          case LUA_ERRRUN: error = "runtime error"; break;
          case LUA_ERRMEM: error = "out of memory"; break;
          case LUA_ERRERR: error = "error handling error"; break;
        }
      } else {
        error = s;
      }

      throw std::exception();
    }
  }

  operator()() {
    int ret;
    if (0 != (ret = lua_pcall(state, 2, LUA_MULTRET, 0))) {
      std::string error;

      const char* s = lua_tostring(state, -1);

      if (s == NULL) {
        switch (ret) {
          case LUA_ERRRUN: error = "runtime error"; break;
          case LUA_ERRMEM: error = "out of memory"; break;
          case LUA_ERRERR: error = "error handling error"; break;
        }
      } else {
        error = s;
      }

      cb_(error, true);
    }
  }

  void finish() {
    cb_("", false);
  }

  void error(const std::string& error) {
    cb_(error, true);
  }

 private:
  callback cb_;
};

static const luaL_Reg httplib[] = {
  {"request", http_request},
  {NULL, NULL}
};

static int http_request(lua_State* state) {
  assert(lua_gettop(state) == 2);

  assert(lua_isstring(state, 1));
  assert(lua_isfunction(state, 2));

  std::string url = lua_tostring(state, 1);

  lua_remove(state, 1);
  lua_setglobal(state, "callback");

  webclient->async_get(url, boost::bind(http_response, state, _1, _2), io_service);

  return 0;
}

static void http_response(lua_State* state, std::string response, bool success) {
  if (!success) {
    std::cout << "error: " << response << "\n";
    start_timer(10, state);
    return;
  }

  lua_getglobal(state, "callback");
  assert(lua_isfunction(state, -1));

  lua_pushstring(state, response.c_str());
  lua_pushboolean(state, success);

  int ret;
  if (0 != (ret = lua_pcall(state, 2, LUA_MULTRET, 0))) {
    std::string error;

    const char* s = lua_tostring(state, -1);

    if (s == NULL) {
      switch (ret) {
        case LUA_ERRRUN: error = "runtime error"; break;
        case LUA_ERRMEM: error = "out of memory"; break;
        case LUA_ERRERR: error = "error handling error"; break;
      }
    } else {
      error = s;
    }

    std::cout << "http_response error: " << error << "\n";
  }
}

static int start_timer(lua_State* state) {
  int top = lua_gettop(state);
  assert(top == 2 || top == 1);
  assert(lua_isnumber(state, -1) && (top == 1 || lua_isnumber(state, -2)));

  int n1 = luaL_checkint(state, -1);

  int sleep = n1;

  start_timer(sleep, state);

  return 0;
}

static void start_timer(int sec, lua_State* state) {
  std::cout << "restarting in t = " << sec << "\n";
  timer->expires_from_now(boost::posix_time::seconds(sec));
  timer->async_wait(boost::bind(run_script, state));
}

static void openhttp(lua_State* state) {
  luaL_newlib(state, httplib);
  lua_setglobal(state, "http");
}

static void run_script(lua_State* state) {
  lua_getglobal(state, "test");
  int ret;
  if (0 != (ret = lua_pcall(state, 0, 0, 0))) {
    std::string error;

    const char* s = lua_tostring(state, -1);

    if (s == NULL) {
      switch (ret) {
        case LUA_ERRRUN: error = "runtime error"; break;
        case LUA_ERRMEM: error = "out of memory"; break;
        case LUA_ERRERR: error = "error handling error"; break;
      }
    } else {
      error = s;
    }

    std::cout << "run_script error: " << error << "\n";
  }
  std::cout << "run_script finished\n";
}

int main() {
  webclient = new http::webclient;
  io_service = new boost::asio::io_service;
  timer = new boost::asio::deadline_timer(*io_service);

  lua_State* state = nullptr;
  if ((state = init("test.lua")) == nullptr) {
    std::cerr << "failed to init lua state\n";
    return 1;
  }

  boost::asio::io_service::work work(*io_service);
  io_service->post(boost::bind(run_script, state));
  io_service->run();

  return 0;
}
