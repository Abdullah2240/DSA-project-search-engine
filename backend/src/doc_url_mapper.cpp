#include <string>
#include <unordered_map>
#include <fstream>
#include "doc_url_mapper.hpp"
#include "json.hpp"

bool DocURLMapper::load(const std::string& filename) {
    try {
        std::ifstream in(filename);
        if (!in.is_open()) return false;

        nlohmann::json j;
        in >> j;

        for (auto& [key, value] : j.items()) {
            int id = std::stoi(key);
            urls[id] = value.get<std::string>();
        }
        return true;
    } catch (...) {
        return false;
    }
}

const std::string& DocURLMapper::get(int doc_id) const {
    static const std::string empty = "";
    auto it = urls.find(doc_id);
    return (it != urls.end()) ? it->second : empty;
}

void DocURLMapper::add_mapping(int doc_id, const std::string& url) {
    urls[doc_id] = url;
}
