cmake_minimum_required (VERSION 2.6)
project(botscript)

set(BOOST_PATH "/home/wasabi/boost_1_52_0/")

SET(CMAKE_CXX_FLAGS "-std=c++0x")

add_executable(botscript
  src/http/url.cc
  src/http/http_source.cc
  src/http/http_con.cc
  src/http/util.cc
  src/http/webclient.cc
  src/lua/lua_connection.cc
  src/lua/lua_util.cc
  src/lua/lua_http.cc
  src/bot_browser.cc
  src/module.cc
  src/package.cc
  src/bot.cc
  src/main.cc
)

add_library(bs_lib SHARED
  src/http/url.cc
  src/http/http_source.cc
  src/http/http_con.cc
  src/http/util.cc
  src/http/webclient.cc
  src/lua/lua_connection.cc
  src/lua/lua_util.cc
  src/lua/lua_http.cc
  src/bot_browser.cc
  src/module.cc
  src/package.cc
  src/bot.cc
  src/java/net_makielski_botscript_Bot.cpp
)

include_directories(${BOOST_PATH})
include_directories("external_lib/rapidjson-r64/include")
include_directories("external_lib/lua-5.2.0/src")
include_directories("external_lib/tidy/include")
include_directories("external_lib/pugixml-1.0/src")
include_directories("/usr/lib/jvm/java-6-openjdk-amd64/include/")

set_target_properties(botscript PROPERTIES LINK_FLAGS "-L../ -L${BOOST_PATH}stage/lib/")
set_target_properties(bs_lib PROPERTIES LINK_FLAGS "-L../ -L${BOOST_PATH}stage/lib/")

target_link_libraries(botscript boost_system boost_thread boost_filesystem boost_regex boost_iostreams -pthread z tidy pugixml lua z dl)
target_link_libraries(bs_lib boost_system boost_thread boost_filesystem boost_regex boost_iostreams -pthread z tidy pugixml lua z dl)
