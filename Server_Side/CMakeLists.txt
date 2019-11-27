cmake_minimum_required(VERSION 3.10)
project(Server_Side C)


set(CMAKE_C_STANDARD 99)

include_directories(../Client_Side)
add_executable(Server_Side HTTPServer.c )
