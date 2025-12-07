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

// Struct to hold temporary document stats with title/body separation
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
            
            // Extract title and body tokens separately
            std::vector<std::string> title_tokens;
            std::vector<std::string> body_tokens;
            
            // Handle pre-tokenized format with title_tokens and body_tokens
            if (doc_obj.contains("title_tokens") && doc_obj.contains("body_tokens")) {
                title_tokens = doc_obj["title_tokens"].get<std::vector<std::string>>();
                body_tokens = doc_obj["body_tokens"].get<std::vector<std::string>>();
            } else if (doc_obj.contains("tokens")) {
                // Legacy format: all tokens together (treat as body)
                body_tokens = doc_obj["tokens"].get<std::vector<std::string>>();
            } else {
                // Tokenize from raw text fields
                if (doc_obj.contains("title") && !doc_obj["title"].is_null()) {
                    std::string title = doc_obj["title"].get<std::string>();
                    title_tokens = tokenize(title);
                }
                if (doc_obj.contains("body") && !doc_obj["body"].is_null()) {
                    std::string body = doc_obj["body"].get<std::string>();
                    body_tokens = tokenize(body);
                } else if (doc_obj.contains("abstract") && !doc_obj["abstract"].is_null()) {
                    std::string abstract = doc_obj["abstract"].get<std::string>();
                    body_tokens = tokenize(abstract);
                }
            }

            // Map automatically sorts keys by WordID (0, 1, 2...)
            std::map<int, WordStats> doc_stats;
            int title_pos = 0;
            int body_pos = 0;

            // Process title tokens
            for (const auto& token : title_tokens) {
                std::string lower_token = token;
                std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                             [](unsigned char c) { return std::tolower(c); });
                
                if (lexicon_.count(lower_token)) {
                    int id = lexicon_[lower_token];
                    doc_stats[id].title_frequency++;
                    doc_stats[id].title_positions.push_back(title_pos);
                }
                title_pos++;
            }

            // Process body tokens
            for (const auto& token : body_tokens) {
                std::string lower_token = token;
                std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                             [](unsigned char c) { return std::tolower(c); });
                
                if (lexicon_.count(lower_token)) {
                    int id = lexicon_[lower_token];
                    doc_stats[id].body_frequency++;
                    doc_stats[id].body_positions.push_back(body_pos);
                }
                body_pos++;
            }

            // Only store document if it contains valid words
            if (!doc_stats.empty()) {
                json doc_json;
                int total_tokens = title_tokens.size() + body_tokens.size();
                doc_json["doc_length"] = total_tokens; // Critical for BM25
                doc_json["title_length"] = title_tokens.size();
                doc_json["body_length"] = body_tokens.size();

                json words_obj;
                for (const auto& [id, stats] : doc_stats) {
                    words_obj[std::to_string(id)] = {
                        {"title_frequency", stats.title_frequency},
                        {"body_frequency", stats.body_frequency},
                        {"weighted_frequency", stats.get_weighted_frequency()},
                        {"title_positions", stats.title_positions},
                        {"body_positions", stats.body_positions}
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