#ifndef FORWARD_INDEX_HPP
#define FORWARD_INDEX_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include "json.hpp" 

using json = nlohmann::json;

// Struct to hold temporary document stats with title/body separation.
// Defined here so it can be passed to append_document.
struct WordStats {
    int title_frequency = 0;
    int body_frequency = 0;
    std::vector<int> title_positions;
    std::vector<int> body_positions;
    
    // Get total weighted frequency (title words get 3x weight)
    int get_weighted_frequency() const {
        return title_frequency * 3 + body_frequency;
    }
};

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

    // Appends a single document to the existing file (For Dynamic Addition)
    void append_document(const std::string& output_path, int doc_id, const std::map<int, WordStats>& doc_stats);

private:
    std::map<std::string, int> lexicon_; // Stores frozen WordIDs
    json forward_index_json_;            // Final JSON object
    int total_docs_ = 0;                 // Document counter
};

#endif // FORWARD_INDEX_HPP