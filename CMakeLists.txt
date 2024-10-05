cmake_minimum_required(VERSION 3.30)

project(simplezip VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(simplezip sources/main.cpp sources/simplezip.cpp)
