#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "LexiconWithTrie.hpp"
#include "doc_url_mapper.hpp"
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
    std::unordered_map<int, json> barrel_cache_;
    json& get_barrel(int barrel_id);
};