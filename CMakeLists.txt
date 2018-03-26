cmake_minimum_required(VERSION 3.0)
project(protocol)

set(CMAKE_CXX_STANDARD 11)

add_executable(client client.c)
add_executable(server server.c)