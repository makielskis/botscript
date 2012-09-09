About
-----

Botscript is a library thats main target is to make life easier when programming
browser game bots. But it can also be used for other HTTP based activities.
It comes with a handy Node.js API that can be used to build
nifty user interfaces or other services around botscript.

Botscript makes heavy use of the Boost C++ extensions.  
Other external libraries used are: Lua 5.2, pugixml 1.0, rapidjson and HTML Tidy.

Build external libraries
------------------------

This steps will help you to build the dependencies of Bot Script on
a Debian/Ubuntu system. But on other systems the packages will have similar names.

You need either to be root or prepend `sudo` to use `apt-get`.

### Boost
  - Get zlib-dev for boost-iostreams: `apt-get install libz-dev`
  - Download Boost (>= 1.48.0) from boost.org
  - Build bjam according to the Boost introductions (`sh ./bootstrap.sh`).
  - `NO_BZIP2=1 ./b2 --layout=system --with-system --with-thread --with-regex --with-iostreams --with-filesystem cxxflags=-fPIC variant=release link=static threading=multi runtime-link=static`

### Lua 5.2
  - Get readline: `apt-get install libreadline-dev`
  - Go to Lua folder: `cd external_lib/lua-5.2.0`
  - Make Lua: `make linux CC="g++ -fPIC"` (replace "linux" by your system)
  - Copy static library: `cp src/liblua.a ../../`

### pugixml 1.0
  - Install CMake: `apt-get install cmake`
  - Go to pugixml folder: `cd external_lib/pugixml-1.0`
  - Create and go to build folder: `mkdir build && cd build`
  - Create makefile from cmake file: `cmake ../scripts/`
  - Make: `make CXX_FLAGS="-fPIC"`
  - Copy static library: `cp libpugixml.a ../../../`

### HTML Tidy
  - Install autotools: `apt-get install libtool automake`
  - Go to tidy folder: `cd external_lib/tidy`
  - Generate build setup: `sh build/gnuauto/setup.sh`
  - Configure: `./configure --enable-shared=no CC="gcc -fPIC"`
  - Make: `make`
  - Copy static library: `cp src/.libs/libtidy.a ../../`


Build BotScript
---------------

  - Create and goto build folder `mkdir build && cd build`
  - Edit CMake boost path (line 4 and 5 in src/CMakeLists.txt).
  - Create makefile from cmake file: `cmake ../src`
  - Make: `make`
  - Voilà! You just have build botscript.

Build BotScript Node.js extension
---------------------------------

  - Install Node.js following the steps on:
    https://github.com/joyent/node/wiki/Installing-Node.js-via-package-manager
  - Adjust the Boost path (`BOOST_PATH`) and the current project path (`BUILD_PATH`) in the "wscript" file
  - Execute `node-waf configure`
    (or `node-waf configure build` and leave out the next step)
  - Execute `node-waf build`
  - Use the resulting addon.node file in `build/Release`

How to Use the BotScript Node.js extension
------------------------------------------

TODO: write this section
