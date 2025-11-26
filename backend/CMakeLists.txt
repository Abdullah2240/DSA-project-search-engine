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

# Build forward index executable
add_executable(build_forward_index 
    cpp/src/build_forward_index.cpp 
    cpp/src/forward_index.cpp 
)

# Build inverted index executable
add_executable(build_inverted_index 
    cpp/src/build_inverted_index.cpp 
    cpp/src/inverted_index.cpp
)