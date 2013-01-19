import subprocess
import sys
import os, os.path
import gzip
from os.path import basename

###################
# CHECK ARGUMENTS #
###################
if len(sys.argv) != 3:
  print("usage: " + sys.argv[0] + " package_folder output_folder")
  exit(1)

#################
# CREATE FOLDER #
#################
if not os.path.exists(sys.argv[2]): os.makedirs(sys.argv[2])

###################
# BASIC VARIABLES #
###################
package_name = os.path.basename(os.path.splitext(sys.argv[2])[0])
folder = sys.argv[1] + "/"
output = open(sys.argv[2] + "/" + package_name + ".cc", "w")

####################
# WRITE CMAKE FILE #
####################
cmake = open(sys.argv[2] + "/CMakeLists.txt", "w")
cmake.write("\
cmake_minimum_required (VERSION 2.6)\n\
project(kv)\n\
add_library(kv SHARED " + sys.argv[2] + ".cc)\n\
set(CMAKE_SHARED_LIBRARY_SUFFIX "")\n\
set(CMAKE_SHARED_LIBRARY_PREFIX "")\n\
set_target_properties(kv PROPERTIES OUTPUT_NAME \"" + package_name + ".package\")")
cmake.close()

######################
# WRITE LIBRARY CODE #
######################
output.write("\
#include <string>\n\
#include <map>\n\
\n\
extern \"C\" std::map<std::string, std::string> load_" + package_name + "() {\n\
  std::map<std::string, std::string> modules;\n\n")

for name in os.listdir(folder):
  module = os.path.basename(os.path.splitext(name)[0])
  extension = os.path.splitext(name)[1]
  name = folder + name
  if extension == ".lua" and not name.startswith("."):

    output.write("  const char " + module + "[] = {\n")

    luac = module + ".lua.bin"
    subprocess.call(["luac  -o " + luac + " " + name], shell=True)

    f_in = open(name, 'rb')
    f_out = gzip.open(name + ".gz", 'wb')
    f_out.writelines(f_in)
    f_out.close()
    f_in.close()

    byte_count = 0

    with open(name + ".gz", "rb") as f:
      count = 0
      byte = f.read(1)
      while byte != "":
        if count == 0: output.write("    ")
        output.write("(char) 0x" + byte.encode('hex'))
        byte_count += 1
        byte = f.read(1)
        if byte != "":
          output.write(", ")
        if count > 10:
          count = 0
          output.write("\n")
        else:
          count += 1

    os.remove(luac)
    os.remove(name + ".gz")

    output.write("\n  };\n")
    output.write("  modules[\"" + module + "\"] = std::string(" + module + ", " + str(byte_count) + ");\n\n")

output.write("\
  return modules;\n\
}\n")

output.close()
