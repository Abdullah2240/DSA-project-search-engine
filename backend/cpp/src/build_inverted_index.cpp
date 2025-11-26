#include "inverted_index.hpp"
#include <iostream>

int main() {
    // Forward Index path
    const std::string FORWARD_INDEX_PATH = "backend/data/processed/forward_index.json";

    // The directory where we save the 10 barrel files
    const std::string OUTPUT_DIR = "backend/data/processed/barrels";

    // Number of barrels to create
    const int NUM_BARRELS = 10; 

    std::cout << "Starting Inverted Index Build" << std::endl;
    std::cout << "Target Barrels: " << NUM_BARRELS << std::endl;

    // Build the Inverted Index
    InvertedIndexBuilder builder(NUM_BARRELS);
    builder.build(FORWARD_INDEX_PATH, OUTPUT_DIR);

    std::cout << "Build Complete" << std::endl;
    return 0;
}