cmake_minimum_required(VERSION 3.14)
project(dsa_search_engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(/include)
include_directories(include)
include_directories(src)

# ----------------------------
# Build lexicon executable
# ----------------------------
add_executable(build_lexicon 
    src/build_lexicon.cpp
    src/lexicon.cpp
)

# ----------------------------
# Build forward index executable
# ----------------------------
add_executable(build_forward_index 
    src/build_forward_index.cpp 
    src/forward_index.cpp 
)

# ----------------------------
# Build inverted index executable
# ----------------------------
add_executable(build_inverted_index 
    src/build_inverted_index.cpp 
    src/inverted_index.cpp
)

# ----------------------------
# Build doc_url_mapper as a library
# ----------------------------
add_library(doc_url_mapper STATIC
    src/doc_url_mapper.cpp
    include/doc_url_mapper.hpp
)

# ----------------------------
# Build test_search executable
# ----------------------------
add_executable(test_search 
    src/test_search.cpp 
    src/lexicon.cpp
)
target_link_libraries(test_search doc_url_mapper)

# ----------------------------
# Build search_engine executable
# ----------------------------
add_executable(search_engine
    src/main.cpp
    src/SearchService.cpp
    src/lexicon.cpp
)
target_link_libraries(search_engine doc_url_mapper)

# ----------------------------
# Link platform libraries
# ----------------------------
if(WIN32)
    target_link_libraries(search_engine ws2_32 crypt32)
else()
    target_link_libraries(search_engine pthread)
endif()
