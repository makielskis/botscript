# Compile and run release version:
# mkdir build
# cd build
# cmake -DCMAKE_BUILD_TYPE=Release ..
# cd ..
# ./build/botscript

# Compile and run debug version
# mkdir build
# cd build
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cd ..
# ./build/botscript

cmake_minimum_required(VERSION 2.8.8)
project(botscript)


################################
# Boost
################################
FIND_PACKAGE(Boost COMPONENTS
  system
  thread
  filesystem
  regex
  iostreams
REQUIRED)
IF (Boost_FOUND)
	INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
ELSE(Boost_FOUND)
	MESSAGE("No Boost found.")
ENDIF(Boost_FOUND)


################################
# Compile flags
################################
if(MSVC)
	# VS2012 doesn't support correctly the tuples yet
	add_definitions(/D _VARIADIC_MAX=10)
else()
	# GCC -> use c++11 standard.
	add_definitions("-std=c++11 -fPIC")

  # Show all warnings.
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
  set(CMAKE_CSS_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
endif()


################################
# Include Paths
################################
include_directories("external_lib/rapidjson-r64/include")
include_directories("external_lib/lua-5.2.0/src")
include_directories("external_lib/tidy/include")
include_directories("external_lib/pugixml-1.0/src")
include_directories("/usr/lib/jvm/java-7-openjdk-amd64/include/")


################################
# Object library
################################
add_library(botscript_obj_lib OBJECT
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
)

################################
# Main executable
################################
add_executable(botscript
  $<TARGET_OBJECTS:botscript_obj_lib>
  src/main.cc
)
set_target_properties(botscript PROPERTIES LINK_FLAGS "-L../")
target_link_libraries(botscript
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_IOSTREAMS_LIBRARY}
  z
  tidy
  pugixml
  lua
  dl
  -pthread
)


################################
# JNI Library
################################
add_library(bs_lib SHARED
  $<TARGET_OBJECTS:botscript_obj_lib>
  src/java/net_makielski_botscript_Bot.cpp
)
set_target_properties(bs_lib PROPERTIES LINK_FLAGS "-L../")
target_link_libraries(bs_lib
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_IOSTREAMS_LIBRARY}
  z
  tidy
  pugixml
  lua
  dl
  -pthread
)
