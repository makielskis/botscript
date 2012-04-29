cmake_minimum_required (VERSION 2.6)
project(botscript)

#SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
#SET (CMAKE_CXX_FLAGS                "-Wall")
#SET (CMAKE_CXX_FLAGS_DEBUG          "-g")
#SET (CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
#SET (CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
#SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

add_executable(botscript main.cpp lua_connection.cpp bot.cpp module.cpp)
add_library(pybotscript SHARED pybotscript.cpp lua_connection.cpp bot.cpp module.cpp)

set_target_properties(pybotscript PROPERTIES LINKER_LANGUAGE CXX)

include_directories("/home/wasabi/boost_1_48_0/")
include_directories("/home/wasabi/Development/cppbot/botscript/rapidjson-read-only/include/")
include_directories("/home/wasabi/Development/cppbot/botscript/lua-5.2.0/src")
include_directories("/usr/include/python2.7")
include_directories("/home/wasabi/Development/cppbot/botscript/tidy/include")

set_target_properties(botscript PROPERTIES LINK_FLAGS "-L/home/wasabi/boost_1_48_0/stage/lib/ -L/home/wasabi/Development/cppbot/botscript/")
set_target_properties(pybotscript PROPERTIES LINK_FLAGS "-L/home/wasabi/boost_1_48_0/stage/lib/ -L/home/wasabi/Development/cppbot/botscript/")

target_link_libraries(botscript boost_system boost_thread boost_regex boost_iostreams boost_filesystem -pthread tidy pugixml lua dl z)
target_link_libraries(pybotscript boost_system boost_python boost_thread boost_regex boost_iostreams boost_filesystem -pthread tidy pugixml lua dl z)

