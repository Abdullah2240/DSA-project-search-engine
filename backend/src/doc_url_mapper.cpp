#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include "doc_url_mapper.hpp"
#include "json.hpp"

using json = nlohmann::json;

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

bool DocURLMapper::save(const std::string& filename) const {
    try {
        json j = json::object();
        
        for (const auto& [doc_id, url] : urls) {
            j[std::to_string(doc_id)] = url;
        }
        
        // Write to temporary file first
        std::string temp_path = filename + ".tmp";
        std::ofstream out(temp_path, std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "[DocURLMapper] Error: Could not open file for writing: " << temp_path << std::endl;
            return false;
        }
        
        out << j.dump(2);
        out.flush();
        
        if (!out.good()) {
            std::cerr << "[DocURLMapper] Error: Write failed for: " << temp_path << std::endl;
            out.close();
            return false;
        }
        
        out.close();
        
        // Atomic rename
        if (std::rename(temp_path.c_str(), filename.c_str()) != 0) {
            std::cerr << "[DocURLMapper] Error: Could not rename temp file" << std::endl;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[DocURLMapper] Error saving URL mappings: " << e.what() << std::endl;
        return false;
    }
}
