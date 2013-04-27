About
=====

Botscript is a tool that makes life easier when programming browser game bots.
But it can also be used for other HTTP based activities (such as unit testing).
It comes with a handy Node.js API that can be used to build nifty user
interfaces or other services around botscript.

Botscript makes heavy use of the Boost C++ extensions.  
Other external libraries used are: Lua 5.2, pugixml 1.0, rapidjson and HTML Tidy.

Building from Sources
=====================

We've build Botscript on Linux (Ubuntu 10.04, 11.10, 12.04; x86_64 and ARMv7)
and Windows (Windows 7, x86_64 using Visual Studio 2012) but since Botscript 
itself as well as all dependencies are intended to be cross-platform, we assume
that it will also run on Mac OS, Solaris, BSD, et cetera.

We use C++11. An up-to-date compiler is required.

Linux
-----

The following steps will guide you through building Botscript from the sources
on a Debian/Ubuntu based system. But on other Unix systems the packages will
have similar names.

You need either to be root or prepend `sudo` to use `apt-get` / `aptitude`.

### Boost
  - Get zlib-dev for boost-iostreams: `apt-get install libz-dev`
  - Download Boost (>= 1.52.0) from [boost.org](http://boost.org)
  - Build bjam according to the Boost introductions (`sh ./bootstrap.sh`).
  - `NO_BZIP2=1 ./b2 --layout=system --with-system --with-thread --with-regex --with-iostreams --with-filesystem cxxflags=-fPIC variant=release link=static threading=multi runtime-link=static`

### Lua 5.2
  - Get readline: `apt-get install libreadline-dev libncurses-dev`
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


### Botscript
  - Create and goto build folder `mkdir build && cd build`
  - Edit CMake boost path (line 4 and 5 in src/CMakeLists.txt).
  - Create makefile from cmake file: `cmake ../src`
  - Make: `make`
  - Voilà! You just have build botscript.


Windows
-------

To build Botscript on Windows, you need to have Visual Studio 2012 installed.

### Boost

  - Download Boost (>= 1.52.0) from [boost.org](http://boost.org)
  - Extract the downloaded archive (i.e. to `C:\boost...`
  - Open a console (cmd) and go to the extracted folder: `cd C:\boost...`
  - `bootstrap`
  - Download the zlib from [zlib.net](http://zlib.net/) and extract the archive
  - `b2 --with-system --with-thread --with-date_time --with-chrono --with-regex --with-filesystem --with-iostreams -s ZLIB_SOURCE=C:\path\to\zlib`

### Botscript

  - Open Botscript VS Solution located at `visual_studio\botscript\botscript.sln`
  - Change boost path: right-click the Botscript project - options - linker options - additional linker paths
  - Right-click the Botscript project and choose "Build"


Build BotScript Node.js extension
---------------------------------

  - Install Node.js following the steps on:
    https://github.com/joyent/node/wiki/Installing-Node.js-via-package-manager
  - Adjust the Boost path (`boost_path`) and the current project path (`build_path`) in the "bindings.gyp" file
  - Install node-gyp: `npm install -g node-gyp`
  - Execute `node-gyp configure`
    (or `node-gyp configure build` and leave out the next step)
  - Execute `node-gyp build`
  - Use the resulting addon.node file in `build/Release`
