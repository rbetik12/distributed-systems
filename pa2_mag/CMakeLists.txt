cmake_minimum_required(VERSION 3.17)
project(pa2_mag C)

set(CMAKE_C_COMPILER "clang")

add_executable(pa2 
common.h
ipc.c
ipc.h
IPCWrapper.h
Log.c
pa2.c
pa2345.h
Utils.c
Utils.h
)

target_compile_options(pa2 PRIVATE -Wall -Werror -pedantic -std=c99)
target_link_libraries(pa2 ${CMAKE_CURRENT_SOURCE_DIR}/libruntime.so)

