cmake_minimum_required(VERSION 3.14)
project(lexicon_builder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(cpp/include)
include_directories(include)
include_directories(.)

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

# Build search test executable
add_executable(test_search 
    cpp/src/test_search.cpp 
    cpp/src/lexicon.cpp
)

# Build search engine executable
add_executable(search_engine
    src/main.cpp
    src/SearchService.cpp
    cpp/src/lexicon.cpp
)

# Link necessary libraries
if(WIN32)
    target_link_libraries(search_engine ws2_32 crypt32)
else()
    target_link_libraries(search_engine pthread)
endif()