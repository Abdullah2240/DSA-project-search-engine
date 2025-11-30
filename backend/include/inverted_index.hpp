#ifndef INVERTED_INDEX_HPP
#define INVERTED_INDEX_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "json.hpp" 

using json = nlohmann::json;

// Single entry in the inverted index
struct InvertedEntry {
    int doc_id;
    int frequency; // How many times the word appears
    std::vector<int> positions; // Where the word appears
};

// Using alias for clarity
// A Barrel is a Map: Word ID maps to List of Entries i.e. Docs containing that word
using BarrelMap = std::map<int, std::vector<InvertedEntry>>;

class InvertedIndexBuilder {
public:
    InvertedIndexBuilder(int total_barrels);

    void build(const std::string& forward_index_path, const std::string& output_dir);

private:
    int total_barrels_;

    // Decides which barrel a word goes into
    int get_barrel_id(int word_id);

    // Saves one barrel to a file
    void save_barrel(int barrel_id, const BarrelMap& barrel_data, const std::string& output_dir);
};

#endif // INVERTED_INDEX_HPP