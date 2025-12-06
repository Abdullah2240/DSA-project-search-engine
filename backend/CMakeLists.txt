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
# Build test_autocomplete executable
# ----------------------------
add_executable(test_autocomplete 
    src/test_autocomplete.cpp 
    src/lexicon.cpp
    src/Trie.cpp
    src/LexiconWithTrie.cpp
)

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

# ----------------------------
# Custom target to generate data file (if needed)
# ----------------------------
find_program(PYTHON_EXECUTABLE python python3)
if(PYTHON_EXECUTABLE)
    add_custom_target(generate_data
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/data/raw/getData.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating cleaned.jsonl data file from OpenAlex API"
    )
else()
    message(WARNING "Python not found. Cannot generate data automatically.")
endif()

# ----------------------------
# Custom target to run all build processes in sequence
# This builds doc_id to URL map, lexicon, forward index, and inverted index
# Note: search_engine is a server that runs indefinitely, so run it separately
# ----------------------------
if(PYTHON_EXECUTABLE)
    add_custom_target(run_all
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_docid_to_url.py
        COMMAND $<TARGET_FILE:build_lexicon>
        COMMAND $<TARGET_FILE:build_forward_index>
        COMMAND $<TARGET_FILE:build_inverted_index>
        DEPENDS build_lexicon build_forward_index build_inverted_index
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building doc_id to URL map, lexicon, forward index, and inverted index"
    )
else()
    add_custom_target(run_all
        COMMAND $<TARGET_FILE:build_lexicon>
        COMMAND $<TARGET_FILE:build_forward_index>
        COMMAND $<TARGET_FILE:build_inverted_index>
        DEPENDS build_lexicon build_forward_index build_inverted_index
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running all build processes: lexicon, forward index, and inverted index"
    )
    message(WARNING "Python not found. Cannot build doc_id to URL map automatically.")
endif()

# ----------------------------
# Custom target to build everything: generate data, build doc_id to URL map, then build all indices
# ----------------------------
if(PYTHON_EXECUTABLE)
    add_custom_target(build_all
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/data/raw/getData.py
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_docid_to_url.py
        COMMAND $<TARGET_FILE:build_lexicon>
        COMMAND $<TARGET_FILE:build_forward_index>
        COMMAND $<TARGET_FILE:build_inverted_index>
        DEPENDS build_lexicon build_forward_index build_inverted_index
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating data, building doc_id to URL map, and building all indices: lexicon, forward index, and inverted index"
    )
endif()