#include "httplib.h" 
#include "SearchService.hpp"
#include "PDFProcessor.hpp"
#include "lexicon.hpp"
#include "forward_index.hpp"
#include "inverted_index.hpp"
#include "DocumentMetadata.hpp"
#include "doc_url_mapper.hpp"
#include <iostream>
#include <fstream>

int main() {
    // Clean up old temp JSON files from previous runs
    PDFProcessor::cleanup_temp_files();
    
    SearchService engine;
    
    // Initialize components for PDF processing
    Lexicon lexicon;
    lexicon.load_from_json("data/processed/lexicon.json");
    
    ForwardIndexBuilder forward_builder;
    forward_builder.load_lexicon("data/processed/lexicon.json");
    
    InvertedIndexBuilder inverted_builder(100);
    
    DocumentMetadata metadata;
    metadata.load("data/processed/document_metadata.json");
    
    DocURLMapper url_mapper;
    url_mapper.load("data/processed/docid_to_url.json");
    
    PDFProcessor pdf_processor(lexicon, forward_builder, inverted_builder, metadata, url_mapper);
    
    httplib::Server svr;

    // CORS middleware - Add CORS headers to all responses
    svr.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // Handle CORS preflight requests
    svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
    });

    // Serve static files from the static directory
    svr.set_mount_point("/", "./static");
    
    // Set default MIME types
    svr.set_file_extension_and_mimetype_mapping("js", "application/javascript");
    svr.set_file_extension_and_mimetype_mapping("mjs", "application/javascript");
    svr.set_file_extension_and_mimetype_mapping("css", "text/css");
    svr.set_file_extension_and_mimetype_mapping("html", "text/html");
    svr.set_file_extension_and_mimetype_mapping("svg", "image/svg+xml");
    svr.set_file_extension_and_mimetype_mapping("ico", "image/x-icon");

    // Root endpoint - Redirect to index.html or serve API status
    svr.Get("/api", [](const httplib::Request& req, httplib::Response& res) {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>DSA Search Engine API</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; background: #0a0e27; color: #fff; }
        h1 { color: #60a5fa; }
        .endpoint { background: rgba(255,255,255,0.05); padding: 15px; margin: 10px 0; border-radius: 8px; border-left: 4px solid #60a5fa; }
        .method { color: #34d399; font-weight: bold; }
        code { background: rgba(0,0,0,0.3); padding: 2px 6px; border-radius: 4px; }
        a { color: #60a5fa; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <h1>🚀 DSA Search Engine API</h1>
    <p>Backend server is running successfully!</p>
    <h2>Available Endpoints:</h2>
    <div class="endpoint">
        <span class="method">GET</span> <code>/search?q=&lt;query&gt;</code><br>
        Search for documents matching the query<br>
        <a href="/search?q=computer" target="_blank">Try example: /search?q=computer</a>
    </div>
    <div class="endpoint">
        <span class="method">GET</span> <code>/autocomplete?q=&lt;prefix&gt;&amp;limit=&lt;num&gt;</code><br>
        Get autocomplete suggestions<br>
        <a href="/autocomplete?q=comp&limit=5" target="_blank">Try example: /autocomplete?q=comp&limit=5</a>
    </div>
    <div class="endpoint">
        <span class="method">POST</span> <code>/upload</code><br>
        Upload PDF files (multipart/form-data)
    </div>
    <hr>
    <p>Frontend is served at: <a href="/" target="_blank">http://localhost:8080</a></p>
</body>
</html>
        )";
        res.set_content(html, "text/html");
    });

    // Define Route: /search?q=...
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("q")) {
            std::string query = req.get_param_value("q");
            std::string json_output = engine.search(query);
            res.set_content(json_output, "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\": \"Missing 'q' parameter\"}", "application/json");
        }
    });

    // Define Route: /autocomplete?q=...&limit=10
    svr.Get("/autocomplete", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("q")) {
            std::string prefix = req.get_param_value("q");
            int limit = 10;
            if (req.has_param("limit")) {
                try {
                    limit = std::stoi(req.get_param_value("limit"));
                    if (limit < 1) limit = 1;
                    if (limit > 50) limit = 50;
                } catch (...) {
                    limit = 10;
                }
            }
            std::string json_output = engine.autocomplete(prefix, limit);
            res.set_content(json_output, "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\": \"Missing 'q' parameter\"}", "application/json");
        }
    });

    // Define Route: /download/:doc_id - Download uploaded PDFs
    svr.Get(R"(/download/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int doc_id = std::stoi(req.matches[1]);
            std::string pdf_path = "data/temp_pdfs/" + std::to_string(doc_id) + ".pdf";
            
            // Check if file exists
            std::ifstream file(pdf_path, std::ios::binary);
            if (!file.is_open()) {
                res.status = 404;
                res.set_content("{\"error\": \"PDF not found\"}", "application/json");
                return;
            }
            
            // Read file content
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();
            
            // Serve PDF
            res.set_header("Content-Type", "application/pdf");
            res.set_header("Content-Disposition", "attachment; filename=\"document_" + std::to_string(doc_id) + ".pdf\"");
            res.set_content(content, "application/pdf");
            
            std::cout << "[Download] Served PDF for doc_id " << doc_id << std::endl;
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid doc_id\"}", "application/json");
        }
    });

    // Define Route: /upload - Handle PDF uploads (saves to temp_pdfs for later processing)
    svr.Post("/upload", [&engine, &pdf_processor](const httplib::Request& req, httplib::Response& res) {
        try {
            if (req.has_header("Content-Type")) {
                std::string content_type = req.get_header_value("Content-Type");
                
                if (content_type.find("multipart/form-data") != std::string::npos) {
                    int uploaded_count = 0;
                    int failed_count = 0;
                    std::vector<int> new_doc_ids;
                    
                    // Process uploaded files from multipart form
                    if (req.form.has_file("files")) {
                        auto files = req.form.get_files("files");
                        
                        for (const auto& file : files) {
                            std::string filename = file.filename;
                            std::string content = file.content;
                            
                            // Save to temp directory
                            std::string temp_path = "data/temp_pdfs/" + filename;
                            std::ofstream out(temp_path, std::ios::binary);
                            if (out.is_open()) {
                                out.write(content.data(), content.size());
                                out.close();
                                std::cout << "[Upload] Saved: " << filename << std::endl;
                                
                                // Process and index immediately
                                int doc_id = -1;
                                if (pdf_processor.process_and_index(temp_path, doc_id)) {
                                    uploaded_count++;
                                    new_doc_ids.push_back(doc_id);
                                    std::cout << "[Upload] ✅ Indexed doc_id " << doc_id << std::endl;
                                } else {
                                    failed_count++;
                                    std::cerr << "[Upload] ❌ Failed to index: " << filename << std::endl;
                                }
                            } else {
                                failed_count++;
                                std::cerr << "[Upload] Failed to save: " << filename << std::endl;
                            }
                        }
                    }
                    
                    // Reload indices in SearchService
                    if (uploaded_count > 0) {
                        std::cout << "[Upload] Reloading search engine indices..." << std::endl;
                        engine.reload_delta_index();
                        engine.reload_metadata();
                    }
                    
                    std::string response = "{\"success\": true, \"uploadedCount\": " + 
                                          std::to_string(uploaded_count) + 
                                          ", \"failedCount\": " + std::to_string(failed_count) + 
                                          ", \"newDocIds\": [";
                    for (size_t i = 0; i < new_doc_ids.size(); ++i) {
                        response += std::to_string(new_doc_ids[i]);
                        if (i < new_doc_ids.size() - 1) response += ", ";
                    }
                    response += "], \"message\": \"";
                    if (uploaded_count > 0) {
                        response += "PDFs uploaded and indexed successfully! They are now searchable.\"}";
                    } else {
                        response += "Upload completed but no PDFs were processed.\"}";
                    }
                    res.set_content(response, "application/json");
                } else {
                    res.status = 400;
                    res.set_content("{\"error\": \"Invalid content type. Expected multipart/form-data\"}", "application/json");
                }
            } else {
                res.status = 400;
                res.set_content("{\"error\": \"No files provided or invalid request\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\": \"Upload failed: " + std::string(e.what()) + "\"}", "application/json");
        }
    });

    std::cout << "======================================" << std::endl;
    std::cout << "Server starting on port 8080..." << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Available endpoints:" << std::endl;
    std::cout << "  - GET  /              (Frontend App)" << std::endl;
    std::cout << "  - GET  /search?q=<query>" << std::endl;
    std::cout << "  - GET  /autocomplete?q=<prefix>&limit=<num>" << std::endl;
    std::cout << "  - POST /upload (multipart/form-data)" << std::endl;
    std::cout << "  - GET  /download/<doc_id>" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Open: http://localhost:8080" << std::endl;
    std::cout << "======================================" << std::endl;

    if (!svr.listen("127.0.0.1", 8080)) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }

    return 0;
    
}