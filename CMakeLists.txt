cmake_minimum_required(VERSION 3.10)

project(pawnraknet)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(AddSAMPPlugin)

add_subdirectory(lib/omp-sdk)

add_samp_plugin(${PROJECT_NAME}
  plugin.def

  src/Pawn.RakNet.inc
  src/main.h
  src/main.cc
  src/plugin.h
  src/plugin.cc
  src/native_param.h
  src/config.h
  src/config.cc
  src/bitstream_handle.h
  src/bitstream_handle.cc
  src/bitstream_pool.h
  src/bitstream_pool.cc
  src/script.h
  src/script.cc

  lib/RakNet/bitstream.hpp
  lib/RakNet/bitstream.cpp
  lib/RakNet/packet.hpp
  lib/RakNet/Encoding/huffman_tree_node.hpp
  lib/RakNet/Encoding/huffman_tree.hpp
  lib/RakNet/Encoding/huffman_tree.cpp
  lib/RakNet/Encoding/str_compress.hpp
  lib/RakNet/Encoding/str_compress.cpp

  lib/samp-ptl/ptl.h
)

target_include_directories(${PROJECT_NAME} PRIVATE lib lib/samp-ptl)

target_link_libraries(${PROJECT_NAME} PRIVATE OMP-SDK)

# AddSAMPPlugin targets the 32-bit SA-MP ABI: hardcoded `-m32` for GCC and
# `POSITION_INDEPENDENT_CODE Off`. Neither works on 64-bit Unix — a shared
# object must be PIC, and `-m32` conflicts with building an x64 binary at all.
# Undo both when the toolchain is producing a 64-bit build so we can ship an
# x64 component for open.mp without forking the cmake-modules submodule.
if(CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  foreach(prop COMPILE_FLAGS LINK_FLAGS)
    get_target_property(flags ${PROJECT_NAME} ${prop})
    if(flags)
      string(REPLACE "-m32" "" flags "${flags}")
      set_target_properties(${PROJECT_NAME} PROPERTIES ${prop} "${flags}")
    endif()
  endforeach()

  set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
