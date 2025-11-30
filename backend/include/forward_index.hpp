#ifndef FORWARD_INDEX_HPP
#define FORWARD_INDEX_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include "json.hpp" 

using json = nlohmann::json;

// Class to handle building the Forward Index from processed data
class ForwardIndexBuilder {
public:
    ForwardIndexBuilder();
    
    // Loads the frozen lexicon (word -> id mapping)
    bool load_lexicon(const std::string& filepath);

    // Main logic: Reads dataset, calculates TF, positions, and doc length
    void build_index(const std::string& dataset_path);

    // Saves the resulting JSON to disk (compact format)
    void save_to_file(const std::string& output_path);

private:
    std::map<std::string, int> lexicon_; // Stores frozen WordIDs
    json forward_index_json_;            // Final JSON object
    int total_docs_ = 0;                 // Document counter
};

#endif // FORWARD_INDEX_HPP