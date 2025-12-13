#include "../include/SearchService.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>

struct SearchResult {
    int doc_id;
    std::string url;
    double score;  // Changed to double for advanced scoring
    int publication_year;  // For secondary sorting
    int cited_by_count;   // For tertiary sorting
};

// Sort by: 1) Final score (desc), 2) Publication year (desc - recent first), 3) Citations (desc)
bool compareResults(const SearchResult& a, const SearchResult& b) {
    if (std::abs(a.score - b.score) > 0.001) {  // Score differs significantly
        return a.score > b.score;
    }
    if (a.publication_year != b.publication_year) {
        return a.publication_year > b.publication_year;  // Recent first
    }
    return a.cited_by_count > b.cited_by_count;  // More citations first
}

SearchService::SearchService() {
    std::cout << "[Engine] Initializing Search Service...\n";
    // Load lexicon with trie (trie is built automatically on load)
    if (!lexicon_trie_.load_from_json("data/processed/lexicon.json")) {
        std::cerr << "[Engine] CRITICAL: Could not load lexicon.json\n";
    } else {
        std::cout << "[Engine] Lexicon loaded: " << lexicon_trie_.size() << " words\n";
        std::cout << "[Engine] Trie built and ready for autocomplete\n";
    }
    if (!doc_url_mapper.load("data/processed/docid_to_url.json")) {
        std::cerr << "[Engine] WARNING: Could not load doc_url_map.json\n";
    }
    
    // Load document metadata for ranking
    if (!document_metadata_.load("data/processed/document_metadata.json")) {
        std::cerr << "[Engine] WARNING: Could not load document_metadata.json\n";
        std::cerr << "[Engine] Run extract_metadata.py to generate metadata file\n";
    } else {
        std::cout << "[Engine] Document metadata loaded: " << document_metadata_.size() << " documents\n";
    }
    
    // Load forward index for title frequency lookups
    load_forward_index();
}

json& SearchService::get_barrel(int barrel_id) {
    if (barrel_cache_.find(barrel_id) == barrel_cache_.end()) {
        std::string path = "data/processed/barrels/inverted_barrel_" + std::to_string(barrel_id) + ".json";
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

void SearchService::load_forward_index() {
    std::ifstream f("data/processed/forward_index.json");
    if (f.is_open()) {
        try {
            f >> forward_index_cache_;
            std::cout << "[Engine] Forward index loaded for ranking\n";
        } catch (const std::exception& e) {
            std::cerr << "[Engine] WARNING: Could not parse forward_index.json: " << e.what() << "\n";
        }
    } else {
        std::cerr << "[Engine] WARNING: Could not open forward_index.json\n";
    }
}

int SearchService::get_title_frequency(int doc_id, int word_id) const {
    if (forward_index_cache_.empty() || !forward_index_cache_.contains("forward_index")) {
        return 0;
    }
    
    try {
        std::string doc_id_str = std::to_string(doc_id);
        std::string word_id_str = std::to_string(word_id);
        
        if (forward_index_cache_["forward_index"].contains(doc_id_str)) {
            auto& doc_data = forward_index_cache_["forward_index"][doc_id_str];
            if (doc_data.contains("words") && doc_data["words"].contains(word_id_str)) {
                auto& word_stats = doc_data["words"][word_id_str];
                if (word_stats.contains("title_frequency")) {
                    return word_stats["title_frequency"].get<int>();
                }
            }
        }
    } catch (const std::exception&) {
        // Silently return 0 if any error
    }
    
    return 0;
}

std::string SearchService::search(std::string query) {
    json response_json;
    response_json["query"] = query;
    response_json["results"] = json::array();

    std::string clean_query;
    for(char c : query) if(!isspace(c)) clean_query += tolower(c);

    int word_id = lexicon_trie_.get_word_index(clean_query);

    if (word_id != -1) {
        int barrel_id = word_id % 100;
        json& barrel = get_barrel(barrel_id);
        std::string id_str = std::to_string(word_id);

        if (barrel.contains(id_str)) {
            auto raw = barrel[id_str];
            std::vector<SearchResult> ranked;
            
            for (auto& entry : raw) {
                int doc_id = entry[0];
                int weighted_freq = entry[1];  // This is weighted_frequency
                std::vector<int> positions;
                
                // Extract positions if available
                if (entry.size() > 2 && entry[2].is_array()) {
                    positions = entry[2].get<std::vector<int>>();
                }
                
                // Get title frequency from forward index
                int title_frequency = get_title_frequency(doc_id, word_id);
                
                // Calculate advanced score using RankingScorer
                ScoreComponents scores = ranking_scorer_.calculate_score(
                    weighted_freq,
                    title_frequency,
                    positions,
                    doc_id,
                    &document_metadata_
                );
                
                // Get metadata for sorting
                int publication_year = document_metadata_.get_publication_year(doc_id);
                int cited_by_count = document_metadata_.get_cited_by_count(doc_id);
                
                std::string url = doc_url_mapper.get(doc_id);
                ranked.push_back({
                    doc_id, 
                    url, 
                    scores.final_score,
                    publication_year,
                    cited_by_count
                });
            }
            
            // Sort by score, then year, then citations
            std::sort(ranked.begin(), ranked.end(), compareResults);

            for (const auto& res : ranked) {
                json item;
                item["docId"] = res.doc_id;
                item["score"] = res.score;
                item["url"] = res.url;
                
                // Add metadata if available
                if (res.publication_year > 0) {
                    item["publication_year"] = res.publication_year;
                }
                if (res.cited_by_count > 0) {
                    item["cited_by_count"] = res.cited_by_count;
                }
                
                response_json["results"].push_back(item);
                
                if (response_json["results"].size() >= 50) break;
            }
        }
    }
    
    return response_json.dump();
}

std::string SearchService::autocomplete(const std::string& prefix, int limit) {
    json response_json;
    response_json["prefix"] = prefix;
    response_json["suggestions"] = json::array();
    
    if (prefix.empty() || limit <= 0) {
        return response_json.dump();
    }
    
    // Clean the prefix (lowercase, remove extra spaces)
    std::string clean_prefix;
    for (char c : prefix) {
        if (!std::isspace(c)) {
            clean_prefix += std::tolower(static_cast<unsigned char>(c));
        }
    }
    
    // Get autocomplete suggestions from trie
    std::vector<std::string> suggestions = lexicon_trie_.autocomplete(clean_prefix, limit);
    
    for (const auto& suggestion : suggestions) {
        response_json["suggestions"].push_back(suggestion);
    }
    
    return response_json.dump();
}