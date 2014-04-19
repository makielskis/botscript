#!/usr/bin/python

import subprocess
import sys
import os, os.path
import gzip
from os.path import basename

########################
# CHECK ARGUMENT COUNT #
########################
if len(sys.argv) != 3 and len(sys.argv) != 4 and len(sys.argv) != 5:
  print("usage: " + sys.argv[0] + " package_folder output_folder [HGC] [luac_path]")
  print("FLAGS: H - Generate .h code instead of .cc code")
  print("       G - Use gzip compression")
  print("       C - Compile lua sources using luac")
  print("       S - Compile using the \"-s\" flag")
  exit(1)

################
# SET DEFAULTS #
################
generate_header = False
use_gzip        = False
use_luac        = False
use_s_flag      = False

##################
# ABSOLUTE PATHS #
##################
sys.argv[1] = os.path.abspath(sys.argv[1])
sys.argv[2] = os.path.abspath(sys.argv[2])

#########################
# READ LUAC PATH IF SET #
#########################
if len(sys.argv) == 5:
  luac_path = os.path.abspath(sys.argv[4])
else:
  luac_path = "luac"

#####################
# READ FLAGS IF SET #
#####################
if len(sys.argv) >= 4:
  for c in sys.argv[3]:
    if c == 'H':
      generate_header = True
    elif c == 'G':
      use_gzip = True
    elif c == 'C':
      use_luac = True
    elif c == 'S':
      use_s_flag = True
    else:
      print("unknown argument " + c)
      exit(1)

#################
# CREATE FOLDER #
#################
if not generate_header and not os.path.exists(sys.argv[2]): os.makedirs(sys.argv[2])

###################
# BASIC VARIABLES #
###################
if generate_header:
  extension = '.h'
else:
  extension = '.cc'
package_name = os.path.basename(os.path.splitext(sys.argv[2])[0])
folder = sys.argv[1] + "/"
if generate_header:
    output_file_path = sys.argv[2]
else:
    output_file_path = sys.argv[2] + "/" + package_name + extension
output_file_abs_path = os.path.abspath(output_file_path)
output = open(output_file_path, "w")

####################
# WRITE CMAKE FILE #
####################
if not generate_header:
    cmake = open(sys.argv[2] + "/CMakeLists.txt", "w")
    cmake.write("\
    cmake_minimum_required (VERSION 2.6)\n\
    project(" + package_name + ")\n" +
    ("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -s\")\n" if use_s_flag else "") +
    "add_library(" + package_name + " SHARED " + output_file_abs_path + ")\n\
    set(CMAKE_SHARED_LIBRARY_SUFFIX "")\n\
    set(CMAKE_SHARED_LIBRARY_PREFIX "")\n\
    set_target_properties(" + package_name + " PROPERTIES OUTPUT_NAME \"" + package_name + ".package\")")
    cmake.close()

######################
# WRITE LIBRARY CODE #
######################

# INCLUDES
output.write("\
#include <string>\n\
#include <map>\n\
\n")

# PREPROCESSOR DEFINES
if not generate_header:
  output.write("\
#if defined _WIN32 || defined _WIN64\n\
#define EXP_FUNCTION __declspec(dllexport) \n\
#define CALLING_CONVENTION __cdecl \n\
#else\n\
#define EXP_FUNCTION extern \"C\" \n\
#define CALLING_CONVENTION\n\
#endif\n\
\n\
extern \"C\" {\n\
\n\
EXP_FUNCTION ")

# FUNCTION DEFINITION
output.write("void*")
if not generate_header:
  output.write(" CALLING_CONVENTION")
output.write(" load_" + package_name + "() {\n\
  std::map<std::string, std::string>* modules = new std::map<std::string, std::string>;\n\n")

for name in os.listdir(folder):

  module = os.path.basename(os.path.splitext(name)[0])
  extension = os.path.splitext(name)[1]
  name = folder + name

  if extension == ".lua" and not name.startswith("."):
    filename = name

    output.write("  const char " + module + "[] = {\n")

    # APPLY LUAC
    if use_luac:
      filename = module + ".lua.bin"
      os.chdir(os.path.dirname(name))
      name = os.path.relpath(name)
      print(luac_path + " -o " + filename + " " + name)
      os.system(luac_path + " -o " + filename + " " + name)

    # APPLY GZIP
    if use_gzip:
      print("gzip " + filename)
      f_in = open(filename, 'rb')
      f_out = gzip.open(name + ".gz", 'wb')
      f_out.writelines(f_in)
      f_out.close()
      f_in.close()
      filename = name + ".gz"

    byte_count = 0
    with open(filename, "rb") as f:
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

    # CLEANUP LUAC OUTPUT
    if use_luac:
      os.remove(module + ".lua.bin")

    # CLEANUP GZIP OUTPUT
    if use_gzip:
      os.remove(name + ".gz")

    output.write("\n  };\n")
    output.write("  (*modules)[\"" + module + "\"] = std::string(" + module + ", " + str(byte_count) + ");\n\n")

output.write("\
  return modules;\n\
}")

if not generate_header:
  output.write("\n}")

output.close()
