cmake_minimum_required(VERSION 4.0)
project(CPU_Emulator C)

set(CMAKE_C_STANDARD 17)

add_executable(CPU_Emulator CPU.c
        CPU.h)
