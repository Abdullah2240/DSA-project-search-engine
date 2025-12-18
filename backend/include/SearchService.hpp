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
#include "json.hpp"

using json = nlohmann::json;

// Struct for Delta Index entries
struct DeltaEntry {
    int doc_id;
    int frequency;
    std::vector<int> positions;
};

class SearchService {
public:
    SearchService(); 

    // Returns a raw JSON string of results
    std::string search(std::string query);
    
    // Returns autocomplete suggestions as JSON string
    std::string autocomplete(const std::string& prefix, int limit = 10);

private:
    LexiconWithTrie lexicon_trie_;
    DocURLMapper doc_url_mapper;
    DocumentMetadata document_metadata_;
    RankingScorer ranking_scorer_;
    std::unordered_map<int, json> barrel_cache_;
    // RAM OPTIMIZATION: Maps DocID -> Byte Position in forward_index.jsonl
    std::unordered_map<int, std::streampos> doc_offsets_; 

    // DYNAMIC SEARCH: Holds the small "new" index in RAM
    std::unordered_map<int, std::vector<DeltaEntry>> delta_index_;

    // Helpers
    json& get_barrel(int barrel_id);
    
    // Replaces load_forward_index
    void build_offset_map();
    void load_delta_index();
    json get_document_from_disk(int doc_id);

    // Updated getters to use disk seek
    int get_title_frequency(int doc_id, int word_id);
    int get_document_length(int doc_id);
};