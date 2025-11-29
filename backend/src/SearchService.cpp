#include "../../include/SearchService.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>

struct SearchResult {
    int doc_id;
    int score;
};

// Sort by Score TF Descending
bool compareResults(const SearchResult& a, const SearchResult& b) {
    return a.score > b.score;
}

SearchService::SearchService() {
    std::cout << "[Engine] Initializing Search Service...\n";
    // Ensure this path matches where lexicon.json actually is
    if (!lexicon_.load_from_json("backend/data/processed/lexicon.json")) {
        std::cerr << "[Engine] CRITICAL: Could not load lexicon.json\n";
    }
}

json& SearchService::get_barrel(int barrel_id) {
    if (barrel_cache_.find(barrel_id) == barrel_cache_.end()) {
        std::string path = "backend/data/processed/barrels/inverted_barrel_" + std::to_string(barrel_id) + ".json";
        std::ifstream f(path);
        json j;
        if (f.is_open()) {
            f >> j;
            barrel_cache_[barrel_id] = j;
        } else {
            // Prevent crash if file missing
            barrel_cache_[barrel_id] = json::object();
        }
    }
    return barrel_cache_[barrel_id];
}

std::string SearchService::search(std::string query) {
    json response_json;
    response_json["query"] = query;
    response_json["results"] = json::array();

    std::string clean_query;
    for(char c : query) if(!isspace(c)) clean_query += tolower(c);

    int word_id = lexicon_.get_word_index(clean_query);

    if (word_id != -1) {
        int barrel_id = word_id % 100;
        json& barrel = get_barrel(barrel_id);
        std::string id_str = std::to_string(word_id);

        if (barrel.contains(id_str)) {
            auto raw = barrel[id_str];
            std::vector<SearchResult> ranked;
            
            for (auto& entry : raw) {
                // Entry format: [DocID, Frequency, [Positions]]
                ranked.push_back({entry[0], entry[1]});
            }
            std::sort(ranked.begin(), ranked.end(), compareResults);

            for (const auto& res : ranked) {
                json item;
                item["docId"] = res.doc_id;
                item["score"] = res.score;
                response_json["results"].push_back(item);
                
                if (response_json["results"].size() >= 50) break;
            }
        }
    }
    
    return response_json.dump();
}