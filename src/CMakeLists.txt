cmake_minimum_required (VERSION 2.6)
project(botscript)

set(BOOST_PATH "../../../../boost_1_48_0/")
set(PYTHON_INCLUDE_PATH "/usr/include/python2.7")

# Uncomment to use Clang compiler (improved error/warning output).
#SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
#SET (CMAKE_CXX_FLAGS                "-Wall")
#SET (CMAKE_CXX_FLAGS_DEBUG          "-g")
#SET (CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
#SET (CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
#SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

add_executable(botscript main.cpp lua_connection.cpp bot.cpp module.cpp)
add_library(pybotscript SHARED pybotscript.cpp lua_connection.cpp bot.cpp module.cpp)
set_target_properties(pybotscript PROPERTIES LINKER_LANGUAGE CXX)

include_directories(${BOOST_PATH})
include_directories(${PYTHON_INCLUDE_PATH})
include_directories("../external_lib/rapidjson-r64/include")
include_directories("../external_lib/lua-5.2.0/src")
include_directories("../external_lib/tidy/include")
include_directories("../external_lib/pugixml-1.0/src")

set_target_properties(botscript PROPERTIES LINK_FLAGS "-L../ -L${BOOST_PATH}stage/lib/")
set_target_properties(pybotscript PROPERTIES LINK_FLAGS "-L../ -L${BOOST_PATH}stage/lib/")

target_link_libraries(botscript boost_system boost_thread boost_regex boost_iostreams boost_filesystem -pthread tidy pugixml lua dl z)
target_link_libraries(pybotscript boost_system boost_python boost_thread boost_regex boost_iostreams boost_filesystem -pthread tidy pugixml lua dl z)
