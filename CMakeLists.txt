cmake_minimum_required(VERSION 3.27)
project(FlatOutUCFO2Handling)

SET(CMAKE_SYSTEM_NAME Windows)

SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
SET(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
SET(CMAKE_RANLIB i686-w64-mingw32-ranlib)

SET(CMAKE_CXX_STANDARD 20)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -masm=intel -s -static")

add_library(FlatOutUCFO2Handling SHARED main.cpp)
target_include_directories(FlatOutUCFO2Handling PUBLIC ../nya-common)
target_include_directories(FlatOutUCFO2Handling PUBLIC ../nya-common/3rdparty)
target_link_options(FlatOutUCFO2Handling PRIVATE "-Wl,--exclude-all-symbols")
set_target_properties(FlatOutUCFO2Handling PROPERTIES PREFIX "")
set_target_properties(FlatOutUCFO2Handling PROPERTIES SUFFIX "_gcp.asi")