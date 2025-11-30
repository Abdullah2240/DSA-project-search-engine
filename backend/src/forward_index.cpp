#include "forward_index.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map> 

// Helper tokenizer, cleans text and splits into tokens
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string clean_text;
    
    // Lowercase and remove non-alphanumeric chars
    for (char c : text) {
        if (std::isalnum(c) || std::isspace(c)) {
            clean_text += std::tolower(c);
        } else {
            clean_text += ' ';
        }
    }

    // Split by whitespace
    std::stringstream ss(clean_text);
    std::string word;
    while (ss >> word) {
        if (!word.empty()) tokens.push_back(word);
    }
    return tokens;
}

// Struct to hold temporary document stats
struct WordStats {
    int frequency = 0;
    std::vector<int> positions;
};

ForwardIndexBuilder::ForwardIndexBuilder() {}

// Load the frozen lexicon to ensure consistent WordIDs
bool ForwardIndexBuilder::load_lexicon(const std::string& filepath) {
    std::cout << "Loading lexicon from: " << filepath << std::endl;
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "ERROR: Could not open lexicon file." << std::endl;
        return false;
    }
    try {
        json j;
        f >> j;
        // Handle both simple and nested JSON formats
        if (j.contains("word_to_index")) {
            for (auto& el : j["word_to_index"].items()) lexicon_[el.key()] = el.value();
        } else {
            for (auto& el : j.items()) lexicon_[el.key()] = el.value();
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON error: " << e.what() << std::endl;
        return false;
    }
    std::cout << "Lexicon loaded. Size: " << lexicon_.size() << std::endl;
    return true;
}

// Reads dataset line-by-line and builds the index
void ForwardIndexBuilder::build_index(const std::string& dataset_path) {
    std::cout << "Reading dataset: " << dataset_path << std::endl;
    std::ifstream dataset(dataset_path);
    if (!dataset.is_open()) {
        std::cerr << "CRITICAL: Could not open dataset!" << std::endl;
        return;
    }

    std::string line;
    json inner_map; 
    int doc_int_id = 0;

    while (std::getline(dataset, line)) {
        try {
            auto doc_obj = json::parse(line);
            std::vector<std::string> tokens;

            // Use pre-tokenized list if available, else tokenize raw text
            if (doc_obj.contains("tokens")) {
                tokens = doc_obj["tokens"].get<std::vector<std::string>>();
            } else {
                std::string text = "";
                if (doc_obj.contains("title") && !doc_obj["title"].is_null()) 
                    text += doc_obj["title"].get<std::string>() + " ";
                if (doc_obj.contains("abstract") && !doc_obj["abstract"].is_null()) 
                    text += doc_obj["abstract"].get<std::string>();
                tokens = tokenize(text);
            }

            // Map automatically sorts keys by WordID (0, 1, 2...)
            std::map<int, WordStats> doc_stats;
            int current_pos = 0;

            // Filter tokens against Lexicon and collect stats
            for (const auto& token : tokens) {
                // Ensure token is lowercase (lexicon stores lowercase words)
                std::string lower_token = token;
                std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                             [](unsigned char c) { return std::tolower(c); });
                
                if (lexicon_.count(lower_token)) {
                    int id = lexicon_[lower_token];
                    doc_stats[id].frequency++;
                    doc_stats[id].positions.push_back(current_pos);
                }
                current_pos++;
            }

            // Only store document if it contains valid words
            if (!doc_stats.empty()) {
                json doc_json;
                doc_json["doc_length"] = tokens.size(); // Critical for BM25

                json words_obj;
                for (const auto& [id, stats] : doc_stats) {
                    words_obj[std::to_string(id)] = {
                        {"frequency", stats.frequency},
                        {"positions", stats.positions}
                    };
                }
                doc_json["words"] = words_obj;
                inner_map[std::to_string(doc_int_id)] = doc_json;
            }
            
            doc_int_id++;
            if (doc_int_id % 5000 == 0) std::cout << "Processed " << doc_int_id << " docs..." << std::endl;

        } catch (const std::exception&) { continue; }
    }
    
    forward_index_json_["forward_index"] = inner_map;
    forward_index_json_["total_documents"] = doc_int_id;
}

// Save final JSON to disk
void ForwardIndexBuilder::save_to_file(const std::string& output_path) {
    std::ofstream outfile(output_path);
    if (outfile.is_open()) {
        // Use compact mode (-1) to save disk space
        outfile << forward_index_json_.dump(-1); 
        std::cout << "Saved to " << output_path << std::endl;
    } else {
        std::cerr << "Error saving file." << std::endl;
    }
}