cmake_minimum_required(VERSION 3.16)
project(llgo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(llgo STATIC
    src/llgo.cpp
    src/llgo_api.hpp
    src/frontend.hpp
)

target_include_directories(llgo PUBLIC src)
