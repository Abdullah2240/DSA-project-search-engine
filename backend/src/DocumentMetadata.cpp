#include "DocumentMetadata.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

DocumentMetadata::DocumentMetadata() {}

bool DocumentMetadata::load(const std::string& metadata_path) {
    std::ifstream in(metadata_path);
    if (!in.is_open()) {
        std::cerr << "[Metadata] Warning: Could not open metadata file: " << metadata_path << std::endl;
        return false;
    }
    
    try {
        json j;
        in >> j;
        
        metadata_.clear();
        
        // Expected format: {doc_id: {year, month, cited_count, title, url, keywords}}
        for (auto& [key, value] : j.items()) {
            int doc_id = std::stoi(key);
            DocMetadata meta;
            meta.doc_id = doc_id;
            
            if (value.contains("publication_year")) {
                meta.publication_year = value["publication_year"].get<int>();
            }
            if (value.contains("publication_month")) {
                meta.publication_month = value["publication_month"].get<int>();
            }
            if (value.contains("cited_by_count")) {
                meta.cited_by_count = value["cited_by_count"].get<int>();
            }
            if (value.contains("title")) {
                meta.title = value["title"].get<std::string>();
            }
            if (value.contains("url")) {
                meta.url = value["url"].get<std::string>();
            }
            if (value.contains("keywords") && value["keywords"].is_array()) {
                meta.keywords = value["keywords"].get<std::vector<std::string>>();
            }
            
            metadata_[doc_id] = meta;
        }
        
        std::cout << "[Metadata] Loaded metadata for " << metadata_.size() << " documents" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[Metadata] Error parsing metadata file: " << e.what() << std::endl;
        return false;
    }
}

const DocMetadata* DocumentMetadata::get_metadata(int doc_id) const {
    auto it = metadata_.find(doc_id);
    if (it == metadata_.end()) {
        return nullptr;
    }
    return &(it->second);
}

bool DocumentMetadata::has_metadata(int doc_id) const {
    return metadata_.find(doc_id) != metadata_.end();
}

int DocumentMetadata::get_publication_year(int doc_id) const {
    auto it = metadata_.find(doc_id);
    if (it == metadata_.end()) {
        return 0;
    }
    return it->second.publication_year;
}

int DocumentMetadata::get_cited_by_count(int doc_id) const {
    auto it = metadata_.find(doc_id);
    if (it == metadata_.end()) {
        return 0;
    }
    return it->second.cited_by_count;
}

void DocumentMetadata::add_document(int doc_id, int pub_year, int pub_month, int citations, 
                                    const std::string& title, const std::string& url) {
    DocMetadata meta;
    meta.doc_id = doc_id;
    meta.publication_year = pub_year;
    meta.publication_month = pub_month;
    meta.cited_by_count = citations;
    meta.title = title;
    meta.url = url;
    
    metadata_[doc_id] = meta;
    std::cout << "[Metadata] Added metadata for doc " << doc_id << std::endl;
}





