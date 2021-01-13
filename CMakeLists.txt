cmake_minimum_required(VERSION 3.17)
project(botscript)

find_package(Threads)

include(cmake/pkg.cmake)

################################
# Static packages
################################
option(STATIC_PACKAGES "STATIC_PACKAGES" ON)
if (STATIC_PACKAGES)
  add_definitions(-DSTATIC_PACKAGES)
endif()


################################
# Static library
################################
set(bs-boost-libs boost)
file(GLOB exe_files "src/main.cc")
file(GLOB_RECURSE bs_files "src/*.cc")
list(REMOVE_ITEM  bs_files ${exe_files})
add_library(bs EXCLUDE_FROM_ALL ${bs_files})
target_include_directories(bs PUBLIC src)
target_link_libraries(bs zlibstatic ssl crypto ${CMAKE_THREAD_LIBS_INIT})
if(MSVC)
  target_link_libraries(bs rapidjson tidy boost-filesystem boost-regex boost-iostreams ${bs-boost-libs} tidy pugixml lua)
  set(_def
    _VARIADIC_MAX=10
    _WIN32_WINNT=0x0501
  )
  set_target_properties(bs PROPERTIES COMPILE_DEFINITIONS "${_def}")
elseif(MINGW)
  target_link_libraries(bs rapidjson tidy boost-filesystem boost-regex boost-iostreams -Wl,-Bstatic ${bs-boost-libs} tidy pugixml lua ws2_32 mswsock)
  set_target_properties(bs PROPERTIES COMPILE_FLAGS "-std=c++11 -s")
  set(_def
    BOOST_THREAD_USE_LIB
    WINVER=0x0501
    _WIN32_WINNT=0x0501
    _WIN32_IE=0x0501
  )
  set_target_properties(bs PROPERTIES COMPILE_DEFINITIONS "${_def}")
elseif(ANDROID)
  target_link_libraries(bs rapidjson tidy boost-filesystem boost-regex boost-iostreams ${bs-boost-libs} tidy pugixml lua dl)
  set_target_properties(bs PROPERTIES COMPILE_FLAGS "-std=c++11 -fPIC")
  set(_def ANDROID)
  set_target_properties(bs PROPERTIES COMPILE_DEFINITIONS "${_def}")
else()
  set_target_properties(bs PROPERTIES COMPILE_FLAGS "-Wall -Wextra -std=c++11")
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(bs rapidjson tidy boost-filesystem boost-regex boost-iostreams ${bs-boost-libs} tidy pugixml lua)
  else()
    target_link_libraries(bs rapidjson tidy boost-filesystem boost-regex boost-iostreams ${bs-boost-libs} tidy pugixml lua dl)
  endif()
endif()

if(STATIC_PACKAGES)
  target_link_libraries(bs pg pg-res)
endif()


################################
# Unit Tests
################################
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dir.h.in
        ${CMAKE_BINARY_DIR}/generated/test_dir.h
)
add_library(test-dir INTERFACE)
target_include_directories(test-dir INTERFACE ${CMAKE_BINARY_DIR}/generated)

add_executable(botscript-tests EXCLUDE_FROM_ALL test/config_test.cpp)
set_target_properties(botscript-tests PROPERTIES COMPILE_FLAGS "-std=c++11")
target_link_libraries(botscript-tests test-dir boost-filesystem gtest gtest_main bs ${bs-boost-libs} tidy pugixml lua)
if (NOT MSVC)
  target_link_libraries(botscript-tests dl)
endif()
add_test(botscript-tests botscript-tests)
