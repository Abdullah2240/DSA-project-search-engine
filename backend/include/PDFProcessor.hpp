#pragma once
#include <string>
#include <vector>
#include <map>
#include "lexicon.hpp"
#include "forward_index.hpp"
#include "inverted_index.hpp"
#include "DocumentMetadata.hpp"
#include "doc_url_mapper.hpp"

struct ProcessedPDF {
    int doc_id;
    std::string title;
    std::vector<std::string> tokens;
    std::map<int, WordStats> doc_stats;
    bool success;
    std::string error;
};

class PDFProcessor {
public:
    PDFProcessor(
        Lexicon& lexicon,
        ForwardIndexBuilder& forward_builder,
        InvertedIndexBuilder& inverted_builder,
        DocumentMetadata& metadata,
        DocURLMapper& url_mapper
    );

    // Process a single uploaded PDF and add to indices
    bool process_and_index(const std::string& pdf_path, int& assigned_doc_id);

private:
    Lexicon& lexicon_;
    ForwardIndexBuilder& forward_builder_;
    InvertedIndexBuilder& inverted_builder_;
    DocumentMetadata& metadata_;
    DocURLMapper& url_mapper_;

    // Get next available doc_id
    int get_next_doc_id();
    
    // Call Python tokenizer
    ProcessedPDF tokenize_pdf(const std::string& pdf_path, int doc_id);
    
    // Build doc stats from tokens
    std::map<int, WordStats> build_doc_stats(const std::vector<std::string>& tokens);
};
