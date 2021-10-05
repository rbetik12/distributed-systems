cmake_minimum_required(VERSION 3.20)
project(lab1 C)

set(CMAKE_C_STANDARD 99)

add_executable(lab1 main.c common.h ipc.h pa1.h ipc.c IOMisc.h log.c)