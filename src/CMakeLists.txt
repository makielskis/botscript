cmake_minimum_required (VERSION 2.6)
project(botscript)

add_library(pybotscript SHARED pybotscript.cpp lua_connection.cpp bot.cpp module.cpp)
set_target_properties(pybotscript PROPERTIES LINKER_LANGUAGE CXX)

# Boost libraries
find_package(Boost 1.48.0 COMPONENTS system python thread regex iostreams filesystem REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

include_directories("/usr/include/python2.7")
include_directories("../external_lib/rapidjson-64/include/")
include_directories("../external_lib/lua-5.2.0/src")
include_directories("../external_lib/tidy/include")

set_target_properties(pybotscript PROPERTIES LINK_FLAGS "-L/home/wasabi/boost_1_48_0/stage/lib/ -L../")
target_link_libraries(pybotscript -pthread tidy pugixml lua dl z)

