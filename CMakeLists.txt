cmake_minimum_required (VERSION 2.6)
project(botscript)

set(BOOST_PATH "/home/wasabi/boost_1_52_0/")

# Uncomment to use Clang compiler (improved error/warning output).
SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
SET (CMAKE_CXX_FLAGS                "-std=c++0x -g -O0")
#SET (CMAKE_CXX_FLAGS_DEBUG          "-g")
#SET (CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
#SET (CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
#SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

add_executable(proxy_check
  src/http/url.cc
  src/http/http_source.cc
  src/http/http_con.cc
  src/http/util.cc
  src/http/webclient.cc
  src/proxy_check.cpp
)

include_directories(${BOOST_PATH})
include_directories("external_lib/rapidjson-r64/include")
include_directories("external_lib/lua-5.2.0/src")
include_directories("external_lib/tidy/include")
include_directories("external_lib/pugixml-1.0/src")

set_target_properties(proxy_check PROPERTIES LINK_FLAGS "-L../ -L${BOOST_PATH}stage/lib/")

target_link_libraries(proxy_check boost_system boost_thread boost_regex boost_iostreams -pthread z tidy pugixml lua z dl)
