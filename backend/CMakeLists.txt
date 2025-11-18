cmake_minimum_required(VERSION 3.14)
project(backend_example)

# Include pybind11 subdirectory
add_subdirectory(pybind11)

# Build a Python module from your C++ code
pybind11_add_module(example cpp/example.cpp)
