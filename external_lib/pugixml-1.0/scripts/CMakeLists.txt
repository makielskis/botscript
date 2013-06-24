project(pugixml)

if(MSVC)
else()
	# Enable building a shared library.
	add_definitions("-fPIC")
endif()

set(SOURCES ../src/pugixml.hpp ../src/pugiconfig.hpp ../src/pugixml.cpp)

add_library(pugixml STATIC ${SOURCES})
