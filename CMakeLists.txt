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
# Dependencies
################################
add_subdirectory(external_lib/lua-5.2.0)
add_subdirectory(external_lib/pugixml-1.0/scripts)
add_subdirectory(external_lib/tidy)


################################
# Boost
################################
if(MSVC)
  set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "C:/boost_1_52_0")
  set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "C:/boost_1_52_0/stage/lib")
endif()

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS
  system
  thread
  filesystem
  regex
  iostreams
REQUIRED)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIR})
else(Boost_FOUND)
	message("No Boost found.")
endif(Boost_FOUND)
link_directories(${CMAKE_LIBRARY_PATH})


################################
# Compile flags
################################
if(MSVC)
  # VS2012 doesn't support correctly the tuples yet
  add_definitions(/D _VARIADIC_MAX=10)
  add_definitions(/D _WIN32_WINNT=0x0501)
elseif(MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  add_definitions("-DBOOST_THREAD_USE_LIB")
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
# Static library
################################
add_library(bs STATIC
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
  src/config.cc
  src/bot.cc
)
if(MSVC)
  target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua)
elseif(MINGW)
  target_link_libraries(bs -Wl,-Bstatic ${Boost_LIBRARIES} tidy pugixml lua z ws2_32 mswsock)
else()
  target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
endif()


################################
# Main executable
################################
add_executable(botscript src/main.cc)
if(MSVC)
  target_link_libraries(botscript bs ${Boost_LIBRARIES} tidy pugixml lua)
elseif(MINGW)
  target_link_libraries(botscript -Wl,-Bstatic ${Boost_LIBRARIES} tidy pugixml lua z ws2_32 mswsock)
else()
  target_link_libraries(botscript bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
endif()


################################
# JNI Library
################################
add_library(bs_lib SHARED src/java/net_makielski_botscript_Bot.cpp)
if(MSVC)
  target_link_libraries(bs_lib bs ${Boost_LIBRARIES} tidy pugixml lua)
else()
  target_link_libraries(bs_lib bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
endif()


################################
# Unit Tests
################################
add_subdirectory(test/gtest-1.6.0)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(botscript-tests
  test/config_test.cpp
)

target_link_libraries(botscript-tests gtest gtest_main bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
add_test(botscript-tests botscript-tests)
