set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0.0)
set(CMAKE_SYSTEM_PROCESSOR armv7)
get_filename_component(BERRYGRAM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_C_COMPILER "${BERRYGRAM_ROOT}/porting/native/bb10-cc")
set(CMAKE_CXX_COMPILER "${BERRYGRAM_ROOT}/porting/native/bb10-cxx")
set(CMAKE_AR "${BERRYGRAM_ROOT}/porting/native/bb10-ar")
set(CMAKE_RANLIB "${BERRYGRAM_ROOT}/porting/native/bb10-ranlib")
set(CMAKE_FIND_ROOT_PATH
    "${BERRYGRAM_ROOT}/build/porting/work/native-prefix"
    "${BERRYGRAM_ROOT}/build/porting/gcc9/gcc-9.3.0-berrycore/arm-blackberry-qnx8eabi"
    "${BERRYGRAM_ROOT}/build/porting/gcc9/gcc-9.3.0-berrycore")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
