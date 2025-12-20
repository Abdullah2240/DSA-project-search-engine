#include "PDFProcessor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

PDFProcessor::PDFProcessor(
    Lexicon& lexicon,
    ForwardIndexBuilder& forward_builder,
    InvertedIndexBuilder& inverted_builder,
    DocumentMetadata& metadata,
    DocURLMapper& url_mapper
) : lexicon_(lexicon),
    forward_builder_(forward_builder),
    inverted_builder_(inverted_builder),
    metadata_(metadata),
    url_mapper_(url_mapper) {}

int PDFProcessor::get_next_doc_id() {
    // Read existing test.jsonl to find max doc_id
    std::ifstream f("data/processed/test.jsonl");
    int max_id = -1;
    
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            try {
                auto j = json::parse(line);
                if (j.contains("doc_id")) {
                    int id = j["doc_id"].get<int>();
                    if (id > max_id) max_id = id;
                }
            } catch (...) {}
        }
    }
    
    return max_id + 1;
}

ProcessedPDF PDFProcessor::tokenize_pdf(const std::string& pdf_path, int doc_id) {
    ProcessedPDF result;
    result.doc_id = doc_id;
    result.success = false;

    // Create temp output file for Python script
    std::string temp_json = "data/temp_pdfs/temp_" + std::to_string(doc_id) + ".json";
    
    // Call Python tokenizer (use py -3.14 for Python 3.14)
    std::string python_cmd = "py -3.14 scripts/tokenize_single_pdf.py \"" 
                           + pdf_path + "\" " 
                           + std::to_string(doc_id) + " \"" 
                           + temp_json + "\"";
    
    std::cout << "[PDFProcessor] Tokenizing: " << pdf_path << std::endl;
    int ret = std::system(python_cmd.c_str());
    
    if (ret != 0) {
        result.error = "Python tokenizer failed";
        return result;
    }

    // Read tokenized output
    std::ifstream f(temp_json);
    if (!f.is_open()) {
        result.error = "Could not read tokenized output";
        return result;
    }

    try {
        json j;
        f >> j;
        
        result.title = j.value("title", "Untitled");
        result.tokens = j.value("body_tokens", std::vector<std::string>());
        
        if (result.tokens.empty()) {
            result.error = "No tokens extracted from PDF";
            return result;
        }
        
        result.success = true;
        
        // Clean up temp file
        fs::remove(temp_json);
        
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        return result;
    }

    return result;
}

std::map<int, WordStats> PDFProcessor::build_doc_stats(const std::vector<std::string>& tokens) {
    std::map<int, WordStats> doc_stats;
    int pos = 0;
    
    for (const auto& token : tokens) {
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                     [](unsigned char c) { return std::tolower(c); });
        
        int word_id = lexicon_.get_word_index(lower_token);
        if (word_id != -1) {
            doc_stats[word_id].body_frequency++;
            doc_stats[word_id].body_positions.push_back(pos);
        }
        pos++;
    }
    
    return doc_stats;
}

bool PDFProcessor::process_and_index(const std::string& pdf_path, int& assigned_doc_id) {
    std::cout << "[PDFProcessor] Starting processing: " << pdf_path << std::endl;
    
    // 1. Get next doc_id
    assigned_doc_id = get_next_doc_id();
    std::cout << "[PDFProcessor] Assigned doc_id: " << assigned_doc_id << std::endl;
    
    // 2. Tokenize PDF using Python
    ProcessedPDF processed = tokenize_pdf(pdf_path, assigned_doc_id);
    if (!processed.success) {
        std::cerr << "[PDFProcessor] ERROR: " << processed.error << std::endl;
        return false;
    }
    
    std::cout << "[PDFProcessor] Extracted " << processed.tokens.size() << " tokens" << std::endl;
    
    // 3. Update Lexicon (adds new words if needed)
    lexicon_.update_from_tokens(processed.tokens, "data/processed/lexicon.json");
    std::cout << "[PDFProcessor] Lexicon updated" << std::endl;
    
    // 4. Build document statistics
    auto doc_stats = build_doc_stats(processed.tokens);
    std::cout << "[PDFProcessor] Built stats for " << doc_stats.size() << " unique words" << std::endl;
    
    // 5. Append to Forward Index
    forward_builder_.append_document("data/processed/forward_index.jsonl", assigned_doc_id, doc_stats);
    std::cout << "[PDFProcessor] Forward index updated" << std::endl;
    
    // 6. Update Delta Barrel (Inverted Index)
    inverted_builder_.update_delta_barrel(assigned_doc_id, doc_stats);
    std::cout << "[PDFProcessor] Delta barrel updated" << std::endl;
    
    // 7. Add metadata
    metadata_.add_document(
        assigned_doc_id,
        2024,
        1,
        0,
        processed.title,
        "uploaded://" + fs::path(pdf_path).filename().string()
    );
    std::cout << "[PDFProcessor] Metadata added" << std::endl;
    
    // 8. Add URL mapping
    url_mapper_.add_mapping(assigned_doc_id, "uploaded://" + fs::path(pdf_path).filename().string());
    std::cout << "[PDFProcessor] URL mapping added" << std::endl;
    
    // 9. Append to test.jsonl for persistence
    std::ofstream outfile("data/processed/test.jsonl", std::ios::app);
    if (outfile.is_open()) {
        json doc_json;
        doc_json["doc_id"] = assigned_doc_id;
        doc_json["title"] = processed.title;
        doc_json["body_tokens"] = processed.tokens;
        doc_json["word_count"] = processed.tokens.size();
        doc_json["pdf_path"] = pdf_path;
        doc_json["url"] = "uploaded://" + fs::path(pdf_path).filename().string();
        outfile << doc_json.dump(-1) << "\n";
        std::cout << "[PDFProcessor] Added to test.jsonl" << std::endl;
    }
    
    std::cout << "[PDFProcessor] ✅ Document " << assigned_doc_id << " is now searchable!" << std::endl;
    return true;
}