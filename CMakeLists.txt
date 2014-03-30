cmake_minimum_required(VERSION 2.6)
project(botscript)


################################
# Dependencies
################################
if (NOT TARGET lua)
  add_subdirectory(external_lib/lua-5.2)
endif()
add_subdirectory(external_lib/pugixml-1.2/scripts)
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


################################
# Include Paths
################################
include_directories("external_lib/lua-5.2/src")
include_directories("external_lib/rapidjson/include")
include_directories("external_lib/tidy/include")
include_directories("external_lib/pugixml-1.2/src")
include_directories("./src")


################################
# Static packages
################################
if (STATIC_PACKAGES)
  add_definitions(-DSTATIC_PACKAGES)
endif()


################################
# Package compilation
################################
if (ANDROID OR STATIC_PACKAGES)
  add_custom_target(compile_packages
    rm -rf ${CMAKE_CURRENT_LIST_DIR}/src/pg.h &&
    rm -rf ${CMAKE_CURRENT_LIST_DIR}/src/kv.h &&
    python ${CMAKE_CURRENT_LIST_DIR}/generate_package.py ${CMAKE_CURRENT_LIST_DIR}/packages/kv ${CMAKE_CURRENT_LIST_DIR}/src/kv.h HGC ${luac_location} &&
    python ${CMAKE_CURRENT_LIST_DIR}/generate_package.py ${CMAKE_CURRENT_LIST_DIR}/packages/pg ${CMAKE_CURRENT_LIST_DIR}/src/pg.h HGC ${luac_location}
  )
elseif (MINGW)
  add_custom_target(compile_packages
    rm -rf pg && rm -rf kv &&
    python ${CMAKE_CURRENT_LIST_DIR}/generate_package.py ${CMAKE_CURRENT_LIST_DIR}/packages/kv kv GCS ${luac_location} &&
    python ${CMAKE_CURRENT_LIST_DIR}/generate_package.py ${CMAKE_CURRENT_LIST_DIR}/packages/pg pg GCS ${luac_location} &&
    cd kv &&
    cmake -DCMAKE_TOOLCHAIN_FILE=~/mxe/usr/i686-pc-mingw32/share/cmake/mxe-conf.cmake -DBoost_THREADAPI=win32 . &&
    make &&
    ~/mxe/usr/bin/i686-pc-mingw32-strip kv.package &&
    cd .. &&
    cd pg &&
    cmake -DCMAKE_TOOLCHAIN_FILE=~/mxe/usr/i686-pc-mingw32/share/cmake/mxe-conf.cmake -DBoost_THREADAPI=win32 . &&
    make &&
    ~/mxe/usr/bin/i686-pc-mingw32-strip pg.package
  )
else()
  add_custom_target(compile_packages
    rm -rf pg && rm -rf kv &&
    python ${CMAKE_CURRENT_LIST_DIR}/generate_package.py ${CMAKE_CURRENT_LIST_DIR}/packages/kv kv GC ${luac_location} &&
    python ${CMAKE_CURRENT_LIST_DIR}/generate_package.py ${CMAKE_CURRENT_LIST_DIR}/packages/pg pg GC ${luac_location} &&
    cd kv &&
    cmake -DCMAKE_BUILD_TYPE=MinSizeRel . &&
    make &&
    cd .. &&
    cd pg &&
    cmake -DCMAKE_BUILD_TYPE=MinSizeRel . &&
    make
  )
endif()
add_dependencies(compile_packages luac)


################################
# Static library
################################
file(GLOB exe_files "src/main.cc")
file(GLOB_RECURSE bs_files "src/*.cc")
list(REMOVE_ITEM  bs_files ${exe_files})
add_library(bs STATIC EXCLUDE_FROM_ALL ${bs_files})
if(MSVC)
  target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua)
  set(_def
    _VARIADIC_MAX=10
    _WIN32_WINNT=0x0501
  )
  set_target_properties(bs PROPERTIES COMPILE_DEFINITIONS "${_def}")
elseif(MINGW)
  target_link_libraries(bs -Wl,-Bstatic z ${Boost_LIBRARIES} tidy pugixml lua ws2_32 mswsock)
  set_target_properties(bs PROPERTIES COMPILE_FLAGS "-std=c++11 -s")
  set(_def
    BOOST_THREAD_USE_LIB
    WINVER=0x0501
    _WIN32_WINNT=0x0501
    _WIN32_IE=0x0501
  )
  set_target_properties(bs PROPERTIES COMPILE_DEFINITIONS "${_def}")
elseif(ANDROID)
  target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
  set_target_properties(bs PROPERTIES COMPILE_FLAGS "-std=c++11 -fPIC")
  set(_def ANDROID)
  set_target_properties(bs PROPERTIES COMPILE_DEFINITIONS "${_def}")
  add_dependencies(bs compile_packages)
else()
  set_target_properties(bs PROPERTIES COMPILE_FLAGS "-Wall -Wextra -std=c++11")
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua z)
  else()
    target_link_libraries(bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
  endif()
endif()


################################
# Main executable (currently not supported)
################################
#add_executable(botscript ${exe_files})
#if(MSVC)
#  target_link_libraries(botscript bs ${Boost_LIBRARIES} tidy pugixml lua)
#elseif(MINGW)
#  target_link_libraries(botscript -Wl,-Bstatic ${Boost_LIBRARIES} tidy pugixml lua z ws2_32 mswsock)
#else()
#  target_link_libraries(botscript bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
#endif()


################################
# Unit Tests
################################
add_subdirectory(test/gtest-1.7.0)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(botscript-tests EXCLUDE_FROM_ALL test/config_test.cpp)
set_target_properties(botscript-tests PROPERTIES COMPILE_FLAGS "-std=c++11")

target_link_libraries(botscript-tests gtest gtest_main bs ${Boost_LIBRARIES} tidy pugixml lua z dl -pthread)
add_test(botscript-tests botscript-tests)
