cmake_minimum_required(VERSION 3.20)
project(lab1 C)

set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Werror -Wall -Wextra -pedantic")

add_executable(lab1 pa1/Main.c pa1/common.h pa1/ipc.h pa1/pa1.h pa1/ipc.c pa1/IOMisc.h pa1/Log.c pa1/IO.c)

project(lab2 C)

set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Werror -Wall -Wextra -pedantic")

add_executable(lab2 pa2/pa23.c pa2/common.h pa2/ipc.h pa2/pa2345.h pa2/ipc.c pa2/IOMisc.h pa2/Log.c pa2/IO.c pa2/bank_robbery.c
        pa2/banking.c pa2/banking.h)