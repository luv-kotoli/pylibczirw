cmake_minimum_required(VERSION 3.11)

set(CMAKE_MACOSX_RPATH 1)

IF (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version" FORCE)
ENDIF(APPLE)

project(_pylibCZIrw VERSION 3.3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

IF (WIN32)
    set(PYBIND11_CPP_STANDARD /std:c++17)
ELSE()
    # linking with 'thread' is necessary if we use std::thread and related under Linux it seems
    #   otherwise - the program simply crashes (no build-error)
    #set(CMAKE_C_COMPILER clang)
    #set(CMAKE_CXX_COMPILER clang++)
    set(PYBIND11_CPP_STANDARD -std=c++17)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++17 -fPIC -O0 -g -D_FILE_OFFSET_BITS=64 -fvisibility=hidden")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__ANSI__ -fPIC -D_FILE_OFFSET_BITS=64")
    add_compile_definitions(LINUXENV)

ENDIF(WIN32)
message(STATUS "Found pybind11 v${pybind11_VERSION}: ${pybind11_INCLUDE_DIRS}")

SET(pybind11_DIR libs/pybind11)

set(LIBCZI_INCLUDE_DIR libs/libCZIrw/Src/libCZI)
message(STATUS ${LIBCZI_INCLUDE_DIR})

add_compile_definitions(_STATICLIBBUILD)

# no need to build the unit-tests, CZIcmd or the dynamic-library of the libCZI-repo
set(LIBCZI_BUILD_UNITTESTS OFF CACHE  BOOL "" FORCE)
set(LIBCZI_BUILD_DYNLIB OFF CACHE  BOOL "" FORCE)
set(LIBCZI_BUILD_CZICMD OFF CACHE  BOOL "" FORCE)
set(LIBCZI_BUILD_CZICHECK OFF CACHE  BOOL "" FORCE)

add_subdirectory(libs/libCZIrw/)
add_subdirectory(libs/pybind11)

include_directories(libs/libCZIrw/)

find_package(pybind11)
message(STATUS "Found pybind11 v${pybind11_VERSION}: ${pybind11_INCLUDE_DIRS}")

set(PYLIBCZIRW_API_HEADERS _pylibCZIrw/src/api/CZIreadAPI.h  _pylibCZIrw/src/api/PImage.h _pylibCZIrw/src/api/inc_libCzi.h _pylibCZIrw/src/api/CZIwriteAPI.h)

set(PYLIBCZIRW_API_SRC _pylibCZIrw/src/api/CZIreadAPI.cpp  _pylibCZIrw/src/api/PImage.cpp _pylibCZIrw/src/api/DllMain.cpp _pylibCZIrw/src/api/CZIwriteAPI.cpp)

set(PYLIBCZIRW_PYBIND11 _pylibCZIrw/src/bindings/CZIrw.cpp  _pylibCZIrw/src/bindings/PbHelper.h _pylibCZIrw/src/bindings/PbHelper.cpp)

set(TARGET_API _pylibCZIrw_API)
set(TARGET_BINDINGS _pylibCZIrw)
set(TARGET_MEMTEST mem_check)

add_library(${TARGET_API} STATIC ${PYLIBCZIRW_API_SRC} ${PYLIBCZIRW_API_HEADERS})


# that's a hack in order to fix the linker error in debug-builds on Windows -> something like "fatal error LNK1104: cannot open file 'python39_d.lib'"
# Problem seems to be that on Windows, the debug-build uses some pragma's to link to the "_d"-library, but the 'pybind11_add_module'-method below
# does not seem to know about this "_d"-thing. 
# So, the makeshift fix here is: we use a global "link-directory", and point it to the folder where the python-libs are found.
# https://stackoverflow.com/questions/59126760/building-a-python-c-extension-on-windows-with-a-debug-python-installation
IF (WIN32)
  IF(CMAKE_BUILD_TYPE MATCHES Debug)
    get_filename_component(MY_PYTHON_LIB_DIR  ${PYTHON_LIBRARIES} DIRECTORY)     
    #message(STATUS PYTHON_LIB_DIR ${PYTHON_LIB_DIR})

    link_directories(${MY_PYTHON_LIB_DIR})
  ENDIF()
ENDIF()


target_include_directories(${TARGET_API} PRIVATE ${LIBCZI_INCLUDE_DIR})

target_link_libraries(${TARGET_API} INTERFACE libCZIStatic JxrDecodeStatic)

add_dependencies(${TARGET_API} libCZIStatic JxrDecodeStatic)

pybind11_add_module(${TARGET_BINDINGS} MODULE ${PYLIBCZIRW_API_SRC} ${PYLIBCZIRW_PYBIND11})

target_include_directories(${TARGET_BINDINGS} PRIVATE ${LIBCZI_INCLUDE_DIR})

target_link_libraries(${TARGET_BINDINGS} PRIVATE libCZIStatic JxrDecodeStatic)

message(STATUS "Configuration Terminated")

# Lines below are for building the main script which can help debugging c++ execution and add sanity check.
add_executable(${TARGET_MEMTEST} "_pylibCZIrw/tests/main.cpp")
target_include_directories(${TARGET_MEMTEST} PRIVATE ${LIBCZI_INCLUDE_DIR})
target_link_libraries(${TARGET_MEMTEST} PRIVATE libCZIStatic JxrDecodeStatic ${TARGET_API})