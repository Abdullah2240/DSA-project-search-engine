#pragma once
// DocumentMetadata.hpp
// Stores and manages document metadata for ranking purposes
// Includes publication dates, citation counts, keywords, etc.

#include <string>
#include <unordered_map>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

struct DocMetadata {
    int doc_id;
    int publication_year;
    int publication_month;
    int cited_by_count;     // From page_rank
    std::string title;
    std::string url;
    std::vector<std::string> keywords;  // Optional keywords if available
    
    DocMetadata() : doc_id(-1), publication_year(0), publication_month(0), cited_by_count(0) {}
};

class DocumentMetadata {
public:
    DocumentMetadata();
    
    // Load metadata from JSON file
    bool load(const std::string& metadata_path);
    
    // Get metadata for a document
    const DocMetadata* get_metadata(int doc_id) const;
    
    // Check if metadata exists for a document
    bool has_metadata(int doc_id) const;
    
    // Get publication year (returns 0 if not found)
    int get_publication_year(int doc_id) const;
    
    // Get citation count (returns 0 if not found)
    int get_cited_by_count(int doc_id) const;
    
    // Get total number of documents with metadata
    size_t size() const { return metadata_.size(); }
    
    // Get document count (alias for size, used for getting next doc_id)
    int get_document_count() const { return static_cast<int>(metadata_.size()); }
    
    // Add new document metadata (for dynamic uploads)
    void add_document(int doc_id, int pub_year, int pub_month, int citations, 
                     const std::string& title, const std::string& url);
    
    // Save metadata to JSON file
    bool save(const std::string& metadata_path) const;

private:
    std::unordered_map<int, DocMetadata> metadata_;
};





