#include "forward_index.hpp"
#include <iostream>

int main() {
    // === CONFIGURATION (Teammate will edit these) ===
    
    // 1. Where is the Lexicon?
    const std::string LEXICON_PATH = "backend/data/processed/lexicon.json";
    
    // 2. Where should we save the result?
    const std::string OUTPUT_PATH  = "backend/data/processed/forward_index.json";
    
    // 3. Where is the dataset? 
    // IMPORTANT: Change this to YOUR local path when testing
    const std::string DATASET_PATH = "backend/data/processed/cleaned.jsonl"; 

    std::cout << "--- Starting Forward Index Build ---" << std::endl;

    ForwardIndexBuilder builder;

    if (!builder.load_lexicon(LEXICON_PATH)) {
        return 1;
    }

    builder.build_index(DATASET_PATH);
    builder.save_to_file(OUTPUT_PATH);

    return 0;
}