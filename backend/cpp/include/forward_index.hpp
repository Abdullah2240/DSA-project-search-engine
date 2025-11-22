#ifndef FORWARD_INDEX_HPP
#define FORWARD_INDEX_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include "json.hpp" 

using json = nlohmann::json;

class ForwardIndexBuilder {
public:
    ForwardIndexBuilder();
    
    // Loads the "frozen" lexicon.json
    bool load_lexicon(const std::string& filepath);

    // Reads dataset, builds index with BOTH list and counts (Hybrid)
    void build_index(const std::string& dataset_path);

    // Saves the final JSON output
    void save_to_file(const std::string& output_path);

private:
    std::map<std::string, int> lexicon_;
    json forward_index_json_; 
    int total_docs_ = 0;
};

#endif // FORWARD_INDEX_HPP