cmake_minimum_required(VERSION 3.16)
project(llgo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------
# Static library (unity build: one TU)
# -------------------------------------------------------
add_library(llgo STATIC src/llgo.cpp)
target_include_directories(llgo PUBLIC src)
target_compile_options(llgo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers>)