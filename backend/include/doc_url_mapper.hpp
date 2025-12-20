#pragma once
#include <string>
#include <unordered_map>
#include "json.hpp"

class DocURLMapper {
public:
    // Load mappings from JSON file
    bool load(const std::string& filename);

    // Get URL for a doc_id; returns empty string if not found
    const std::string& get(int doc_id) const;
    
    // Add new mapping (for dynamic uploads)
    void add_mapping(int doc_id, const std::string& url);

private:
    std::unordered_map<int, std::string> urls;
};
