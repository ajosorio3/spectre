// Distributed under the MIT License.
// See LICENSE.txt for details.

// This file is configured into the build directory by CMake and compiled as
// part of the build system. It exposes information from CMake to C++.

#include "@CMAKE_SOURCE_DIR@/src/Informer/InfoFromBuild.hpp"

#include <sstream>
#include <string>

std::string spectre_version() { return std::string("@SPECTRE_VERSION@"); }

std::string unit_test_build_path() { return "@CMAKE_BINARY_DIR@/tests/Unit/"; }

std::string unit_test_src_path() { return "@CMAKE_SOURCE_DIR@/tests/Unit/"; }

std::string info_from_build() {
  std::ostringstream os;
  os << "SpECTRE Build Information:\n";
  os << "Version:                      " << spectre_version() << "\n";
  os << "Compiled on host:             @HOSTNAME@\n";
  os << "Compiled in directory:        @CMAKE_BINARY_DIR@\n";
  os << "Source directory is:          @CMAKE_SOURCE_DIR@\n";
  os << "Compiled on git branch:       " << git_branch() << "\n";
  os << "Compiled on git revision:     " << git_description() << "\n";
  os << "Linked on:                    " << link_date() << "\n";
#ifdef SPECTRE_DEBUG
  os << "Build type:                   Debug\n";
#else
  os << "Build type:                   Release\n";
#endif
  return os.str();
}

#ifndef __APPLE__
/* Set up a pretty print script for GDB to print spectre types in GDB in a
 * more readable manner.
 *
 *
 * Note: The "MS" section flags are to remove duplicates.
 */
#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name                    \
      "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("@CMAKE_SOURCE_DIR@/tools/SpectrePrettyPrinters.py")

#undef DEFINE_GDB_PY_SCRIPT
#endif // ndef __APPLE__
