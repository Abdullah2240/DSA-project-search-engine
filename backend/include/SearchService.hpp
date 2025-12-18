#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "LexiconWithTrie.hpp"
#include "doc_url_mapper.hpp"
#include "DocumentMetadata.hpp"
#include "RankingScorer.hpp"
#include "json.hpp"

using json = nlohmann::json;

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
    json forward_index_cache_;  // Cache for forward index to get title_frequency
    
    json& get_barrel(int barrel_id);
    void load_forward_index();
    int get_title_frequency(int doc_id, int word_id) const;
    int get_document_length(int doc_id) const;
};