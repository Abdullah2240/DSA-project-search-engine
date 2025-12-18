#include "inverted_index.hpp"
#include <filesystem>

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

    // RAM OPTIMIZATION: Process Line-by-Line (JSONL)
    // We do NOT load the whole JSON into RAM anymore.
    
    std::vector<BarrelMap> barrels(total_barrels_);
    std::string line;
    int total_docs_processed = 0;

    std::cout << "Inverting data..." << std::endl;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        
        try {
            // Parse one line (one document)
            auto doc_line = json::parse(line);
            
            // Format: {"doc_id": "10", "data": {...}}
            std::string doc_id_str = doc_line["doc_id"].get<std::string>();
            int doc_id = std::stoi(doc_id_str);
            json& doc_data = doc_line["data"];

            if (doc_data.contains("words")) {
                for (auto& word_item : doc_data["words"].items()) {
                    int word_id = std::stoi(word_item.key());
                    json& stats = word_item.value();

                    InvertedEntry entry;
                    entry.doc_id = doc_id;
                    
                    if (stats.contains("weighted_frequency")) {
                        entry.frequency = stats["weighted_frequency"].get<int>();
                    } else if (stats.contains("frequency")) {
                        entry.frequency = stats["frequency"].get<int>();
                    } else {
                        // Fallback calc
                        int title_freq = stats.contains("title_frequency") ? stats["title_frequency"].get<int>() : 0;
                        int body_freq = stats.contains("body_frequency") ? stats["body_frequency"].get<int>() : 0;
                        entry.frequency = title_freq * 3 + body_freq;
                    }

                    // Collect positions
                    std::vector<int> all_positions;
                    if (stats.contains("title_positions")) {
                        auto tp = stats["title_positions"].get<std::vector<int>>();
                        all_positions.insert(all_positions.end(), tp.begin(), tp.end());
                    }
                    if (stats.contains("body_positions")) {
                        auto bp = stats["body_positions"].get<std::vector<int>>();
                        all_positions.insert(all_positions.end(), bp.begin(), bp.end());
                    }
                    // Fallback
                    if (all_positions.empty() && stats.contains("positions")) {
                        all_positions = stats["positions"].get<std::vector<int>>();
                    }
                    entry.positions = all_positions;

                    // Add to appropriate barrel in memory
                    barrels[get_barrel_id(word_id)][word_id].push_back(entry);
                }
            }
            
            total_docs_processed++;
            if (total_docs_processed % 5000 == 0) {
                std::cout << "Processed " << total_docs_processed << " documents..." << std::endl;
            }

        } catch (const std::exception& e) {
            // Skip malformed lines without crashing
            continue; 
        }
    }

    // Saves everything to disk
    std::cout << "Inversion complete. Writing barrels to disk..." << std::endl;

    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }

    for (int i = 0; i < total_barrels_; ++i) {
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

// Add a single document to the Delta Barrel (for Dynamic Uploads)
void InvertedIndexBuilder::update_delta_barrel(int doc_id, const std::map<int, WordStats>& doc_stats) {
    std::string delta_path = "data/processed/barrels/inverted_delta.json";
    json delta_json;

    // 1. Load existing Delta (if exists)
    std::ifstream in(delta_path);
    if (in.good()) {
        try { in >> delta_json; } catch(...) {}
    }
    in.close();

    // 2. Add new entries
    for (const auto& [word_id, stats] : doc_stats) {
        std::string w_id_str = std::to_string(word_id);
        
        json entry = json::array({
            doc_id,
            stats.get_weighted_frequency(),
            stats.title_positions // merging positions logic if needed
        });

        if (delta_json.contains(w_id_str)) {
            delta_json[w_id_str].push_back(entry);
        } else {
            delta_json[w_id_str] = json::array({ entry });
        }
    }

    // 3. Save Delta
    std::ofstream out(delta_path);
    out << delta_json.dump(-1);
    std::cout << "[InvertedIndex] Updated Delta Barrel for doc " << doc_id << std::endl;
}

// NEW: End-of-Day Merge
void InvertedIndexBuilder::merge_delta_to_main(const std::string& output_dir) {
    std::string delta_path = output_dir + "/inverted_delta.json";
    std::ifstream in(delta_path);
    if (!in.good()) return;

    std::cout << "[Maintenance] Merging Delta Barrel into Main Barrels...\n";
    json delta_json;
    in >> delta_json;
    in.close();

    // Group updates by Barrel ID to minimize disk I/O
    std::map<int, std::map<std::string, json>> updates_by_barrel;
    for (auto& [word_id_str, new_entries] : delta_json.items()) {
        int word_id = std::stoi(word_id_str);
        int barrel_id = get_barrel_id(word_id);
        updates_by_barrel[barrel_id][word_id_str] = new_entries;
    }

    // Process each affected barrel
    for (auto& [barrel_id, updates] : updates_by_barrel) {
        std::string barrel_path = output_dir + "/inverted_barrel_" + std::to_string(barrel_id) + ".json";
        json main_barrel;
        
        std::ifstream bf(barrel_path);
        if (bf.good()) { try { bf >> main_barrel; } catch(...) {} }
        bf.close();

        for (auto& [w_id, entries] : updates) {
            if (main_barrel.contains(w_id)) {
                for(auto& e : entries) main_barrel[w_id].push_back(e);
            } else {
                main_barrel[w_id] = entries;
            }
        }

        std::ofstream out(barrel_path);
        out << main_barrel.dump(-1);
        std::cout << "  Merged updates into Barrel " << barrel_id << "\n";
    }

    // Clear Delta File
    std::ofstream clear(delta_path);
    clear << "{}";
    std::cout << "[Maintenance] Merge Complete. Delta barrel cleared.\n";
}