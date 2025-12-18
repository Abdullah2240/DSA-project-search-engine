#include "../include/SearchService.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>
#include <sstream>

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

//Split query into words
std::vector<std::string> split_query(const std::string& str) {
    std::vector<std::string> words;
    std::stringstream ss(str);
    std::string word;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
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
    
    // build map instead of loading full forward index
    build_offset_map();

    //load delta index
    load_delta_index();
}

// Map builder for forward index offsets
void SearchService::build_offset_map() {
    std::cout << "[Engine] Building Forward Index Offset Map...\n";
    // NOTE: We are now reading the .jsonl file!
    std::ifstream f("data/processed/forward_index.jsonl"); 
    if (!f.is_open()) {
        std::cerr << "[Engine] WARNING: Could not open forward_index.jsonl\n";
        return;
    }

    std::string line;
    std::streampos current_offset = 0; 

    while (std::getline(f, line)) {
        if (line.empty()) {
            current_offset = f.tellg(); 
            continue;
        }

        try {
            // Parse ONLY the ID to save RAM
            size_t id_pos = line.find("\"doc_id\"");
            if (id_pos != std::string::npos) {
                size_t start_quote = line.find("\"", id_pos + 9);
                size_t end_quote = line.find("\"", start_quote + 1);
                std::string id_str = line.substr(start_quote + 1, end_quote - start_quote - 1);
                int doc_id = std::stoi(id_str);
                
                doc_offsets_[doc_id] = current_offset;
            }
        } catch (...) {}
        
        current_offset = f.tellg(); 
    }
    std::cout << "[Engine] Mapped " << doc_offsets_.size() << " documents.\n";
}

// delta index loader
void SearchService::load_delta_index() {
    std::ifstream f("data/processed/barrels/inverted_delta.json");
    if (!f.good()) return; 

    try {
        json j;
        f >> j;
        for (auto& item : j.items()) {
            int word_id = std::stoi(item.key());
            std::vector<DeltaEntry> entries;
            for (auto& elem : item.value()) {
                entries.push_back({
                    elem[0].get<int>(), // doc_id
                    elem[1].get<int>(), // freq
                    elem[2].get<std::vector<int>>() // positions
                });
            }
            delta_index_[word_id] = entries;
        }
        std::cout << "[Engine] Delta Index loaded for dynamic content.\n";
    } catch (...) {}
}

// disk fetcher for forward index
json SearchService::get_document_from_disk(int doc_id) {
    if (doc_offsets_.find(doc_id) == doc_offsets_.end()) return nullptr;

    std::ifstream f("data/processed/forward_index.jsonl");
    f.seekg(doc_offsets_[doc_id]); 

    std::string line;
    if (std::getline(f, line)) {
        try {
            return json::parse(line);
        } catch (...) {}
    }
    return nullptr;
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

int SearchService::get_title_frequency(int doc_id, int word_id) {
    json doc = get_document_from_disk(doc_id);
    if (doc == nullptr) return 0;
    
    try {
        std::string word_id_str = std::to_string(word_id);
        if (doc["data"]["words"].contains(word_id_str)) {
            return doc["data"]["words"][word_id_str]["title_frequency"].get<int>();
        }
    } catch (...) {}
    return 0;
}

int SearchService::get_document_length(int doc_id) {
    json doc = get_document_from_disk(doc_id);
    if (doc == nullptr) return 0;
    
    try {
        return doc["data"]["doc_length"].get<int>();
    } catch (...) {}
    return 0;
}

std::string SearchService::search(std::string query) {
    json response_json;
    response_json["query"] = query;
    response_json["results"] = json::array();

    // 1. Clean and Split Query
    std::string clean_query_str;
    for(char c : query) {
        if(std::isalnum(static_cast<unsigned char>(c))) {
            clean_query_str += std::tolower(static_cast<unsigned char>(c));
        } else {
            clean_query_str += ' ';
        }
    }

    std::vector<std::string> query_words = split_query(clean_query_str);
    if (query_words.empty()) return response_json.dump();

    // Score accumulation maps
    std::unordered_map<int, double> doc_scores;
    std::unordered_map<int, int> doc_match_count;
    std::unordered_map<int, std::map<int, std::vector<int>>> doc_positions_map;

    int valid_query_words = 0;

    // 2. Iterate Words
    for (int i = 0; i < query_words.size(); ++i) {
        std::string word = query_words[i];
        int word_id = lexicon_trie_.get_word_index(word);

        if (word_id != -1) {
            valid_query_words++;
            std::string id_str = std::to_string(word_id);
            
            // A. Retrieve Main Barrel Data
            int barrel_id = word_id % 100;
            json& barrel = get_barrel(barrel_id);
            
            // Temporary list to hold entries from both Barrel AND Delta
            std::vector<DeltaEntry> combined_entries;

            // Load from Disk Barrel
            if (barrel.contains(id_str)) {
                auto raw = barrel[id_str];
                for (auto& entry : raw) {
                    if (entry.size() < 3) continue;
                    combined_entries.push_back({
                        entry[0].get<int>(), // doc_id
                        entry[1].get<int>(), // freq
                        entry[2].get<std::vector<int>>() // pos
                    });
                }
            }

            // B. Retrieve Delta Index Data (Dynamic Content)
            if (delta_index_.count(word_id)) {
                const auto& deltas = delta_index_[word_id];
                combined_entries.insert(combined_entries.end(), deltas.begin(), deltas.end());
            }

            // C. Process Combined Entries
            for (auto& entry : combined_entries) {
                int doc_id = entry.doc_id;
                int weighted_freq = entry.frequency;
                std::vector<int> positions = entry.positions;

                // Optimization: Don't fetch doc_len/title_freq yet to save I/O
                // Use default values for initial filtering or fetch if strictly needed.
                // For accuracy, we fetch them now.
                
                int title_freq = get_title_frequency(doc_id, word_id);
                int doc_len = get_document_length(doc_id);

                // Calculate advanced score using Teammate's Scorer
                ScoreComponents scores = ranking_scorer_.calculate_score(
                    weighted_freq,
                    title_freq,
                    positions,
                    doc_id,
                    doc_len,
                    &document_metadata_
                );

                doc_scores[doc_id] += scores.final_score;
                doc_match_count[doc_id]++;
                doc_positions_map[doc_id][i] = positions;
            }
        }
    }

    if (valid_query_words == 0) return response_json.dump();

    // 3. Filter Intersection (AND Logic) and Apply Proximity
    std::vector<SearchResult> final_results;

    for (auto const& [doc_id, count] : doc_match_count) {
        // Strict AND logic: Doc must contain ALL valid query words
        if (count == valid_query_words) {
            double final_score = doc_scores[doc_id];

            // Proximity bonus
            for (int k = 0; k < query_words.size() - 1; ++k) {
                if (doc_positions_map[doc_id].count(k) && doc_positions_map[doc_id].count(k + 1)) {
                    const auto& posA = doc_positions_map[doc_id][k];
                    const auto& posB = doc_positions_map[doc_id][k + 1];

                    bool found_adjacent = false;
                    for (int a : posA) {
                        for (int b : posB) {
                            if (b == a + 1) { found_adjacent = true; break; }
                        }
                        if (found_adjacent) break;
                    }
                    if (found_adjacent) final_score += 100.0; 
                }
            }

            // Get Metadata
            std::string url = doc_url_mapper.get(doc_id);
            int pub_year = document_metadata_.get_publication_year(doc_id);
            int citations = document_metadata_.get_cited_by_count(doc_id);

            final_results.push_back({doc_id, url, final_score, pub_year, citations});
        }
    }

    // 4. Sort
    std::sort(final_results.begin(), final_results.end(), compareResults);

    // 5. Build JSON
    for (const auto& res : final_results) {
        json item;
        item["docId"] = res.doc_id;
        item["score"] = res.score;
        item["url"] = res.url;
        
        if (res.publication_year > 0) item["publication_year"] = res.publication_year;
        if (res.cited_by_count > 0) item["cited_by_count"] = res.cited_by_count;
        
        response_json["results"].push_back(item);
        if (response_json["results"].size() >= 50) break;
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