#include "PDFProcessor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <set>
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

void PDFProcessor::cleanup_temp_files() {
    // Clean up both old and new temp directories
    std::vector<std::string> temp_dirs = {"data/temp_json", "data/temp_pdfs"};
    
    int cleaned = 0;
    int migrated = 0;
    
    for (const auto& temp_dir : temp_dirs) {
        if (!fs::exists(temp_dir)) continue;
        
        try {
            for (const auto& entry : fs::directory_iterator(temp_dir)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                
                std::string filename = entry.path().filename().string();
                
                // Only clean temp_*.json files (not user data)
                if (filename.find("temp_") != 0) continue;
                
                // Migration: Move from temp_pdfs to temp_json
                if (temp_dir == "data/temp_pdfs") {
                    std::string new_path = "data/temp_json/" + filename;
                    
                    // Create temp_json dir if needed
                    if (!fs::exists("data/temp_json")) {
                        fs::create_directories("data/temp_json");
                    }
                    
                    // Only migrate recent files (< 1 hour old)
                    auto ftime = fs::last_write_time(entry.path());
                    auto now = fs::file_time_type::clock::now();
                    auto age = std::chrono::duration_cast<std::chrono::hours>(now - ftime).count();
                    
                    if (age < 1) {
                        try {
                            fs::rename(entry.path(), new_path);
                            migrated++;
                        } catch (...) {
                            fs::remove(entry.path()); // Remove if can't migrate
                            cleaned++;
                        }
                    } else {
                        fs::remove(entry.path()); // Delete old files
                        cleaned++;
                    }
                } else {
                    // Clean old temp files from temp_json (> 1 hour old)
                    auto ftime = fs::last_write_time(entry.path());
                    auto now = fs::file_time_type::clock::now();
                    auto age = std::chrono::duration_cast<std::chrono::hours>(now - ftime).count();
                    
                    if (age > 1) {
                        fs::remove(entry.path());
                        cleaned++;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PDFProcessor] Warning: Error cleaning " << temp_dir << ": " << e.what() << "\n";
        }
    }
    
    if (migrated > 0) {
        std::cout << "[PDFProcessor] Migrated " << migrated << " temp files to temp_json/\n";
    }
    if (cleaned > 0) {
        std::cout << "[PDFProcessor] Cleaned up " << cleaned << " old temp files\n";
    }
}

ProcessedPDF PDFProcessor::tokenize_pdf(const std::string& pdf_path, int doc_id) {
    ProcessedPDF result;
    result.doc_id = doc_id;
    result.success = false;

    // Create dedicated temp directory for JSON files (separate from PDFs)
    std::string temp_dir = "data/temp_json";
    if (!fs::exists(temp_dir)) {
        fs::create_directories(temp_dir);
    }
    
    // Create temp output file for Python script
    std::string temp_json = temp_dir + "/temp_" + std::to_string(doc_id) + ".json";
    
    // Call Python tokenizer (use venv if available, fallback to system python)
    std::string python_exe;
    #ifdef _WIN32
        if (fs::exists("venv/Scripts/python.exe")) {
            python_exe = "venv\\Scripts\\python.exe";
        } else {
            python_exe = "py -3.14";
        }
    #else
        if (fs::exists("venv/bin/python")) {
            python_exe = "venv/bin/python";
        } else {
            python_exe = "python3";
        }
    #endif
    
    std::string python_cmd = python_exe + " scripts/tokenize_single_pdf.py \"" 
                           + pdf_path + "\" " 
                           + std::to_string(doc_id) + " \"" 
                           + temp_json + "\" 2>&1";
    
    std::cout << "[PDFProcessor] Tokenizing (max 5000 tokens, 20 pages)..." << std::endl;
    int ret = std::system(python_cmd.c_str());
    
    if (ret != 0) {
        result.error = "Python tokenizer failed";
        fs::remove(temp_json); // Clean up even on failure
        return result;
    }

    // Read tokenized output
    std::ifstream f(temp_json);
    if (!f.is_open()) {
        result.error = "Could not read tokenized output";
        fs::remove(temp_json); // Clean up
        return result;
    }

    try {
        json j;
        f >> j;
        f.close(); // Close before deleting
        
        result.title = j.value("title", "Untitled Document");
        result.tokens = j.value("body_tokens", std::vector<std::string>());
        
        std::cout << "[PDFProcessor] Title: " << result.title.substr(0, 50) << "..." << std::endl;
        
        if (result.tokens.empty()) {
            result.error = "No tokens extracted from PDF";
            fs::remove(temp_json); // Clean up
            return result;
        }
        
        result.success = true;
        
        // Clean up temp file
        fs::remove(temp_json);
        
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        fs::remove(temp_json); // Clean up on error
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

void PDFProcessor::check_and_merge_delta() {
    // Check delta barrel size
    std::ifstream f("data/processed/barrels/inverted_delta.json");
    if (!f.good()) return;
    
    try {
        json delta;
        f >> delta;
        
        // Count unique documents in delta barrel
        std::set<int> unique_docs;
        for (auto& word_entry : delta.items()) {
            if (word_entry.value().is_array()) {
                for (auto& doc_entry : word_entry.value()) {
                    if (doc_entry.contains("doc_id")) {
                        unique_docs.insert(doc_entry["doc_id"].get<int>());
                    }
                }
            }
        }
        
        int total_docs = unique_docs.size();
        
        // OPTIMIZATION: Higher threshold to reduce merge frequency  
        if (total_docs >= 100) {
            std::cout << "[PDFProcessor] ⚠️  Delta barrel has " << total_docs 
                      << " documents. Consider merging (auto-merge at 100+)" << std::endl;
            
            // Optionally enable auto-merge here if needed
            // inverted_builder_.merge_delta_to_main("data/processed/barrels");
            // std::cout << "[PDFProcessor] ✅ Delta barrel merged and cleared!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[PDFProcessor] Warning: Could not check delta size: " << e.what() << "\n";
    }
}

bool PDFProcessor::process_and_index(const std::string& pdf_path, int& assigned_doc_id) {
    std::cout << "[PDFProcessor] ⏱️  Starting fast processing..." << std::endl;
    
    // 1. Get next doc_id
    assigned_doc_id = get_next_doc_id();
    std::cout << "[PDFProcessor] Assigned doc_id: " << assigned_doc_id << std::endl;
    
    // 2. Tokenize PDF using Python (LIMITED to 5000 tokens, 20 pages)
    ProcessedPDF processed = tokenize_pdf(pdf_path, assigned_doc_id);
    if (!processed.success) {
        std::cerr << "[PDFProcessor] ERROR: " << processed.error << std::endl;
        return false;
    }
    
    std::cout << "[PDFProcessor] ✓ Extracted " << processed.tokens.size() << " tokens" << std::endl;
    std::cout << "[PDFProcessor] ✓ Title: \"" << processed.title << "\"" << std::endl;
    
    // 3. Update Lexicon (adds new words if needed)
    lexicon_.update_from_tokens(processed.tokens, "data/processed/lexicon.json");
    std::cout << "[PDFProcessor] ✓ Lexicon updated" << std::endl;
    
    // 4. Build document statistics
    auto doc_stats = build_doc_stats(processed.tokens);
    std::cout << "[PDFProcessor] ✓ Stats built (" << doc_stats.size() << " unique words)" << std::endl;
    
    // 5. Append to Forward Index
    forward_builder_.append_document("data/processed/forward_index.jsonl", assigned_doc_id, doc_stats);
    std::cout << "[PDFProcessor] ✓ Forward index updated" << std::endl;
    
    // 6. Update Delta Barrel (Inverted Index)
    inverted_builder_.update_delta_barrel(assigned_doc_id, doc_stats);
    std::cout << "[PDFProcessor] ✓ Delta barrel updated" << std::endl;
    
    // 7. Add metadata
    metadata_.add_document(
        assigned_doc_id,
        2024,
        1,
        0,
        processed.title,
        "uploaded://" + fs::path(pdf_path).filename().string()
    );
    std::cout << "[PDFProcessor] ✓ Metadata added" << std::endl;
    
    // 8. Add URL mapping and save to disk for persistence
    std::string url = "uploaded://" + fs::path(pdf_path).filename().string();
    url_mapper_.add_mapping(assigned_doc_id, url);
    url_mapper_.save("data/processed/docid_to_url.json");
    std::cout << "[PDFProcessor] ✓ URL mapping added" << std::endl;
    
    // 8.1. Save metadata to disk for persistence
    metadata_.save("data/processed/document_metadata.json");
    std::cout << "[PDFProcessor] ✓ Metadata saved" << std::endl;
    
    // 8.2. Copy PDF with doc_id as filename to downloads directory (FIXED with verification)
    try {
        std::string download_dir = "data/downloads";
        fs::create_directories(download_dir);
        std::string download_path = download_dir + "/" + std::to_string(assigned_doc_id) + ".pdf";
        
        // Ensure source file exists and is accessible
        if (!fs::exists(pdf_path)) {
            std::cerr << "[PDFProcessor] Warning: Source PDF not found: " << pdf_path << std::endl;
        } else if (pdf_path != download_path) {
            // Use copy_file with overwrite option and error handling
            std::error_code ec;
            fs::copy_file(pdf_path, download_path, fs::copy_options::overwrite_existing, ec);
            
            if (ec) {
                std::cerr << "[PDFProcessor] Warning: Could not copy PDF: " << ec.message() << std::endl;
            } else {
                std::cout << "[PDFProcessor] ✓ PDF copied to downloads/" << assigned_doc_id << ".pdf" << std::endl;
                
                // Verify copied file
                if (fs::exists(download_path) && fs::file_size(download_path) > 0) {
                    std::cout << "[PDFProcessor] ✓ Download file verified (" 
                              << fs::file_size(download_path) / 1024 << " KB)" << std::endl;
                } else {
                    std::cerr << "[PDFProcessor] Warning: Download file verification failed!" << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[PDFProcessor] Warning: Exception copying PDF: " << e.what() << std::endl;
    }
    
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
        std::cout << "[PDFProcessor] ✓ Added to test.jsonl" << std::endl;
    }
    
    // 10. Check if delta barrel needs merging (less frequent)
    check_and_merge_delta();
    
    std::cout << "[PDFProcessor] ✅ Document " << assigned_doc_id << " is SEARCHABLE!" << std::endl;
    return true;
}