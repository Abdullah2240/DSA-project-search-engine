#include "forward_index.hpp"
#include <iostream>
#include <fstream>

int main() {
    // Configuration paths
    const std::string LEXICON_PATH = "data/processed/lexicon.json";
    const std::string OUTPUT_PATH = "data/processed/forward_index.jsonl";
    
    std::string DATASET_PATH = "data/processed/test.jsonl";
    std::ifstream test_file(DATASET_PATH);
    if (!test_file.good()) {
        DATASET_PATH = "data/processed/test.jsonl";
    }
    test_file.close(); 

    std::cout << "--- Starting Forward Index Build ---" << std::endl;

    ForwardIndexBuilder builder;

    // 1. Load Frozen Lexicon
    if (!builder.load_lexicon(LEXICON_PATH)) {
        return 1;
    }

    // 2. Build Index (Calculates TF, Positions, Doc Length)
    builder.build_index(DATASET_PATH);

    // 3. Save Result
    builder.save_to_file(OUTPUT_PATH);

    return 0;
}