cmake_minimum_required(VERSION 3.14)
project(lexicon_builder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(cpp/include)

# Build lexicon executable
add_executable(build_lexicon 
    cpp/src/build_lexicon.cpp
    cpp/src/lexicon.cpp
)

# Build Python module (if pybind11 is available)
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(engine
        cpp/src/bindings.cpp
        cpp/src/engine.cpp
        cpp/src/lexicon.cpp
    )
    message(STATUS "Python module 'engine' will be built")
else()
    message(STATUS "pybind11 not found - Python module will not be built")
    message(STATUS "Install pybind11: pip install pybind11")
endif()
