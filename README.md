About
-----

Botscript is a library thats main target is to make life easier when programming
 browser game bots. But it can also be used for other HTTP based activities.
 It comes with a handy python API that can be used to build
 nifty user interfaces or other services around botscript.

Botscript makes heavy use of the Boost C++ extensions.
 Other external libraries used are: Lua 5.2, pugixml 1.0 and HTML Tidy.

Build external libraries
------------------------

### MongoDB Client (only for proxy_check)
  - Install the scons build system for building the MongoDB client.
  - Go to MongoDB Client folder: `mongo-client-2.1.1`
  - Build MongoDB Client library `scons install --prefix ./build`
  - Copy static library: `cp build/libmongoclient.a ../../`
  - Open the -mongo-cxx-driver.tgz- archive and copy the
    `/mongo-cxx-driver/src/mongo/platform/` folder to
    `external_lib/mongo-client-2.1.1/build/mongo`

### Python (only for pybotscript)
  - Install the python development package from your package management (Linux) 
    or download the required files (Windows)

### Boost
  - Download Boost 1.48.0 from boost.org
  - Build bjam according to the Boost introduction.
  - `./b2 --layout=system --with-system --with-python --with-thread --with-regex --with-iostreams --with-filesystem cxxflags=-fPIC variant=release link=static threading=multi runtime-link=static`

### Lua 5.2
  - Go to Lua folder: `cd external_lib/lua-5.2.0`
  - Make Lua: `make linux CC="g++ -fPIC"` (replace "linux" by your system)
  - Copy static library: `cp src/liblua.a ../../`

### pugixml 1.0
  - Go to pugixml folder: `cd external_lib/pugixml-1.0`
  - Create and go to build folder: `mkdir build && cd build`
  - Create makefile from cmake file: `cmake ../scripts/`
  - Make: `make CXX_FLAGS="-fPIC"`
  - Copy static library: `cp libpugixml.a ../../../`

### HTML Tidy
  - Go to tidy folder: `cd external_lib/tidy`
  - Generate build setup: `sh build/gnuauto/setup.sh`
  - Configure: `./configure --enable-shared=no CC="gcc -fPIC"`
  - Make: `make`
  - Copy static library: `cp src/.libs/libtidy.a ../../`


Build BotScript
---------------

  - Create and goto build folder `mkdir build && cd build`
  - Edit cmake boost path and python include path (line 4 and 5).  
  (- If you want to get an executable file uncomment all 'botscript' lines.)
  - Create makefile from cmake file: `cmake ../src`
  - Make: `make`
  - Rename library: `mv libpybotscript.so pybotscript.so`
  - Have fun with pybotscript!

How to use pybotscript
----------------------

`$ python`  
`>>> from pybotscript import bot`  
`>>> mybot = bot("username", "password", "package", "http://server.com", "proxy:port")`  
`>>> mybot.execute("set_module123_active", "1")`

For a complete reference have a look at the pybotscript documentation.
