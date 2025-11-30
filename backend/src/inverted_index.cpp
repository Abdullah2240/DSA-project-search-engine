#include "inverted_index.hpp"

namespace fs = std::filesystem;

// Constructor which saves the number of barrels we want
InvertedIndexBuilder::InvertedIndexBuilder(int total_barrels) {
    total_barrels_ = total_barrels;
}

// Distribute words evenly across barrels
int InvertedIndexBuilder::get_barrel_id(int word_id) {
    return word_id % total_barrels_;
}

// Main function to build the Inverted Index from a Forward Index
void InvertedIndexBuilder::build(const std::string& forward_index_path, const std::string& output_dir) {
    std::cout << "Loading Forward Index from: " << forward_index_path << std::endl;
    
    // Open the Forward Index file
    std::ifstream f(forward_index_path);
    if (!f.is_open()) {
        std::cerr << "CRITICAL ERROR: Could not open Forward Index!" << std::endl;
        return;
    }

    // Load JSON into RAM
    json forward_json;
    f >> forward_json; 

    // Prepare barrels in memory, each barrel is a map from WordID to list of InvertedEntries
    std::vector<BarrelMap> barrels(total_barrels_);

    // Get the "forward_index" part of the JSON
    auto& fwd_idx = forward_json["forward_index"];
    int total_docs_processed = 0;

    std::cout << "Inverting data..." << std::endl;

    // Loop through every document in the Forward Index
    // item.key() is the DocID string, item.value() is the data
    for (auto& item : fwd_idx.items()) {
        int doc_id = std::stoi(item.key()); // Converts "10" string to 10 int
        json& doc_data = item.value();

        // Checks if this doc has words
        if (doc_data.contains("words")) {

            // Loops through every word in this Document
            for (auto& word_item : doc_data["words"].items()) {
                int word_id = std::stoi(word_item.key());
                json& stats = word_item.value();

                // Creates the entry for this word
                InvertedEntry entry;
                entry.doc_id = doc_id;
                entry.frequency = stats["frequency"];
                
                // Converts JSON array to C++ vector
                entry.positions = stats["positions"].get<std::vector<int>>();

                // Calculates which barrel this word belongs to
                int barrel_index = get_barrel_id(word_id);

                // Adds the entry to that barrel's map
                barrels[barrel_index][word_id].push_back(entry);
            }
        }
        
        // Shows progress every 5000 documents
        total_docs_processed++;
        if (total_docs_processed % 5000 == 0) {
            std::cout << "Processed " << total_docs_processed << " documents..." << std::endl;
        }
    }

    // Saves everything to disk
    std::cout << "Inversion complete. Writing barrels to disk..." << std::endl;

    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }

    for (int i = 0; i < total_barrels_; ++i) {
        // Only save if the barrel has data
        if (!barrels[i].empty()) {
            save_barrel(i, barrels[i], output_dir);
        }
    }
}

// Saves one barrel map to a JSON file
void InvertedIndexBuilder::save_barrel(int barrel_id, const BarrelMap& barrel_data, const std::string& output_dir) {
    json j_barrel;
    
    // Loop through every word in this barrel
    for (auto const& [word_id, entries_list] : barrel_data) {
        json j_entries = json::array();

        // Loop through every document that contained this word
        for (const auto& entry : entries_list) {
            // Save as a simple List [DocID, Frequency, Positions]

            j_entries.push_back({
                entry.doc_id,
                entry.frequency,
                entry.positions
            });
        }

        // Add to barrel JSON
        j_barrel[std::to_string(word_id)] = j_entries;
    }

    // Write to file
    std::string filename = output_dir + "/inverted_barrel_" + std::to_string(barrel_id) + ".json";
    std::ofstream out(filename);
    out << j_barrel.dump(-1);
    
    std::cout << "Saved Barrel " << barrel_id << " (" << barrel_data.size() << " unique words)" << std::endl;
}