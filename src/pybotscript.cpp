/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 28. April 2012  makielskis@gmail.com
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

#include <string>

#include "boost/python.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/bind.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/thread.hpp"

#include "./exceptions/bad_login_exception.h"
#include "./exceptions/lua_exception.h"
#include "./exceptions/invalid_proxy_exception.h"
#include "./bot.h"

/// RAII class for releasing and aquiring the global interpreter lock.
class gil_release {
 public:
  gil_release() {
      save_state = PyEval_SaveThread();
  }

  ~gil_release() {
      PyEval_RestoreThread(save_state);
  }

 private:
  PyThreadState* save_state;
};

/// Bot derived from the default bot. Intended to used with boost::python.
class pybot : public botscript::bot {
 public:
  /**
   * Loads a bot (without releasing the global interpreter lock).
   *
   * \param username the login username
   * \param password the login password
   * \param package the lua scrip package
   *                 (contains at least servers.lua and base.lua)
   * \param server the server address to use
   * \param proxy the proxy to use (empty for direct connection).
                  (only the first proxy in a list will be checked!)
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   */
  pybot(const std::string& username, const std::string& password,
        const std::string& package, const std::string& server,
        const std::string& proxy)
  throw(botscript::lua_exception, botscript::bad_login_exception,
        botscript::invalid_proxy_exception)
    : botscript::bot(username, password, package, server, proxy),
      callback_function_(NULL) {
  }

  /**
   * Loads the given bot configuration.
   *
   * \param configuration JSON configuration string
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   */
  explicit pybot(const std::string& configuration)
  throw(botscript::lua_exception, botscript::bad_login_exception,
        botscript::invalid_proxy_exception)
    : bot(configuration),
      callback_function_(NULL) {
  };

  /**
   * Loads a bot (with releasing the global interpreter lock).
   *
   * \param username the login username
   * \param password the login password
   * \param package the lua scrip package
   *                 (contains at least servers.lua and base.lua)
   * \param server the server address to use
   * \param proxy the proxy to use (empty for direct connection).
                  (only the first proxy in a list will be checked!)
   * \exception lua_exception if loading a module or login script failes
   * \exception bad_login_exception if logging in fails
   * \exception invalid_proxy_exception if we could not connect to the proxy
   * \return the loaded bot
   */
  static boost::python::object loadSingleBot(const std::string& username,
                                             const std::string& password,
                                             const std::string& package,
                                             const std::string& server,
                                             const std::string& proxy)
  throw(botscript::lua_exception, botscript::bad_login_exception,
        botscript::invalid_proxy_exception) {
    gil_release nogil;
    std::auto_ptr<pybot> bot(
        new pybot(username, password, package, server, proxy));
    return boost::python::object(bot);
  }

  /**
   * Loads the given bot configurations in parallel.
   *
   * \param configs a identifier - configuration mapping
   * \return the loaded bots
   */
  static boost::python::dict loadBots(boost::python::dict configs) {
    boost::python::dict result;

    // Post load tasks to io_service.
    boost::asio::io_service io_service;
    boost::python::list iterkeys = (boost::python::list) configs.iterkeys();
    for (int i = 0; i < boost::python::len(iterkeys); i++) {
      std::string k = boost::python::extract<std::string>(iterkeys[i]);
      std::string v = boost::python::extract<std::string>(configs[iterkeys[i]]);
      io_service.post(boost::bind(&pybot::loadBot, k, v, &result));
      result[k] = boost::python::object();
    }

    // Pay for a round of threads.
    boost::thread_group threads;
    for (unsigned int i = 0; i < 2; ++i) {
      threads.create_thread(
          boost::bind(&boost::asio::io_service::run, &io_service));
    }

    // Wait for the load tasks to finish.
    io_service.run();
    threads.join_all();

    return result;
  }

  /**
   * Callback function override. Called on changes.
   *
   * \sa botscript::bot::callback()
   * \param id the bot identifier
   * \param k the key that changed
   * \param v the new value
   */
  virtual void callback(std::string id, std::string k, std::string v) {
    boost::lock_guard<boost::mutex> lock(call_mutex_);
    if (callback_function_ != NULL) {
      PyGILState_STATE gstate;
      gstate = PyGILState_Ensure();
      try {
        boost::python::call<void>(callback_function_, id, k, v);
      } catch(const boost::python::error_already_set& e) {
        std::cout << "Python callback raised an exception.\n";
      }
      PyGILState_Release(gstate);
    }
  }

  /**
   * Sets the callback function.
   *
   * \param callback_function the callback function to set
   */
  void set_callback(PyObject* callback_function) {
    callback_function_ = callback_function;
  }

 private:
  static void loadBot(const std::string identifier,
                      const std::string configuration,
                      boost::python::dict* result) {
    try {
      std::auto_ptr<pybot> bot(new pybot(configuration));
      {
        boost::lock_guard<boost::mutex> lock(call_mutex_);
        (*result)[identifier] = boost::python::object(bot);
      }
    } catch(const botscript::lua_exception& e) {
    } catch(const botscript::bad_login_exception& e) {
    } catch(const botscript::invalid_proxy_exception& e) {
    }
  }

  PyObject* callback_function_;
  static boost::mutex call_mutex_;
  static boost::mutex load_mutex_;
};

// Do static initialization.
boost::mutex pybot::call_mutex_;
boost::mutex pybot::load_mutex_;

/// Translates botscript::bad_login_exception to Python user warning.
void bad_login_translator(botscript::bad_login_exception const& e) {
  PyErr_SetString(PyExc_UserWarning, "bad login");
}

/// Translates botscript::lua_exception to Python user warning.
void lua_exception_translator(botscript::lua_exception const& e) {
  PyErr_SetString(PyExc_UserWarning, e.what());
}

/// Translates botscript::invalid_proxy_exception to Python user warning.
void invalid_proxy_exception_translator(
        botscript::invalid_proxy_exception const& e) {
  PyErr_SetString(PyExc_UserWarning, "bad proxy");
}

BOOST_PYTHON_MODULE(pybotscript) {
  PyEval_InitThreads();
  typedef pybot bot;
  using boost::python::class_;
  using boost::python::init;
  using boost::python::register_exception_translator;
  class_<bot, std::auto_ptr<bot>, boost::noncopyable >("bot",
          init< std::string, std::string, std::string, \
               std::string, std::string>())
    .def(init<std::string>())
    .def("configuration", &bot::configuration)
    .def("identifier", &bot::identifier)
    .def("execute", &bot::execute)
    .def("log", &bot::log_msgs)
    .def("callback", &bot::set_callback)
    .def("packages", &bot::loadPackages)
    .def("interface_description", &bot::interface_description)
    .staticmethod("packages")
    .def("create_identifier", &bot::createIdentifier)
    .staticmethod("create_identifier")
    .def("load_bots", &bot::loadBots)
    .staticmethod("load_bots")
    .def("load_bot", &bot::loadSingleBot)
    .staticmethod("load_bot");
  register_exception_translator<botscript::bad_login_exception>(
          bad_login_translator);
  register_exception_translator<botscript::lua_exception>(
          lua_exception_translator);
  register_exception_translator<botscript::invalid_proxy_exception>(
          invalid_proxy_exception_translator);
}

