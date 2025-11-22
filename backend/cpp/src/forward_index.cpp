#include "forward_index.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// --- Helper Tokenizer (Matches the logic in engine.cpp) ---
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string clean_text;
    
    // Simple cleaning: replace non-alphanumeric with space
    for (char c : text) {
        if (std::isalnum(c) || std::isspace(c)) {
            clean_text += std::tolower(c);
        } else {
            clean_text += ' ';
        }
    }

    std::stringstream ss(clean_text);
    std::string word;
    while (ss >> word) {
        if (word.length() > 0) {
            tokens.push_back(word);
        }
    }
    return tokens;
}
// ---------------------------------------------------------

ForwardIndexBuilder::ForwardIndexBuilder() {}

bool ForwardIndexBuilder::load_lexicon(const std::string& filepath) {
    std::cout << "Loading lexicon from: " << filepath << std::endl;
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "ERROR: Could not open lexicon file at " << filepath << std::endl;
        return false;
    }
    try {
        json j;
        f >> j;
        
        // Handle both formats (direct map or nested under "word_to_index")
        if (j.contains("word_to_index")) {
            for (auto& element : j["word_to_index"].items()) {
                lexicon_[element.key()] = element.value();
            }
        } else {
            for (auto& element : j.items()) {
                lexicon_[element.key()] = element.value();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
    std::cout << "Lexicon loaded. Words: " << lexicon_.size() << std::endl;
    return true;
}

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

            // OPTION A: If dataset is already tokenized (has "tokens" list)
            if (doc_obj.contains("tokens")) {
                tokens = doc_obj["tokens"].get<std::vector<std::string>>();
            } 
            // OPTION B: Raw text (Title + Abstract)
            else {
                std::string text = "";
                if (doc_obj.contains("title") && !doc_obj["title"].is_null()) 
                    text += doc_obj["title"].get<std::string>() + " ";
                if (doc_obj.contains("abstract") && !doc_obj["abstract"].is_null()) 
                    text += doc_obj["abstract"].get<std::string>();
                tokens = tokenize(text);
            }

            // Process Tokens: Filter & Count
            std::vector<int> doc_word_ids;
            std::map<std::string, int> tf_counts;

            for (const auto& token : tokens) {
                // Check if word is in the FROZEN LEXICON
                if (lexicon_.count(token)) {
                    int id = lexicon_[token];
                    doc_word_ids.push_back(id);
                    
                    // Count frequency (TF)
                    tf_counts[std::to_string(id)]++;
                }
            }

            // Store Hybrid Structure
            if (!doc_word_ids.empty()) {
                inner_map[std::to_string(doc_int_id)] = {
                    {"words", doc_word_ids},    // Keeps Order
                    {"frequency", tf_counts}    // Keeps Counts
                };
            }
            
            doc_int_id++;
            if (doc_int_id % 5000 == 0) std::cout << "Processed " << doc_int_id << "..." << std::endl;

        } catch (const std::exception& e) { continue; }
    }
    
    forward_index_json_["forward_index"] = inner_map;
    forward_index_json_["total_documents"] = doc_int_id;
}

void ForwardIndexBuilder::save_to_file(const std::string& output_path) {
    std::ofstream outfile(output_path);
    if (outfile.is_open()) {
        // dump(-1) compacts JSON to save space
        outfile << forward_index_json_.dump(-1); 
        std::cout << "Saved to " << output_path << std::endl;
    } else {
        std::cerr << "Error saving file." << std::endl;
    }
}