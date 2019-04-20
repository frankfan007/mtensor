set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER ${GCC_LINARO_TOOLCHAIN}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${GCC_LINARO_TOOLCHAIN}/bin/arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH ${GCC_LINARO_TOOLCHAIN})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)