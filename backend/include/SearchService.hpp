#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <map>
#include "LexiconWithTrie.hpp"
#include "doc_url_mapper.hpp"
#include "DocumentMetadata.hpp"
#include "RankingScorer.hpp"
#include "SemanticScorer.hpp"

using json = nlohmann::json;

// Struct for Delta Index entries
struct DeltaEntry {
    int doc_id;
    int frequency;
    std::vector<int> positions;
};

// In-memory document stats for fast lookup
struct DocStats {
    int doc_length;
    std::unordered_map<int, int> title_frequencies; // word_id -> title_freq
};

class SearchService {
public:
    SearchService(); 

    // Returns a raw JSON string of results
    std::string search(std::string query);
    
    // Returns autocomplete suggestions as JSON string
    std::string autocomplete(const std::string& prefix, int limit = 10);
    
    // Reload indices after dynamic uploads
    void reload_delta_index();
    void reload_metadata();

private:
    LexiconWithTrie lexicon_trie_;
    DocURLMapper doc_url_mapper;
    DocumentMetadata document_metadata_;
    RankingScorer ranking_scorer_;
    std::unordered_map<int, json> barrel_cache_;
    
    // In-memory document statistics for O(1) lookup
    std::unordered_map<int, DocStats> doc_stats_cache_;

    // DYNAMIC SEARCH: Holds the small "new" index in RAM
    std::unordered_map<int, std::vector<DeltaEntry>> delta_index_;
    
    // Semantic search components
    bool semantic_search_enabled_;
    SemanticScorer semantic_scorer_;

    // Helpers
    json& get_barrel(int barrel_id);
    
    // Load all document stats into memory
    void load_document_stats();
    
    void load_delta_index();
    
    // Fast memory-based lookups (replacing disk I/O)
    int get_title_frequency(int doc_id, int word_id);
    int get_document_length(int doc_id);
};