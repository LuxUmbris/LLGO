cmake_minimum_required(VERSION 3.16)
project(llgo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> rcs <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH "")
endif()

add_library(llgo STATIC src/llgo.cpp)
target_include_directories(llgo PUBLIC src)
