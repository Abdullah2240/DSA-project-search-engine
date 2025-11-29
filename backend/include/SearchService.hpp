#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "lexicon.hpp"
#include "json.hpp"

using json = nlohmann::json;

class SearchService {
public:
    SearchService(); 

    // Returns a raw JSON string of results
    std::string search(std::string query);

private:
    Lexicon lexicon_;
    std::unordered_map<int, json> barrel_cache_;
    json& get_barrel(int barrel_id);
};