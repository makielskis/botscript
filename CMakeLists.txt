cmake_minimum_required(VERSION 2.6)
project(botscript)


################################
# Compile flags
################################
if(MINGW)
  add_definitions(-DBOOST_THREAD_USE_LIB -D_WIN32_WINNT=0x0501)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
elseif(ANDROID)
  add_definitions(-DANDROID)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -fPIC")
endif()


################################
# Dependencies
################################
add_subdirectory(external_lib/lua-5.2.0)
add_subdirectory(external_lib/pugixml-1.0/scripts)
add_subdirectory(external_lib/tidy)


################################
# Boost
################################
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
# Include Paths
################################
include_directories("external_lib/rapidjson/include")
include_directories("external_lib/lua-5.2.0/src")
include_directories("external_lib/tidy/include")
include_directories("external_lib/pugixml-1.0/src")


################################
# Static library
################################
file(GLOB exe_files "src/main.cc")
file(GLOB_RECURSE bs_files "src/*.cc")
list(REMOVE_ITEM  bs_files ${exe_files})
add_library(bs STATIC EXCLUDE_FROM_ALL ${bs_files})
if(MINGW)
  target_link_libraries(bs -Wl,-Bstatic ${Boost_LIBRARIES} tidy pugixml lua z ws2_32 mswsock)
else()
  target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
endif()


################################
# Main executable
################################
add_executable(botscript ${exe_files})
if(MINGW)
  target_link_libraries(botscript -Wl,-Bstatic ${Boost_LIBRARIES} tidy pugixml lua z ws2_32 mswsock)
else()
  target_link_libraries(botscript bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
endif()


################################
# Unit Tests
################################
add_subdirectory(test/gtest-1.7.0)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(botscript-tests EXCLUDE_FROM_ALL
  test/config_test.cpp
)

target_link_libraries(botscript-tests gtest gtest_main bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
add_test(botscript-tests botscript-tests)
