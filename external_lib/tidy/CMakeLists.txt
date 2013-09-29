cmake_minimum_required(VERSION 2.8.8)
project(tidy)

if(MSVC)
elseif(MINGW)
	set_source_files_properties(src/mappedio.c PROPERTIES COMPILE_FLAGS -D_MSC_VER=1700)
	add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -D_WIN32_IE=0x0501)
else()
	# Enable building a shared library.
	add_definitions("-fPIC")
endif()

include_directories("include")
set(SOURCES
  src/access.c
  src/alloc.c
  src/attrask.c
  src/attrdict.c
  src/attrget.c
  src/attrs.c
  src/buffio.c
  src/clean.c
  src/config.c
  src/entities.c
  src/fileio.c
  src/istack.c
  src/lexer.c
  src/localize.c
  src/mappedio.c
  src/parser.c
  src/pprint.c
  src/streamio.c
  src/tagask.c
  src/tags.c
  src/tidylib.c
  src/tmbstr.c
  src/utf8.c
)

add_library(tidy STATIC ${SOURCES})
