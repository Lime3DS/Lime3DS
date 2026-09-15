#!/bin/bash -ex

# A basic configuration for the MXE build requires some additional
# flags to build successfully. As such, this script is provided for
# developer convenience.

# This script assumes you are calling from inside the Azahar build enviroment.

# Usage: [...]/tools/basic-mxe-config.sh <relative path to Azahar project root>
# e.g. `../tools/basic-mxe-config.sh ..`

x86_64-w64-mingw32.shared-cmake $1 \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER_AR=x86_64-w64-mingw32.shared-ar \
    -DCMAKE_CXX_COMPILER_RANLIB=x86_64-w64-mingw32.shared-ranlib
