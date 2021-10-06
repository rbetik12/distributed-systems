cmake_minimum_required(VERSION 3.20)
project(lab1 C)

set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS}")

add_executable(lab1 Main.c common.h ipc.h pa1.h ipc.c IOMisc.h Log.c IO.c)