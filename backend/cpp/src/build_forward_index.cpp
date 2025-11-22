#include "forward_index.hpp"
#include <iostream>

int main() {
    // Configuration paths
    const std::string LEXICON_PATH = "backend/data/processed/lexicon.json";
    const std::string OUTPUT_PATH  = "backend/data/processed/forward_index.json";
    const std::string DATASET_PATH = "backend/data/processed/cleaned.jsonl"; 

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